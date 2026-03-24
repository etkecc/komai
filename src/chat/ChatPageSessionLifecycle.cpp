// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chat/ChatPage.h"

#include <QCoreApplication>
#include <QMessageBox>
#include <QMetaObject>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <memory>
#include <utility>

#include <nlohmann/json.hpp>

#include <mtx/responses.hpp>

#include "cache/Cache.h"
#include "encryption/Olm.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

namespace {
constexpr auto LOGOUT_REQUEST_TIMEOUT = std::chrono::seconds(10);
}

void
ChatPage::getBackupVersion()
{
    if (MainWindow::instance() && MainWindow::instance()->matrixBackendHandleId() != 0) {
        nhlog::crypto()->info(
          "Skipping legacy online key backup lookup because matrix-sdk runtime owns sync");
        return;
    }

    if (!UserSettings::instance()->encryptionBackupOnlineEnabled()) {
        nhlog::crypto()->info("Online key backup disabled.");
        return;
    }

    http::client()->backup_version(
      [this](const mtx::responses::backup::BackupVersion &res, mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->warn("Failed to retrieve backup version");
              if (err->status_code == 404)
                  cache::deleteBackupVersion();
              return;
          }

          // switch to UI thread for secrets stuff
          QTimer::singleShot(0, this, [this, res] {
              auto auth_data = nlohmann::json::parse(res.auth_data);

              if (res.algorithm == "m.megolm_backup.v1.curve25519-aes-sha2") {
                  auto key = cache::secret(mtx::secret_storage::secrets::megolm_backup_v1);
                  if (!key) {
                      nhlog::crypto()->info("No key for online key backup.");
                      cache::deleteBackupVersion();
                      return;
                  }

                  using namespace mtx::crypto;
                  auto pubkey = CURVE25519_public_key_from_private(to_binary_buf(base642bin(*key)));

                  const auto backupPublicKey = auth_data.value("public_key", std::string{});
                  bool backupKeyMatches      = (backupPublicKey == pubkey);

                  if (!backupKeyMatches) {
                      // Compatibility for backups created with mtxclient v0.10.1, where
                      // auth_data.public_key was double-encoded in create_online_key_backup:
                      // https://github.com/Nheko-Reborn/mtxclient/blob/v0.10.1/lib/crypto/client.cpp#L267
                      // Upstream fix PR: https://github.com/Nheko-Reborn/mtxclient/pull/113
                      // Accept one decode layer so users can still recover existing backups.
                      try {
                          if (base642bin(backupPublicKey) == pubkey) {
                              backupKeyMatches = true;
                              nhlog::crypto()->warn("Online backup metadata public_key appears "
                                                    "double-encoded; accepting decoded value.");
                          }
                      } catch (...) {
                          // Keep original mismatch behavior below.
                      }
                  }

                  if (!backupKeyMatches) {
                      nhlog::crypto()->info("Our backup key {} does not match the one "
                                            "used in the online backup {}",
                                            pubkey,
                                            backupPublicKey);
                      cache::deleteBackupVersion();
                      return;
                  }

                  auto oldBackupVersion = cache::backupVersion();

                  nhlog::crypto()->info("Using online key backup.");
                  OnlineBackupVersion data{};
                  data.algorithm = res.algorithm;
                  data.version   = res.version;
                  cache::saveBackupVersion(data);

                  // Download all backed-up keys so that encrypted messages
                  // can be decrypted without waiting for individual per-session
                  // lookups.  On fresh login this runs after SSSS unlock makes
                  // the backup key available; on relaunch it covers sessions
                  // whose individual lookup_keybackup() raced with the backup
                  // version not being saved yet on a prior run.
                  // importSessionKeys() skips sessions we already have, so this
                  // is safe to call unconditionally.
                  olm::download_full_keybackup();
              } else {
                  nhlog::crypto()->info("Unsupported key backup algorithm: {}", res.algorithm);
                  cache::deleteBackupVersion();
              }
          });
      });
}

