// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SelfVerificationStatus.h"

#include <QApplication>

#include <nlohmann/json.hpp>

#include <mtx/responses/common.hpp>

#include "Olm.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "encryption/VerificationManager.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/TimelineViewManager.h"
#include "ui/UIA.h"

SelfVerificationStatus::SelfVerificationStatus(QObject *o)
  : QObject(o)
{
    cache::onSelfVerificationStatusChanged(this, [this] { invalidate(); });
    cache::onDatabaseReady(this, [this] { invalidate(); });

    connect(ChatPage::instance(), &ChatPage::contentLoaded, this, [this] {
        cache::markUserKeysOutOfDate({http::client()->user_id().to_string()});
        invalidate();
    });

    connect(ChatPage::instance(),
            &ChatPage::initializeEmptyViews,
            this,
            &SelfVerificationStatus::invalidate);
}

void
SelfVerificationStatus::setupCrosssigning(bool useSSSS,
                                          const QString &password,
                                          bool encryptionBackupOnlineEnabled)
{
    nhlog::db()->info("Clicked setup crossigning");

    auto xsign_keys = olm::client()->create_crosssigning_keys();

    if (!xsign_keys) {
        nhlog::crypto()->critical("Failed to setup cross-signing keys!");
        emit setupFailed(tr("Failed to create keys for cross-signing!"));
        return;
    }

    cache::storeSecret(mtx::secret_storage::secrets::cross_signing_master,
                       xsign_keys->private_master_key);
    cache::storeSecret(mtx::secret_storage::secrets::cross_signing_self_signing,
                       xsign_keys->private_self_signing_key);
    cache::storeSecret(mtx::secret_storage::secrets::cross_signing_user_signing,
                       xsign_keys->private_user_signing_key);

    std::optional<mtx::crypto::OlmClient::OnlineKeyBackupSetup> okb;
    if (encryptionBackupOnlineEnabled) {
        okb = olm::client()->create_online_key_backup(xsign_keys->private_master_key);
        if (!okb) {
            nhlog::crypto()->critical("Failed to setup online key backup!");
            emit setupFailed(tr("Failed to create keys for online key backup!"));
            return;
        }

        cache::storeSecret(mtx::secret_storage::secrets::megolm_backup_v1,
                           mtx::crypto::bin2base64(mtx::crypto::to_string(okb->privateKey)));

        http::client()->post_backup_version(
          okb->backupVersion.algorithm,
          okb->backupVersion.auth_data,
          [](const mtx::responses::Version &v, mtx::http::RequestErr e) {
              if (e) {
                  nhlog::net()->error("error setting up online key backup: {} {} {} {}",
                                      e->parse_error,
                                      e->status_code,
                                      e->error_code,
                                      e->matrix_error.error);
              } else {
                  nhlog::crypto()->info("Set up online key backup: '{}'", v.version);
              }
          });
    }

    std::optional<mtx::crypto::OlmClient::SSSSSetup> ssss;
    if (useSSSS) {
        ssss = olm::client()->create_ssss_key(password.toStdString());
        if (!ssss) {
            nhlog::crypto()->critical("Failed to setup secure server side secret storage!");
            emit setupFailed(tr("Failed to create keys for secure server side secret storage!"));
            return;
        }

        auto master      = mtx::crypto::PkSigning::from_seed(xsign_keys->private_master_key);
        nlohmann::json j = ssss->keyDescription;
        j.erase("signatures");
        ssss->keyDescription
          .signatures[http::client()->user_id().to_string()]["ed25519:" + master.public_key()] =
          master.sign(j.dump());

        http::client()->upload_secret_storage_key(
          ssss->keyDescription.name, ssss->keyDescription, [](mtx::http::RequestErr) {});
        http::client()->set_secret_storage_default_key(ssss->keyDescription.name,
                                                       [](mtx::http::RequestErr) {});

        auto uploadSecret = [ssss](std::string_view key_name, const std::string &secret) {
            mtx::secret_storage::Secret s;
            s.encrypted[ssss->keyDescription.name] =
              mtx::crypto::encrypt(secret, ssss->privateKey, key_name);
            http::client()->upload_secret_storage_secret(
              key_name, s, [key_name](mtx::http::RequestErr) {
                  nhlog::crypto()->info("Uploaded secret: {}", key_name);
              });
        };

        uploadSecret(mtx::secret_storage::secrets::cross_signing_master,
                     xsign_keys->private_master_key);
        uploadSecret(mtx::secret_storage::secrets::cross_signing_self_signing,
                     xsign_keys->private_self_signing_key);
        uploadSecret(mtx::secret_storage::secrets::cross_signing_user_signing,
                     xsign_keys->private_user_signing_key);

        if (okb)
            uploadSecret(mtx::secret_storage::secrets::megolm_backup_v1,
                         mtx::crypto::bin2base64(mtx::crypto::to_string(okb->privateKey)));
    }

    mtx::requests::DeviceSigningUpload device_sign{};
    device_sign.master_key       = xsign_keys->master_key;
    device_sign.self_signing_key = xsign_keys->self_signing_key;
    device_sign.user_signing_key = xsign_keys->user_signing_key;
    http::client()->device_signing_upload(
      device_sign,
      UIA::instance()->genericHandler(tr("Encryption Setup")),
      [this, ssss, xsign_keys](mtx::http::RequestErr e) {
          if (e) {
              nhlog::crypto()->critical("Failed to upload cross signing keys: {}",
                                        e->matrix_error.error);

              emit setupFailed(tr("Encryption setup failed: %1")
                                 .arg(QString::fromStdString(e->matrix_error.error)));
              return;
          }
          nhlog::crypto()->info("Crosssigning keys uploaded!");

          auto deviceKeys = cache::userKeys(http::client()->user_id().to_string());
          if (deviceKeys) {
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

                  auto ssk =
                    mtx::crypto::PkSigning::from_seed(xsign_keys->private_self_signing_key);
                  myKey.signatures[http::client()->user_id().to_string()]
                                  ["ed25519:" + ssk.public_key()] = ssk.sign(j.dump());
                  mtx::requests::KeySignaturesUpload req;
                  req.signatures[http::client()->user_id().to_string()]
                                [http::client()->device_id()] = myKey;

                  http::client()->keys_signatures_upload(
                    req,
                    [](const mtx::responses::KeySignaturesUpload &res, mtx::http::RequestErr err) {
                        if (err) {
                            nhlog::net()->error("failed to upload signatures: {},{}",
                                                mtx::errors::to_string(err->matrix_error.errcode),
                                                static_cast<int>(err->status_code));
                        }

                        // MSVC bug, error C3493: 'key_id' cannot be implicitly captured because no
                        // default capture mode has been specified
                        // for (const auto &[user_id, tmp] : res.errors)
                        //    for (const auto &[key_id, e_] : tmp)
                        //        nhlog::net()->error("signature error for user {} and key "
                        //                            "id {}: {}, {}",
                        //                            user_id,
                        //                            key_id,
                        //                            mtx::errors::to_string(e_.errcode),
                        //                            e_.error);
                        for (const auto &error : res.errors) {
                            const auto &user_id = error.first;
                            for (const auto &key_error : error.second) {
                                const auto &key_id = key_error.first;
                                const auto &e_     = key_error.second;

                                nhlog::net()->error("signature error for user {} and key "
                                                    "id {}: {}, {}",
                                                    user_id,
                                                    key_id,
                                                    mtx::errors::to_string(e_.errcode),
                                                    e_.error);
                            }
                        }
                    });
              }
          }

          if (ssss) {
              auto k = QString::fromStdString(mtx::crypto::key_to_recoverykey(ssss->privateKey));

              QString r;
              for (int i = 0; i < k.size(); i += 4)
                  r += k.mid(i, 4) + " ";

              emit showRecoveryKey(r.trimmed());
          } else {
              emit setupCompleted();
          }
      });
}

