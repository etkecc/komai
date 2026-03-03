// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QThread>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include <mtx/responses.hpp>

#include "AvatarProvider.h"
#include "ChatPage.h"
#include "EventAccessors.h"
#include "Logging.h"
#include "MainWindow.h"
#include "MatrixClient.h"
#include "Utils.h"
#include "cache/Cache.h"
#include "encryption/DeviceVerificationFlow.h"
#include "encryption/Olm.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/RoomSummary.h"
#include "ui/UserProfile.h"
#include "voip/CallManager.h"

#include "notifications/Manager.h"

#include "timeline/Permissions.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineModel.h"
#include "timeline/TimelineViewManager.h"

ChatPage *ChatPage::instance_                    = nullptr;
static constexpr int CHECK_CONNECTIVITY_INTERVAL = 15'000;
static constexpr int RETRY_TIMEOUT               = 5'000;

ChatPage::ChatPage(QSharedPointer<UserSettings> userSettings, QObject *parent)
  : QObject(parent)
  , isConnected_(true)
  , userSettings_{userSettings}
  , notificationsManager(new NotificationsManager(this))
  , callManager_(new CallManager(this))
{
    setObjectName(QStringLiteral("chatPage"));

    instance_ = this;

    view_manager_ = new TimelineViewManager(callManager_, this);

    connect(this,
            &ChatPage::downloadedSecrets,
            this,
            &ChatPage::decryptDownloadedSecrets,
            Qt::QueuedConnection);

    connect(this, &ChatPage::connectionLost, this, [this]() {
        nhlog::net()->info("connectivity lost");
        isConnected_ = false;
        http::client()->shutdown();
    });
    connect(this, &ChatPage::connectionRestored, this, [this]() {
        nhlog::net()->info("trying to re-connect");
        isConnected_ = true;

        // Drop all pending connections.
        http::client()->shutdown();
        trySync();
    });

    connectivityTimer_.setInterval(CHECK_CONNECTIVITY_INTERVAL);
    connect(&connectivityTimer_, &QTimer::timeout, this, [this]() {
        if (http::client()->access_token().empty()) {
            connectivityTimer_.stop();
            return;
        }

        http::client()->versions(
          [this](const mtx::responses::Versions &, mtx::http::RequestErr err) {
              if (err) {
                  emit connectionLost();
                  return;
              }

              // only update spaces every 20 minutes
              if (lastSpacesUpdate < QDateTime::currentDateTime().addSecs(-20 * 60)) {
                  lastSpacesUpdate = QDateTime::currentDateTime();
                  utils::updateSpaceVias();
                  utils::removeExpiredEvents();
              }

              if (!isConnected_)
                  emit connectionRestored();
          });
    });

    connect(
      view_manager_,
      &TimelineViewManager::inviteUsers,
      this,
      [this](QString roomId, QStringList users) {
          for (int ii = 0; ii < users.size(); ++ii) {
              QTimer::singleShot(ii * 500, this, [this, roomId, ii, users]() {
                  const auto user = users.at(ii);

                  http::client()->invite_user(
                    roomId.toStdString(),
                    user.toStdString(),
                    [this, user](const mtx::responses::RoomInvite &, mtx::http::RequestErr err) {
                        if (err) {
                            emit showNotification(tr("Failed to invite user: %1").arg(user));
                            return;
                        }

                        emit showNotification(tr("Invited user: %1").arg(user));
                    });
              });
          }
      });

    connect(this,
            &ChatPage::internalKnock,
            this,
            qOverload<const QString &, const std::vector<std::string> &, QString, bool, bool>(
              &ChatPage::knockRoom),
            Qt::QueuedConnection);
    connect(this, &ChatPage::leftRoom, this, &ChatPage::removeRoom);
    connect(this, &ChatPage::changeToRoom, this, &ChatPage::changeRoom, Qt::QueuedConnection);
    connect(notificationsManager,
            &NotificationsManager::notificationClicked,
            this,
            [this](const QString &roomid, const QString &eventid) {
                Q_UNUSED(eventid)
                auto exWin = MainWindow::instance()->windowForRoom(roomid);
                if (exWin) {
                    exWin->setVisible(true);
                    exWin->raise();
                    exWin->requestActivate();
                } else {
                    view_manager_->rooms()->setCurrentRoom(roomid);
                    MainWindow::instance()->setVisible(true);
                    MainWindow::instance()->raise();
                    MainWindow::instance()->requestActivate();
                }
            });
    connect(notificationsManager,
            &NotificationsManager::sendNotificationReply,
            this,
            &ChatPage::sendNotificationReply);

    connect(
      this,
      &ChatPage::initializeViews,
      view_manager_,
      [this](const mtx::responses::Sync &sync) { view_manager_->sync(sync); },
      Qt::QueuedConnection);
    connect(this,
            &ChatPage::initializeEmptyViews,
            view_manager_,
            &TimelineViewManager::initializeRoomlist);
    connect(this, &ChatPage::syncUI, this, &ChatPage::processSyncUi);

    connect(
      this, &ChatPage::tryInitialSyncCb, this, &ChatPage::tryInitialSync, Qt::QueuedConnection);
    connect(this, &ChatPage::trySyncCb, this, &ChatPage::trySync, Qt::QueuedConnection);
    connect(
      this,
      &ChatPage::tryDelayedSyncCb,
      this,
      [this]() { QTimer::singleShot(RETRY_TIMEOUT, this, &ChatPage::trySync); },
      Qt::QueuedConnection);

    connect(
      this, &ChatPage::newSyncResponse, this, &ChatPage::handleSyncResponse, Qt::QueuedConnection);

    connect(this, &ChatPage::dropToLoginPageCb, this, &ChatPage::dropToLoginPage);

    connect(
      this,
      &ChatPage::startRemoveFallbackKeyTimer,
      this,
      [this]() {
          QTimer::singleShot(std::chrono::minutes(5), this, &ChatPage::removeOldFallbackKey);
          disconnect(
            this, &ChatPage::newSyncResponse, this, &ChatPage::startRemoveFallbackKeyTimer);
      },
      Qt::QueuedConnection);

    connect(
      this,
      &ChatPage::callFunctionOnGuiThread,
      this,
      [](std::function<void()> f) { f(); },
      Qt::QueuedConnection);

    connect(qobject_cast<QGuiApplication *>(QGuiApplication::instance()),
            &QGuiApplication::focusWindowChanged,
            this,
            [this](QWindow *activeWindow) {
                if (activeWindow) {
                    nhlog::ui()->debug("Stopping inactive timer.");
                    lastWindowActive = QDateTime();
                } else {
                    nhlog::ui()->debug("Starting inactive timer.");
                    lastWindowActive = QDateTime::currentDateTime();
                }
            });

    connectCallMessage<mtx::events::voip::CallInvite>();
    connectCallMessage<mtx::events::voip::CallCandidates>();
    connectCallMessage<mtx::events::voip::CallAnswer>();
    connectCallMessage<mtx::events::voip::CallHangUp>();
    connectCallMessage<mtx::events::voip::CallSelectAnswer>();
    connectCallMessage<mtx::events::voip::CallReject>();
    connectCallMessage<mtx::events::voip::CallNegotiate>();
}

