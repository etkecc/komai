// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <optional>

#include <mtx/events.hpp>
#include <mtx/events/presence.hpp>
#include <mtx/secret_storage.hpp>

#include <QDateTime>
#include <QSharedPointer>
#include <QTimer>

#include "matrix/MatrixSyncUpdate.h"
#include "ui/RoomSummary.h"

class TimelineViewManager;
class UserSettings;
class NotificationsManager;
class TimelineModel;
class CallManager;

namespace mtx::requests {
struct CreateRoom;
}
namespace mtx::responses {
struct Notifications;
struct Sync;
struct Timeline;
struct Rooms;
}

using SecretsToDecrypt = std::map<std::string, mtx::secret_storage::AesHmacSha2EncryptedData>;

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

    mtx::presence::PresenceState currentPresence() const;

    void startChat(QString userid, std::optional<bool> encryptionEnabled);

    //! Check if the given room is currently open.
    bool isRoomActive(const QString &room_id);

    const std::unique_ptr<mtx::pushrules::PushRuleEvaluator> &pushruleEvaluator() const
    {
        return pushrules;
    }

    void removeAllNotifications();

public slots:
    bool tryHandleMatrixUri(QString uri);
    bool tryHandleMatrixUri(const QUrl &uri);

    void startChat(QString userid) { startChat(userid, std::nullopt); }
    void leaveRoom(const QString &room_id, const QString &reason);
    void createRoom(const mtx::requests::CreateRoom &req);
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

    void receivedSessionKey(const std::string &room_id, const std::string &session_id);
    void decryptDownloadedSecrets(mtx::secret_storage::AesHmacSha2KeyDescription keyDesc,
                                  const SecretsToDecrypt &secrets);
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

    void trySyncCb();
    void tryDelayedSyncCb();
    void tryInitialSyncCb();
    void leftRoom(const QString &room_id);
    void newRoom(const QString &room_id);
    void changeToRoom(const QString &room_id);
    void startRemoveFallbackKeyTimer();

    void initializeEmptyViews();
    void dropToLoginPageCb(const QString &msg);

    void notifyMessage(const QString &roomid,
                       const QString &eventid,
                       const QString &roomname,
                       const QString &sender,
                       const QString &message,
                       const QImage &icon);

    void retrievedPresence(const QString &statusMsg, mtx::presence::PresenceState state);
    void themeChanged();

    //! Signals for device verificaiton
    void receivedDeviceVerificationAccept(const mtx::events::msg::KeyVerificationAccept &message);
    void receivedDeviceVerificationRequest(const mtx::events::msg::KeyVerificationRequest &message,
                                           std::string sender);
    void receivedRoomDeviceVerificationRequest(
      const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationRequest> &message,
      TimelineModel *model);
    void receivedDeviceVerificationCancel(const mtx::events::msg::KeyVerificationCancel &message);
    void receivedDeviceVerificationKey(const mtx::events::msg::KeyVerificationKey &message);
    void receivedDeviceVerificationMac(const mtx::events::msg::KeyVerificationMac &message);
    void receivedDeviceVerificationStart(const mtx::events::msg::KeyVerificationStart &message,
                                         std::string sender);
    void receivedDeviceVerificationReady(const mtx::events::msg::KeyVerificationReady &message);
    void receivedDeviceVerificationDone(const mtx::events::msg::KeyVerificationDone &message);

    void downloadedSecrets(mtx::secret_storage::AesHmacSha2KeyDescription keyDesc,
                           const SecretsToDecrypt &secrets);
    void promptUnlockKeyBackup();

    void showRoomJoinPrompt(RoomSummary *);
    void internalKnock(const QString &room,
                       const std::vector<std::string> &via,
                       QString reason             = "",
                       bool failedJoin            = false,
                       bool promptForConfirmation = true);
    void newOnlineKeyBackupAvailable();

    void callFunctionOnGuiThread(std::function<void()>);

private slots:
    void removeRoom(const QString &room_id);
    void changeRoom(const QString &room_id);
    void dropToLoginPage(const QString &msg);

    void handleSyncResponse(const mtx::responses::Sync &res, const std::string &prev_batch_token);

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

    void startInitialSync();
    void tryInitialSync();
    void trySync();
    void verifyOneTimeKeyCountAfterStartup();
    void ensureOneTimeKeyCount(const std::map<std::string_view, uint16_t> &counts,
                               const std::optional<std::vector<std::string>> &fallback_keys);
    void removeOldFallbackKey();
    void getProfileInfo();
    void getBackupVersion();

    void loadStateFromCache();
    void resetUI();
    void deleteConfigs();
    void processSyncUi(const komai::NotificationSyncUpdate &sync);

    // returns if the user had no interaction with Komai for quite a while, which means we set our
    // presence to unavailable if automatic presence is enabled
    bool shouldBeUnavailable() const;
    // If we should throttle sync processing to reduce CPU load, if people are spamming messages and
    // we aren't looking
    bool shouldThrottleSync() const;

    template<typename T>
    void connectCallMessage();
    void processDownloadedSecretsUnlockInput(mtx::secret_storage::AesHmacSha2KeyDescription keyDesc,
                                             const SecretsToDecrypt &secrets,
                                             const QString &text);

    TimelineViewManager *view_manager_;

    QTimer connectivityTimer_;
    std::atomic_bool isConnected_;
    std::atomic_bool shuttingDown_{false};

    // Global user settings.
    QSharedPointer<UserSettings> userSettings_;

    NotificationsManager *notificationsManager;
    CallManager *callManager_;

    std::unique_ptr<mtx::pushrules::PushRuleEvaluator> pushrules;

    QDateTime lastSpacesUpdate                 = QDateTime::currentDateTime();
    bool scheduleFallbackKeyRemovalOnNextSync_ = false;

    // Stores when our windows lost focus. Invalid when our windows have focus.
    QDateTime lastWindowActive;

    // Last locally known status message (local submit and/or latest local-user presence from sync).
    // Used as a runtime shadow so UI can reflect updates immediately without waiting for cache
    // echo.
    std::optional<QString> statusMessageShadow_;

    struct PendingSecretsUnlockRequest
    {
        mtx::secret_storage::AesHmacSha2KeyDescription keyDesc;
        SecretsToDecrypt secrets;
    };
    std::optional<PendingSecretsUnlockRequest> pendingSecretsUnlockRequest_;
};