bool
SelfVerificationStatus::verifyMasterKey()
{
    nhlog::db()->info("Clicked verify master key");

    const auto this_user = http::client()->user_id().to_string();
    auto devices         = verificationDevicesFromMasterSignatures();

    if (!devices.empty()) {
        ChatPage::instance()->timelineManager()->verificationManager()->verifyOneOfDevices(
          QString::fromStdString(this_user), std::move(devices));
        return true;
    }

    nhlog::db()->info("No devices to verify");
    return false;
}

void
SelfVerificationStatus::verifyMasterKeyWithPassphrase()
{
    nhlog::db()->info("Clicked verify master key with passphrase");
    olm::download_cross_signing_keys();
}

void
SelfVerificationStatus::verifyUnverifiedDevices()
{
    nhlog::db()->info("Clicked verify unverified devices");
    const auto this_user = http::client()->user_id().to_string();

    auto keys  = cache::userKeys(this_user);
    auto verif = cache::verificationStatus(this_user);

    if (!keys || !verif)
        return;

    std::vector<QString> devices;
    for (const auto &[device, keys_] : keys->device_keys) {
        (void)keys_;
        if (!verif->verified_devices.count(device))
            devices.push_back(QString::fromStdString(device));
    }

    if (!devices.empty())
        ChatPage::instance()->timelineManager()->verificationManager()->verifyOneOfDevices(
          QString::fromStdString(this_user), std::move(devices));
}

void
SelfVerificationStatus::setupEncryptionBackup()
{
    const auto onlineBackupEnabled = UserSettings::instance()->encryptionBackupOnlineEnabled();
    nhlog::crypto()->info(
      "Setting up encryption backup via banner action (online backup enabled={})",
      onlineBackupEnabled);

    // Default backup flow:
    // - enable SSSS with recovery key mode (no passphrase)
    // - honor current online key-backup preference
    setupCrosssigning(true, QString(), onlineBackupEnabled);
}

