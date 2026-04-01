// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QThread>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "matrix/backend/MatrixSdkPaths.h"
#include "matrix/backend/MatrixSessionSecrets.h"
#include "providers/AvatarProvider.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "ui/RoomSummary.h"
#include "ui/UserProfile.h"
#include "utils/Utils.h"
#include "voip/CallManager.h"

#include "notifications/Manager.h"

#include "timeline/Permissions.h"
#include "timeline/PresenceEmitter.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineViewManager.h"

namespace {

struct OwnProfileBootstrapResult
{
    std::optional<komai::MatrixOwnProfile> profile;
    std::optional<komai::MatrixOwnPresence> presence;
    QString profileError;
    QString presenceError;
};

QString
presenceStateForPolicy(UserSettings::Presence policy)
{
    switch (policy) {
    case UserSettings::Presence::Unavailable:
        return QStringLiteral("unavailable");
    case UserSettings::Presence::Offline:
        return QStringLiteral("offline");
    case UserSettings::Presence::AutomaticPresence:
        // Until activity-driven automatic presence is ported, treat automatic
        // as the default online state instead of leaving the setting inert.
        [[fallthrough]];
    case UserSettings::Presence::Online:
    default:
        return QStringLiteral("online");
    }
}

} // namespace

ChatPage *ChatPage::instance_ = nullptr;

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

    connect(this, &ChatPage::connectionLost, this, [this]() {
        nhlog::net()->info("connectivity lost");
        isConnected_ = false;
    });
    connect(this, &ChatPage::connectionRestored, this, [this]() {
        nhlog::net()->info("connectivity restored");
        isConnected_ = true;
    });

    connect(view_manager_,
            &TimelineViewManager::inviteUsers,
            this,
            [this](QString roomId, QStringList users) {
                auto *mainWindow    = MainWindow::instance();
                const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
                if (handleId == 0) {
                    emit showNotification(
                      tr("Cannot invite users until the Matrix session is ready."));
                    return;
                }

                auto inviteNext = std::make_shared<std::function<void(int)>>();
                *inviteNext     = [this, roomId, users, handleId, inviteNext](int index) {
                    if (index >= users.size())
                        return;

                    const auto user = users.at(index);
                    QPointer<ChatPage> guard(this);
                    std::thread([guard, roomId, user, handleId, index, inviteNext]() {
                        const auto context = komai::matrix_backend::blockingCallContext();
                        QString error;
                        const bool ok = komai::MatrixBackendRuntimeService::inviteUser(
                          context, handleId, roomId, user, QString(), &error);

                        if (!guard)
                            return;

                        emit guard->callFunctionOnGuiThread(
                          [guard, roomId, user, ok, error, index, inviteNext]() {
                              if (!guard || guard->shuttingDown_)
                                  return;

                              if (!ok) {
                                  nhlog::ui()->warn(
                                    "Failed to invite {} to {} via matrix-sdk runtime: {}",
                                    user.toStdString(),
                                    roomId.toStdString(),
                                    error.toStdString());
                                  emit guard->showNotification(
                                    ChatPage::tr("Failed to invite %1: %2").arg(user, error));
                              } else {
                                  emit guard->showNotification(
                                    ChatPage::tr("Invited user: %1").arg(user));
                              }

                              (*inviteNext)(index + 1);
                          });
                    }).detach();
                };
                (*inviteNext)(0);
            });

    connect(this,
            &ChatPage::internalKnock,
            this,
            qOverload<const QString &, const std::vector<std::string> &, QString, bool, bool>(
              &ChatPage::knockRoom),
            Qt::QueuedConnection);
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

                notificationsManager->removeNotifications(roomid, {});
            });
    connect(notificationsManager,
            &NotificationsManager::sendNotificationReply,
            this,
            &ChatPage::sendNotificationReply);
    connect(view_manager_->rooms(),
            &RoomlistModel::currentRoomIdChanged,
            this,
            [this](const QString &roomId) {
                if (roomId.isEmpty() || !notificationsManager)
                    return;

                notificationsManager->removeNotifications(roomId, {});
            });
    connect(userSettings_.get(),
            &UserSettings::networkPresenceStatusPolicyChanged,
            this,
            [this](UserSettings::Presence) { syncOwnPresence(); });

    connect(this,
            &ChatPage::initializeEmptyViews,
            view_manager_,
            &TimelineViewManager::initializeRoomlist,
            Qt::QueuedConnection);

    connect(this, &ChatPage::dropToLoginPageCb, this, &ChatPage::dropToLoginPage);

    connect(
      this,
      &ChatPage::callFunctionOnGuiThread,
      this,
      [](std::function<void()> f) { f(); },
      Qt::QueuedConnection);

    connect(callManager_,
            &CallManager::newMessage,
            view_manager_,
            &TimelineViewManager::queueCallMessage);
}

QString
ChatPage::status() const
{
    return statusMessageShadow_.value_or(QString{});
}

void
ChatPage::setStatus(const QString &status)
{
    statusMessageShadow_ = status;
    syncOwnPresence();
}