void
ChatPage::prepareShutdown()
{
    shuttingDown_ = true;
    connectivityTimer_.stop();
    disconnect();
}

void
ChatPage::initiateLogout()
{
    performLogout(LogoutPolicy::BestEffortServerFirst, LogoutRoute::ViaClosingSignal);
}

void
ChatPage::performLogout(LogoutPolicy policy, LogoutRoute route, const QString &loginMessage)
{
    shuttingDown_ = true;

    if (policy == LogoutPolicy::LocalOnly) {
        finalizeLogout(route, loginMessage);
        return;
    }

    auto completed          = std::make_shared<std::atomic_bool>(false);
    auto finalizeOnUiThread = [this, route, loginMessage]() {
        if (QThread::currentThread() == thread()) {
            finalizeLogout(route, loginMessage);
            return;
        }

        QMetaObject::invokeMethod(
          this,
          [this, route, loginMessage] { finalizeLogout(route, loginMessage); },
          Qt::QueuedConnection);
    };

    QTimer::singleShot(LOGOUT_REQUEST_TIMEOUT, this, [this, completed, finalizeOnUiThread] {
        if (completed->exchange(true))
            return;

        nhlog::net()->warn(
          "logout request timed out after {}ms; proceeding with local cleanup",
          std::chrono::duration_cast<std::chrono::milliseconds>(LOGOUT_REQUEST_TIMEOUT).count());
        finalizeOnUiThread();
    });

    http::client()->logout([this, completed, finalizeOnUiThread](const mtx::responses::Logout &,
                                                                 mtx::http::RequestErr err) {
        if (completed->exchange(true))
            return;

        if (err) {
            nhlog::net()->warn("failed to logout on server; proceeding with local cleanup: {}",
                               err);
        }

        finalizeOnUiThread();
    });
}

void
ChatPage::finalizeLogout(LogoutRoute route, const QString &loginMessage)
{
    resetUI();
    deleteConfigs();
    emit loggedOut();
    connectivityTimer_.stop();

    if (route == LogoutRoute::ViaClosingSignal)
        emit closing();
    else
        emit showLoginPage(loginMessage);
}

void
ChatPage::decryptDownloadedSecrets(mtx::secret_storage::AesHmacSha2KeyDescription keyDesc,
                                   const SecretsToDecrypt &secrets)
{
    pendingSecretsUnlockRequest_ = PendingSecretsUnlockRequest{std::move(keyDesc), secrets};
    nhlog::crypto()->info("Prompting user to unlock encryption secrets from key backup.");
    emit promptUnlockKeyBackup();
}

void
ChatPage::submitSecretUnlockInput(const QString &text)
{
    if (!pendingSecretsUnlockRequest_) {
        nhlog::crypto()->warn(
          "Received unlock input, but no pending secrets unlock request exists.");
        return;
    }

    auto request = std::move(*pendingSecretsUnlockRequest_);
    pendingSecretsUnlockRequest_.reset();

    if (text.isEmpty()) {
        nhlog::crypto()->info("Secrets unlock cancelled: empty input.");
        return;
    }

    processDownloadedSecretsUnlockInput(std::move(request.keyDesc), request.secrets, text);
}

void
ChatPage::cancelSecretUnlockInput()
{
    if (!pendingSecretsUnlockRequest_)
        return;

    nhlog::crypto()->info("Secrets unlock prompt dismissed by user.");
    pendingSecretsUnlockRequest_.reset();
}