void
SelfVerificationStatus::resetEncryptionIdentity()
{
    const auto onlineBackupEnabled = UserSettings::instance()->encryptionBackupOnlineEnabled();

    nhlog::crypto()->warn(
      "Reset requested for self-signing identity. Current status={}, online backup={}, "
      "SSSS enabled for this flow (recovery-key style, no passphrase).",
      static_cast<int>(status_),
      onlineBackupEnabled);
    nhlog::crypto()->warn(
      "Reset does not remove old server-side secret storage entries or old backup versions.");

    // Reset behavior: rotate/upload fresh cross-signing keys, reuse current online-backup setting,
    // and enable SSSS without an explicit passphrase (recovery key flow).
    setupCrosssigning(true, QString(), onlineBackupEnabled);
}

void
SelfVerificationStatus::promptCurrentVerificationAction()
{
    emit promptForStatus(static_cast<int>(status_));
}

std::vector<QString>
SelfVerificationStatus::verificationDevicesFromMasterSignatures() const
{
    std::vector<QString> devices;

    const auto this_user = http::client()->user_id().to_string();
    auto keys            = cache::userKeys(this_user);
    if (!keys)
        return devices;

    const auto sigsIt = keys->master_keys.signatures.find(this_user);
    if (sigsIt == keys->master_keys.signatures.end())
        return devices;

    for (const auto &[dev, sig] : sigsIt->second) {
        (void)sig;

        auto d = QString::fromStdString(dev);
        if (!d.startsWith(QLatin1String("ed25519:")))
            continue;

        d.remove(QStringLiteral("ed25519:"));
        if (keys->device_keys.count(d.toStdString()))
            devices.push_back(std::move(d));
    }

    return devices;
}

void
SelfVerificationStatus::invalidate()
{
    using namespace mtx::secret_storage;

    nhlog::db()->debug("Invalidating self verification status");
    if (!cache::isInitialized()) {
        nhlog::db()->debug("SelfVerificationStatus: cache not initialized yet");
        if (canVerifyWithAnotherDevice_) {
            canVerifyWithAnotherDevice_ = false;
            emit canVerifyWithAnotherDeviceChanged();
        }
        return;
    }

    // During startup, onDatabaseReady can fire before the Olm account is loaded/created.
    // Accessing verification state too early can throw "identity_keys: account == nullptr".
    try {
        (void)olm::client()->identity_keys();
    } catch (const std::exception &e) {
        nhlog::db()->debug("SelfVerificationStatus: OLM account not ready yet: {}", e.what());
        QTimer::singleShot(250, this, &SelfVerificationStatus::invalidate);
        return;
    }

    this->hasSSSS_ = false;
    emit hasSSSSChanged();

    auto keys = cache::userKeys(http::client()->user_id().to_string());
    if (!keys || keys->device_keys.find(http::client()->device_id()) == keys->device_keys.end()) {
        if (keys && (keys->seen_device_ids.count(http::client()->device_id()) ||
                     keys->seen_device_keys.count(olm::client()->identity_keys().curve25519))) {
            emit ChatPage::instance()->dropToLoginPageCb(
              tr("Identity key changed. This breaks E2EE, so logging out."));
            return;
        }

        cache::markUserKeysOutOfDate({http::client()->user_id().to_string()});

        QTimer::singleShot(1'000, this, [this] {
            cache::queryKeys(http::client()->user_id().to_string(),
                             [this](const UserKeyCache &, mtx::http::RequestErr) {
                                 QMetaObject::invokeMethod(
                                   this, &SelfVerificationStatus::invalidate, Qt::QueuedConnection);
                             });
        });
    }

    if (!keys) {
        if (canVerifyWithAnotherDevice_) {
            canVerifyWithAnotherDevice_ = false;
            emit canVerifyWithAnotherDeviceChanged();
        }
        return;
    }

    const bool canVerifyNow = !verificationDevicesFromMasterSignatures().empty();
    if (canVerifyWithAnotherDevice_ != canVerifyNow) {
        canVerifyWithAnotherDevice_ = canVerifyNow;
        emit canVerifyWithAnotherDeviceChanged();
    }

    if (keys->master_keys.keys.empty()) {
        if (status_ != SelfVerificationStatus::NoMasterKey) {
            this->status_ = SelfVerificationStatus::NoMasterKey;
            emit statusChanged();
        }
        return;
    }

    http::client()->secret_storage_secret(secrets::cross_signing_self_signing,
                                          [this](Secret secret, mtx::http::RequestErr err) {
                                              if (!err && !secret.encrypted.empty()) {
                                                  this->hasSSSS_ = true;
                                                  emit hasSSSSChanged();
                                              }
                                          });

    auto verifStatus = cache::verificationStatus(http::client()->user_id().to_string());
    if (!verifStatus || !verifStatus->user_verified) {
        if (status_ != SelfVerificationStatus::UnverifiedMasterKey) {
            this->status_ = SelfVerificationStatus::UnverifiedMasterKey;
            emit statusChanged();
        }
        return;
    }

    if (verifStatus->unverified_device_count > 0) {
        if (status_ != SelfVerificationStatus::UnverifiedDevices) {
            this->status_ = SelfVerificationStatus::UnverifiedDevices;
            emit statusChanged();
        }
        return;
    }

    if (status_ != SelfVerificationStatus::AllVerified) {
        this->status_ = SelfVerificationStatus::AllVerified;
        emit statusChanged();
        return;
    }
}

#include "moc_SelfVerificationStatus.cpp"