void
ChatPage::syncOwnPresence()
{
    const auto localUserId   = utils::localUser();
    const auto policy        = userSettings_ ? userSettings_->networkPresenceStatusPolicy()
                                             : UserSettings::Presence::AutomaticPresence;
    const auto presenceState = presenceStateForPolicy(policy);
    const auto statusMessage = status();

    if (auto *presence = PresenceEmitter::instance())
        presence->setLocalPresence(localUserId, presenceState, statusMessage);

    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        nhlog::net()->debug(
          "Skipping matrix-sdk own-presence update because no runtime handle is active");
        return;
    }

    std::thread([handleId, presenceState, statusMessage]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const bool ok = komai::MatrixBackendRuntimeService::setOwnPresence(
          context, handleId, presenceState, statusMessage, &error);

        if (!ok) {
            nhlog::net()->warn("Failed to set own presence via matrix-sdk runtime handle {}: {}",
                               handleId,
                               error.toStdString());
        }
    }).detach();
}

void
ChatPage::getProfileInfo()
{
    const auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() == 0) {
        nhlog::net()->warn(
          "Cannot retrieve own profile via matrix-sdk runtime because no runtime handle is active");
        return;
    }

    const auto handleId = mainWindow->matrixBackendHandleId();
    QPointer<ChatPage> guard(this);

    std::thread([guard, handleId]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        OwnProfileBootstrapResult result;
        result.profile = komai::MatrixBackendRuntimeService::fetchOwnProfile(
          context, handleId, &result.profileError);
        result.presence = komai::MatrixBackendRuntimeService::fetchOwnPresence(
          context, handleId, &result.presenceError);

        if (!guard)
            return;

        emit guard->callFunctionOnGuiThread([guard, result = std::move(result)]() mutable {
            if (!guard || guard->shuttingDown_)
                return;

            if (!result.profile) {
                nhlog::net()->warn("Failed to retrieve own profile info via matrix-sdk runtime "
                                   "handle: {}",
                                   result.profileError.toStdString());
            } else {
                emit guard->setUserDisplayName(result.profile->displayName);
                emit guard->setUserAvatar(result.profile->avatarUrl);
            }

            if (!result.presenceError.isEmpty()) {
                nhlog::net()->warn("Failed to retrieve own presence via matrix-sdk runtime "
                                   "handle: {}",
                                   result.presenceError.toStdString());
            } else if (result.presence && !guard->statusMessageShadow_.has_value()) {
                guard->statusMessageShadow_ = result.presence->statusMessage;
            }

            guard->syncOwnPresence();
        });
    }).detach();
}

bool
ChatPage::isRoomActive(const QString &room_id)
{
    return QGuiApplication::focusWindow() && QGuiApplication::focusWindow()->isActive() &&
           MainWindow::instance()->windowForRoom(room_id) == QGuiApplication::focusWindow();
}

void
ChatPage::dispatchMatrixNotification(const komai::MatrixNotificationItem &notification)
{
    if (shuttingDown_ || !notificationsManager || !userSettings_)
        return;

    if (!userSettings_->notificationsAccountEnabled() || !userSettings_->notificationsEnabled())
        return;

    if (notification.roomId.trimmed().isEmpty() || notification.eventId.trimmed().isEmpty())
        return;

    if (isRoomActive(notification.roomId))
        return;

    komai::NotificationPayload payload{
      .roomId             = notification.roomId,
      .eventId            = notification.eventId,
      .replacementEventId = notification.replacementEventId,
      .roomName           = notification.roomName,
      .senderDisplayName  = notification.senderDisplayName,
      .plainBody          = notification.plainBody,
      .formattedBody      = notification.formattedBody,
      .mediaMxcUrl        = notification.mediaMxcUrl,
      .isReply            = notification.isReply,
      .isEmote            = notification.isEmote,
      .isEncrypted        = notification.isEncrypted,
      .containsSpoiler    = notification.containsSpoiler,
      .hasInlineImage     = notification.hasInlineImage,
      .playSound          = notification.playSound,
    };

    QPointer<ChatPage> guard(this);
    QPointer<NotificationsManager> notificationManager(notificationsManager);
    AvatarProvider::resolve(
      notification.avatarUrl,
      96,
      this,
      [guard, notificationManager, payload = std::move(payload)](QPixmap pixmap) mutable {
          if (!guard || !notificationManager || guard->shuttingDown_ || !guard->userSettings_)
              return;

          if (!guard->userSettings_->notificationsAccountEnabled() ||
              !guard->userSettings_->notificationsEnabled()) {
              return;
          }

          if (guard->isRoomActive(payload.roomId))
              return;

          notificationManager->postNotification(payload, pixmap.toImage());
      });
}

void
ChatPage::dropToLoginPage(const QString &msg)
{
    if (shuttingDown_)
        return;

    nhlog::ui()->info("dropping to the login page: {}", msg.toStdString());

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
    const auto profileId = UserSettings::instance()->profile();
    UserSettings::instance()->clearAuth();
    komai::matrix_backend::clearPersistedMatrixSessionSecrets(profileId);

    const auto matrixPaths = komai::MatrixSdkPathsProvider::forProfile(profileId);
    if (!matrixPaths.matrixDataRoot.isEmpty())
        QDir(matrixPaths.matrixDataRoot).removeRecursively();
    if (!matrixPaths.matrixCacheRoot.isEmpty())
        QDir(matrixPaths.matrixCacheRoot).removeRecursively();
}

void
ChatPage::removeAllNotifications()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    notificationsManager->closeAllNotifications();
#endif
}

#include "moc_ChatPage.cpp"
