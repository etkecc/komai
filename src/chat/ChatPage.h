// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <optional>

#include <QSharedPointer>
#include <QTimer>

#include "ui/RoomSummary.h"

class TimelineViewManager;
class UserSettings;
class NotificationsManager;
class CallManager;

namespace komai {
struct MatrixCreateRoomRequest;
struct MatrixNotificationItem;
}

class ChatPage final : public QObject
{
    Q_OBJECT

public:
    ChatPage(QSharedPointer<UserSettings> userSettings, QObject *parent = nullptr);

    // Initialize all the components of the UI.
    void bootstrap(QString userid,
                   QString deviceId,
                   QString homeserver,
                   QString token,
                   bool hadSessionIdentity);

    static ChatPage *instance() { return instance_; }

    QSharedPointer<UserSettings> userSettings() { return userSettings_; }
    CallManager *callManager() { return callManager_; }
    TimelineViewManager *timelineManager() { return view_manager_; }
    bool isShuttingDown() const { return shuttingDown_.load(); }

    void initiateLogout();
    void prepareShutdown();

    QString status() const;
    void setStatus(const QString &status);

    void startChat(QString userid, std::optional<bool> encryptionEnabled);

    //! Check if the given room is currently open.
    bool isRoomActive(const QString &room_id);
    void dispatchMatrixNotification(const komai::MatrixNotificationItem &notification);

    void removeAllNotifications();

public slots:
    bool tryHandleMatrixUri(QString uri);
    bool tryHandleMatrixUri(const QUrl &uri);

    void startChat(QString userid) { startChat(userid, std::nullopt); }
    void leaveRoom(const QString &room_id, const QString &reason);
    void createRoom(const komai::MatrixCreateRoomRequest &request);
    void joinRoom(const QString &room, const QString &reason = "");
    void knockRoom(const QString &room, QString reason = "") { knockRoom(room, {}, reason, false); }
    void knockRoom(const QString &room,
                   const std::vector<std::string> &via,
                   QString reason             = "",
                   bool failedJoin            = false,
                   bool promptForConfirmation = true);
    void joinRoomVia(const std::string &room_id,
                     const std::vector<std::string> &via,
                     bool promptForConfirmation = true,
                     const QString &reason      = "");

    void inviteUser(const QString &room, QString userid, QString reason);
    void kickUser(const QString &room, QString userid, QString reason);
    void banUser(const QString &room, QString userid, QString reason);
    void unbanUser(const QString &room, QString userid, QString reason);

    void decryptDownloadedSecrets();
    void submitSecretUnlockInput(const QString &text);
    void cancelSecretUnlockInput();
    void sendNotificationReply(const QString &roomid, const QString &eventid, const QString &body);
signals:
    void connectionLost();
    void connectionRestored();

    void contentLoaded();
    void closing();
    void changeWindowTitle(const int);
    void unreadMessages(int count);
    void showNotification(const QString &msg);
    void showLoginPage(const QString &msg);
    void showUserSettingsPage();

    void ownProfileOk();
    void setUserDisplayName(const QString &name);
    void setUserAvatar(const QString &avatar);
    void loggedOut();

    void newRoom(const QString &room_id);
    void changeToRoom(const QString &room_id);

    void initializeEmptyViews();
    void dropToLoginPageCb(const QString &msg);

    void themeChanged();

    void promptUnlockKeyBackup();

    void showRoomJoinPrompt(RoomSummary *);
    void internalKnock(const QString &room,
                       const std::vector<std::string> &via,
                       QString reason             = "",
                       bool failedJoin            = false,
                       bool promptForConfirmation = true);
    void callFunctionOnGuiThread(std::function<void()>);

private slots:
    void changeRoom(const QString &room_id);
    void dropToLoginPage(const QString &msg);

private:
    enum class LogoutPolicy
    {
        BestEffortServerFirst,
        LocalOnly,
    };

    enum class LogoutRoute
    {
        ViaClosingSignal,
        ViaShowLoginPageSignal,
    };

    void
    performLogout(LogoutPolicy policy, LogoutRoute route, const QString &loginMessage = QString());
    void finalizeLogout(LogoutRoute route, const QString &loginMessage = QString());
    static ChatPage *instance_;

    void getProfileInfo();
    void syncOwnPresence();
    void clearRoomNotifications(const QString &roomId);

    void resetUI();
    void deleteConfigs();
    void processDownloadedSecretsUnlockInput(const QString &text);

    TimelineViewManager *view_manager_;

    QTimer connectivityTimer_;
    std::atomic_bool isConnected_;
    std::atomic_bool shuttingDown_{false};

    // Global user settings.
    QSharedPointer<UserSettings> userSettings_;

    NotificationsManager *notificationsManager;
    CallManager *callManager_;

    // Last locally known own status message. Populated from local submits and/or own-presence
    // bootstrap fetches so UI can reflect changes immediately.
    std::optional<QString> statusMessageShadow_;

    bool pendingSecretsUnlockRequest_ = false;
};
