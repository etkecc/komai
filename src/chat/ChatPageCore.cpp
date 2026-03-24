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

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "encryption/DeviceVerificationFlow.h"
#include "encryption/Olm.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "matrix/MatrixSyncUpdate.h"
#include "providers/AvatarProvider.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "ui/RoomSummary.h"
#include "ui/UserProfile.h"
#include "utils/Utils.h"
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

    connect(this,
            &ChatPage::initializeEmptyViews,
            view_manager_,
            &TimelineViewManager::initializeRoomlist);

    connect(
      this, &ChatPage::tryInitialSyncCb, this, &ChatPage::tryInitialSync, Qt::QueuedConnection);
    connect(this, &ChatPage::trySyncCb, this, &ChatPage::trySync, Qt::QueuedConnection);
    connect(
      this,
      &ChatPage::tryDelayedSyncCb,
      this,
      [this]() { QTimer::singleShot(RETRY_TIMEOUT, this, &ChatPage::trySync); },
      Qt::QueuedConnection);

    connect(this, &ChatPage::dropToLoginPageCb, this, &ChatPage::dropToLoginPage);

    connect(
      this,
      &ChatPage::startRemoveFallbackKeyTimer,
      this,
      [this]() {
          QTimer::singleShot(std::chrono::minutes(5), this, &ChatPage::removeOldFallbackKey);
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