void
ChatPage::processDownloadedSecretsUnlockInput(
  mtx::secret_storage::AesHmacSha2KeyDescription keyDesc,
  const SecretsToDecrypt &secrets,
  const QString &text)
{
    nhlog::crypto()->info("Processing security key / passphrase input for secrets unlock.");

    // strip space chars from a recovery key. It can't contain those, but some clients insert them
    // to make them easier to read.
    QString stripped = text;
    stripped.remove(' ');
    stripped.remove('\n');
    stripped.remove('\t');

    auto decryptionKey = mtx::crypto::key_from_recoverykey(stripped.toStdString(), keyDesc);
    if (!decryptionKey && keyDesc.passphrase) {
        try {
            decryptionKey = mtx::crypto::key_from_passphrase(text.toStdString(), keyDesc);
        } catch (std::exception &e) {
            nhlog::crypto()->error("Failed to derive secret key from passphrase: {}", e.what());
        }
    }

    if (!decryptionKey) {
        QMessageBox::information(
          nullptr,
          QCoreApplication::translate("CrossSigningSecrets", "Unlock failed"),
          keyDesc.passphrase
            ? QCoreApplication::translate(
                "CrossSigningSecrets",
                "Failed to unlock your key backup with the provided security key or passphrase")
            : QCoreApplication::translate(
                "CrossSigningSecrets",
                "Failed to unlock your key backup with the provided security key"));
        return;
    }

    auto deviceKeys = cache::userKeys(http::client()->user_id().to_string());
    mtx::requests::KeySignaturesUpload req;

    for (const auto &[secretName, encryptedSecret] : secrets) {
        auto decrypted = mtx::crypto::decrypt(encryptedSecret, *decryptionKey, secretName);
        nhlog::crypto()->debug("Secret {} decrypted: {}", secretName, !decrypted.empty());

        if (!decrypted.empty()) {
            cache::storeSecret(secretName, decrypted);

            if (deviceKeys && deviceKeys->device_keys.count(http::client()->device_id()) &&
                secretName == mtx::secret_storage::secrets::cross_signing_self_signing) {
                auto myKey = deviceKeys->device_keys.at(http::client()->device_id());
                if (myKey.user_id == http::client()->user_id().to_string() &&
                    myKey.device_id == http::client()->device_id() &&
                    myKey.keys["ed25519:" + http::client()->device_id()] ==
                      olm::client()->identity_keys().ed25519 &&
                    myKey.keys["curve25519:" + http::client()->device_id()] ==
                      olm::client()->identity_keys().curve25519) {
                    nlohmann::json j = myKey;
                    j.erase("signatures");
                    j.erase("unsigned");

                    auto ssk = mtx::crypto::PkSigning::from_seed(decrypted);
                    myKey.signatures[http::client()->user_id().to_string()]
                                    ["ed25519:" + ssk.public_key()] = ssk.sign(j.dump());
                    req.signatures[http::client()->user_id().to_string()]
                                  [http::client()->device_id()] = myKey;
                }
            } else if (deviceKeys &&
                       secretName == mtx::secret_storage::secrets::cross_signing_master) {
                auto mk = mtx::crypto::PkSigning::from_seed(decrypted);

                if (deviceKeys->master_keys.user_id == http::client()->user_id().to_string() &&
                    deviceKeys->master_keys.keys["ed25519:" + mk.public_key()] == mk.public_key()) {
                    nlohmann::json j = deviceKeys->master_keys;
                    j.erase("signatures");
                    j.erase("unsigned");
                    mtx::crypto::CrossSigningKeys master_key =
                      j.get<mtx::crypto::CrossSigningKeys>();
                    master_key.signatures[http::client()->user_id().to_string()]
                                         ["ed25519:" + http::client()->device_id()] =
                      olm::client()->sign_message(j.dump());
                    req.signatures[http::client()->user_id().to_string()][mk.public_key()] =
                      master_key;
                }
            }
        }
    }

    if (!req.signatures.empty()) {
        nhlog::crypto()->debug("Uploading new signatures: {}", nlohmann::json(req).dump(2));
        http::client()->keys_signatures_upload(
          req, [](const mtx::responses::KeySignaturesUpload &res, mtx::http::RequestErr err) {
              if (err) {
                  nhlog::net()->error("failed to upload signatures: {}", *err);
              }

              for (const auto &[user_id, tmp] : res.errors)
                  for (const auto &[key_id, e] : tmp)
                      nhlog::net()->error("signature error for user '{}' and key "
                                          "id {}: {} {}",
                                          user_id,
                                          key_id,
                                          mtx::errors::to_string(e.errcode),
                                          e.error);
          });
    }
}
