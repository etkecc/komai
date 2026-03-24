// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QMessageBox>
#include <QTimer>

#include <nlohmann/json.hpp>

#include <mtx/responses.hpp>

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "encryption/Olm.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"
#include "voip/CallManager.h"

void
ChatPage::bootstrap(QString userid,
                    QString deviceId,
                    QString homeserver,
                    QString token,
                    bool hadSessionIdentity)
{
    shuttingDown_ = false;

    using namespace mtx::identifiers;

    try {
        http::client()->set_user(parse<User>(userid.toStdString()));
    } catch (const std::invalid_argument &) {
        nhlog::ui()->critical("bootstrapped with invalid user_id: {}", userid.toStdString());
    }

    http::client()->set_server(homeserver.toStdString());
    http::client()->set_device_id(deviceId.toStdString());
    http::client()->set_access_token(token.toStdString());
    http::client()->verify_certificates(
      UserSettings::instance()->networkTlsEnableCertificateValidation());

    // The Olm client needs the user_id & device_id that will be included
    // in the generated payloads & keys.
    olm::client()->set_user_id(http::client()->user_id().to_string());
    olm::client()->set_device_id(http::client()->device_id());

    try {
        cache::init(userid);

        cache::onDatabaseReady(
          this, [this, userid, deviceId, homeserver, token, hadSessionIdentity] {
              if (shuttingDown_)
                  return;

              nhlog::db()->info("database ready");

              const bool isInitialized = cache::isInitialized();
              const auto cacheVersion  = cache::formatVersion();

              if (isInitialized && !hadSessionIdentity) {
                  nhlog::db()->warn("Cache exists, but no persisted session identity was loaded. "
                                    "Resetting cache to avoid identity/key mismatch.");
                  cache::disconnectFromCache(this);
                  cache::deleteData();

                  // Retry bootstrap once with a clean cache.
                  QTimer::singleShot(0, this, [this, userid, deviceId, homeserver, token]() {
                      bootstrap(userid, deviceId, homeserver, token, true);
                  });
                  return;
              }

              if (!isInitialized && hadSessionIdentity) {
                  nhlog::crypto()->critical(
                    "Persisted session identity exists, but cache is uninitialized. "
                    "Refusing to create a new Olm account for an existing session.");
                  emit dropToLoginPageCb(
                    tr("Local encryption data is missing for this signed-in session.\n\n"
                       "Close Komai and restore your old local data/secret-store backup if you "
                       "have one. Otherwise, sign in again to create a new encryption state."));
                  return;
              }

              try {
                  if (!isInitialized) {
                      cache::setCurrentFormat();
                  } else {
                      if (cacheVersion == cache::CacheVersion::Current) {
                          loadStateFromCache();
                          return;
                      }

                      if (!cache::runMigrations()) {
                          QMessageBox::critical(
                            nullptr,
                            tr("Cache reset failed!"),
                            tr("Resetting incompatible local cache data failed. "
                               "Please open an issue at https://github.com/etkecc/komai "
                               "and try deleting cache data manually."));
                          QCoreApplication::quit();
                      }
                      loadStateFromCache();
                      return;
                  }

                  // It's the first time syncing with this device
                  // There isn't a saved olm account to restore.
                  nhlog::crypto()->info("creating new olm account");
                  olm::client()->create_new_account();
                  auto secret = cache::createPickleSecret();
                  cache::saveOlmAccount(olm::client()->save(secret));
              } catch (const mtx::crypto::olm_exception &e) {
                  nhlog::crypto()->critical("failed to create new olm account {}", e.what());
                  emit dropToLoginPageCb(QString::fromStdString(e.what()));
                  return;
              } catch (const std::exception &e) {
                  nhlog::crypto()->critical("failed to save olm account {}", e.what());
                  emit dropToLoginPageCb(QString::fromStdString(e.what()));
                  return;
              }

              getProfileInfo();
              getBackupVersion();
              tryInitialSync();
              if (UserSettings::instance()->callsLegacyEnabled())
                  callManager_->refreshTurnServer();
              emit MainWindow::instance()->reload();
          });

        cache::onReadReceiptsChanged(
          view_manager_, [this](const QString &room_id, const std::vector<QString> &event_ids) {
              view_manager_->updateReadReceipts(room_id, event_ids);
          });

        cache::onSecretChanged(this, [this](const std::string &secret) {
            if (secret == mtx::secret_storage::secrets::megolm_backup_v1) {
                getBackupVersion();
            }
        });
    } catch (const std::exception &e) {
        nhlog::db()->critical("failure during boot: {}", e.what());
        emit dropToLoginPageCb(tr("Failed to open database, logging out!"));
    }
}

void
ChatPage::loadStateFromCache()
{
    nhlog::db()->info("restoring state from cache");

    auto secret = cache::pickleSecret();
    if (secret.empty()) {
        nhlog::crypto()->critical("pickle secret is empty — secret storage may be unavailable");
        emit dropToLoginPageCb(
          tr("Could not retrieve the encryption secret from your system's secret "
             "storage (e.g. KWallet, GNOME Keyring). This is usually a temporary problem.\n\n"
             "You can close Komai, make sure your secret storage is unlocked, and relaunch. "
             "Your data has not been deleted."));
        return;
    }

    try {
        olm::client()->load(cache::restoreOlmAccount(), secret);

        nhlog::db()->info("Removing old cached messages");
        cache::deleteOldData();
        nhlog::db()->info("Message removal done");

        emit initializeEmptyViews();

        cache::calculateRoomReadStatus();

    } catch (const mtx::crypto::olm_exception &e) {
        nhlog::crypto()->critical("failed to restore olm account: {}", e.what());
        emit dropToLoginPageCb(tr("Failed to restore OLM account."));
        return;
    } catch (const nlohmann::json::exception &e) {
        nhlog::db()->critical("failed to parse cache data: {}", e.what());
        emit dropToLoginPageCb(tr("Failed to restore save data."));
        return;
    } catch (const std::exception &e) {
        nhlog::db()->critical("failed to restore cache: {}", e.what());
        emit dropToLoginPageCb(tr("Failed to restore save data."));
        return;
    }

    nhlog::crypto()->info("ed25519   : {}", olm::client()->identity_keys().ed25519);
    nhlog::crypto()->info("curve25519: {}", olm::client()->identity_keys().curve25519);

    getProfileInfo();
    getBackupVersion();
    verifyOneTimeKeyCountAfterStartup();
    if (UserSettings::instance()->callsLegacyEnabled())
        callManager_->refreshTurnServer();

    emit contentLoaded();

    // Schedule fallback-key retirement after the first steady-state sync arrives.
    scheduleFallbackKeyRemovalOnNextSync_ = true;

    // Start receiving events.
    emit trySyncCb();
}

void
ChatPage::removeRoom(const QString &room_id)
{
    try {
        cache::removeRoom(room_id);
    } catch (const std::exception &e) {
        nhlog::db()->critical("failure while removing room: {}", e.what());
        // TODO: Notify the user.
    }
}

void
ChatPage::receivedSessionKey(const std::string &room_id, const std::string &session_id)
{
    view_manager_->receivedSessionKey(room_id, session_id);
}