void
ChatPage::dropToLoginPage(const QString &msg)
{
    if (shuttingDown_)
        return;

    nhlog::ui()->info("dropping to the login page: {}", msg.toStdString());

    http::client()->shutdown();
    connectivityTimer_.stop();

    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(tr("Something went wrong"));
    msgBox.setText(
      tr("Komai ran into a problem:\n\n%1\n\n"
         "This may be a temporary issue (e.g. your system's secret storage failed to unlock). "
         "If so, you can close Komai, fix the problem, and relaunch — your data will still be "
         "there.\n\n"
         "If the problem persists, you can log out and sign in again, but this will delete your "
         "local message cache and encryption session.")
        .arg(msg));
    auto *closeBtn  = msgBox.addButton(tr("Close && preserve data"), QMessageBox::RejectRole);
    auto *logoutBtn = msgBox.addButton(tr("Log out && start over"), QMessageBox::DestructiveRole);
    msgBox.setDefaultButton(closeBtn);

    msgBox.exec();

    if (msgBox.clickedButton() == static_cast<QAbstractButton *>(logoutBtn)) {
        performLogout(LogoutPolicy::LocalOnly, LogoutRoute::ViaShowLoginPageSignal, msg);
    } else {
        QCoreApplication::exit(1);
        exit(1);
    }
}

void
ChatPage::resetUI()
{
    view_manager_->clearAll();

    emit unreadMessages(0);
}

void
ChatPage::deleteConfigs()
{
    cache::disconnectFromCache(this);
    UserSettings::instance()->clearAuth();
    http::client()->shutdown();
    cache::deleteData();
}

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
                      } else {
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

    // Start receiving events.
    connect(this, &ChatPage::newSyncResponse, &ChatPage::startRemoveFallbackKeyTimer);
    emit trySyncCb();
}

void
ChatPage::removeRoom(const QString &room_id)
{
    try {
        cache::removeRoom(room_id);
        cache::removeInvite(room_id.toStdString());
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

template<typename T>
void
ChatPage::connectCallMessage()
{
    connect(callManager_,
            qOverload<const QString &, const T &>(&CallManager::newMessage),
            view_manager_,
            qOverload<const QString &, const T &>(&TimelineViewManager::queueCallMessage));
}

void
ChatPage::removeAllNotifications()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    notificationsManager->closeAllNotifications();
#endif
}

#include "moc_ChatPage.cpp"
