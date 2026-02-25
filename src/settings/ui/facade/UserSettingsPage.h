// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QProcessEnvironment>
#include <QQmlEngine>
#include <QSharedPointer>

#include <optional>
#include <string>

#include "settings/core/SettingsDefinitions.h"
#include "settings/core/SettingsStore.h"

namespace YAML {
class Node;
}

namespace settings {
class SettingsController;
}

/**
 * UserSettings is the runtime settings model exposed to QML.
 *
 * It owns the in-memory state and emits change notifications for UI components.
 * It does not handle policy/transport details (for example profile path
 * resolution, staged loads, or secure storage layout); those concerns are now
 * orchestrated through settings::SettingsController.
 */
class UserSettings final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    QML_NAMED_ELEMENT(Settings)
    QML_SINGLETON

    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool timelineMessagesHoverHighlight READ timelineMessagesHoverHighlight WRITE
                 setTimelineMessagesHoverHighlight NOTIFY timelineMessagesHoverHighlightChanged)
    Q_PROPERTY(bool timelineMessagesEmojiOnlyEnlarge READ timelineMessagesEmojiOnlyEnlarge WRITE
                 setTimelineMessagesEmojiOnlyEnlarge NOTIFY timelineMessagesEmojiOnlyEnlargeChanged)
    Q_PROPERTY(bool integrationsSystemTrayEnabled READ integrationsSystemTrayEnabled WRITE
                 setIntegrationsSystemTrayEnabled NOTIFY integrationsSystemTrayEnabledChanged)
    Q_PROPERTY(bool integrationsSystemTrayAutostart READ integrationsSystemTrayAutostart WRITE
                 setIntegrationsSystemTrayAutostart NOTIFY integrationsSystemTrayAutostartChanged)
    Q_PROPERTY(bool communitiesSidebarVisible READ communitiesSidebarVisible WRITE
                 setCommunitiesSidebarVisible NOTIFY communitiesSidebarVisibleChanged)
    Q_PROPERTY(bool roomListScrollbarsVisible READ roomListScrollbarsVisible WRITE
                 setRoomListScrollbarsVisible NOTIFY roomListScrollbarsVisibleChanged)
    Q_PROPERTY(bool markdownEnabled READ markdownEnabled WRITE setMarkdownEnabled NOTIFY
                 markdownEnabledChanged)
    Q_PROPERTY(SendMessageKey sendMessageKey READ sendMessageKey WRITE setSendMessageKey NOTIFY
                 sendMessageKeyChanged)
    Q_PROPERTY(AutoReplaceEmoji autoReplaceEmoji READ autoReplaceEmoji WRITE setAutoReplaceEmoji
                 NOTIFY autoReplaceEmojiChanged)
    Q_PROPERTY(TimelineMessageLayout timelineMessageLayout READ timelineMessageLayout WRITE
                 setTimelineMessageLayout NOTIFY timelineMessageLayoutChanged)
    Q_PROPERTY(bool timelineSmallAvatarsEnabled READ timelineSmallAvatarsEnabled WRITE
                 setTimelineSmallAvatarsEnabled NOTIFY timelineSmallAvatarsEnabledChanged)
    Q_PROPERTY(bool stickersEnabled READ stickersEnabled WRITE setStickersEnabled NOTIFY
                 stickersEnabledChanged)
    Q_PROPERTY(
      bool timelineShowOwnAvatarInBubbleLayout READ timelineShowOwnAvatarInBubbleLayout WRITE
        setTimelineShowOwnAvatarInBubbleLayout NOTIFY timelineShowOwnAvatarInBubbleLayoutChanged)
    Q_PROPERTY(QString pinnedReactions READ pinnedReactions WRITE setPinnedReactions NOTIFY
                 pinnedReactionsChanged)
    Q_PROPERTY(
      ShowSenderUsername timelineMessagesSenderUsername READ timelineMessagesSenderUsername WRITE
        setTimelineMessagesSenderUsername NOTIFY timelineMessagesSenderUsernameChanged)
    Q_PROPERTY(int timelineMessagesSenderUsernameLargeRoomThreshold READ
                 timelineMessagesSenderUsernameLargeRoomThreshold CONSTANT)
    Q_PROPERTY(bool timelineMediaAnimateOnHover READ timelineMediaAnimateOnHover WRITE
                 setTimelineMediaAnimateOnHover NOTIFY timelineMediaAnimateOnHoverChanged)
    Q_PROPERTY(bool composerTypingSendEnabled READ composerTypingSendEnabled WRITE
                 setComposerTypingSendEnabled NOTIFY composerTypingSendEnabledChanged)
    Q_PROPERTY(bool timelineTypingShowEnabled READ timelineTypingShowEnabled WRITE
                 setTimelineTypingShowEnabled NOTIFY timelineTypingShowEnabledChanged)
    Q_PROPERTY(RoomSortOrder sidebarsRoomListSort READ sidebarsRoomListSort WRITE
                 setSidebarsRoomListSort NOTIFY sidebarsRoomListSortChanged)
    Q_PROPERTY(
      TimelineMessageActionsPolicy timelineMessageActionsPolicy READ timelineMessageActionsPolicy
        WRITE setTimelineMessageActionsPolicy NOTIFY timelineMessageActionsPolicyChanged)
    Q_PROPERTY(bool timelineReadReceiptsEnabled READ timelineReadReceiptsEnabled WRITE
                 setTimelineReadReceiptsEnabled NOTIFY timelineReadReceiptsEnabledChanged)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled
                 NOTIFY notificationsEnabledChanged)
    Q_PROPERTY(bool notificationsAttentionOnIncoming READ notificationsAttentionOnIncoming WRITE
                 setNotificationsAttentionOnIncoming NOTIFY notificationsAttentionOnIncomingChanged)
    Q_PROPERTY(bool circularAvatarsEnabled READ circularAvatarsEnabled WRITE
                 setCircularAvatarsEnabled NOTIFY circularAvatarsEnabledChanged)
    Q_PROPERTY(NotificationMessageContentPolicy notificationMessageContentPolicy READ
                 notificationMessageContentPolicy WRITE setNotificationMessageContentPolicy NOTIFY
                   notificationMessageContentPolicyChanged)
    Q_PROPERTY(
      bool communityNotificationCountsVisible READ communityNotificationCountsVisible WRITE
        setCommunityNotificationCountsVisible NOTIFY communityNotificationCountsVisibleChanged)
    Q_PROPERTY(bool compactRoomList READ compactRoomList WRITE setCompactRoomList NOTIFY
                 compactRoomListChanged)
    Q_PROPERTY(bool roomListShowLastMessageTime READ roomListShowLastMessageTime WRITE
                 setRoomListShowLastMessageTime NOTIFY roomListShowLastMessageTimeChanged)
    Q_PROPERTY(LastMessagePreview sidebarsRoomListLastMessagePreview READ
                 sidebarsRoomListLastMessagePreview WRITE setSidebarsRoomListLastMessagePreview
                   NOTIFY sidebarsRoomListLastMessagePreviewChanged)
    Q_PROPERTY(bool timelineMediaEffectsEnabled READ timelineMediaEffectsEnabled WRITE
                 setTimelineMediaEffectsEnabled NOTIFY timelineMediaEffectsEnabledChanged)
    Q_PROPERTY(bool uiMotionAnimationsEnabled READ uiMotionAnimationsEnabled WRITE
                 setUiMotionAnimationsEnabled NOTIFY uiMotionAnimationsEnabledChanged)
    Q_PROPERTY(bool windowFocusBlurEnabled READ windowFocusBlurEnabled WRITE
                 setWindowFocusBlurEnabled NOTIFY windowFocusBlurEnabledChanged)
    Q_PROPERTY(int windowFocusBlurDelaySeconds READ windowFocusBlurDelaySeconds WRITE
                 setWindowFocusBlurDelaySeconds NOTIFY windowFocusBlurDelaySecondsChanged)
    Q_PROPERTY(int maxContentWidth READ maxContentWidth WRITE setMaxContentWidth NOTIFY
                 maxContentWidthChanged)
    Q_PROPERTY(int maxTimelineWidth READ maxTimelineWidth WRITE setMaxTimelineWidth NOTIFY
                 maxTimelineWidthChanged)
    Q_PROPERTY(
      int roomListWidth READ roomListWidth WRITE setRoomListWidth NOTIFY roomListWidthChanged)
    Q_PROPERTY(int communityListWidth READ communityListWidth WRITE setCommunityListWidth NOTIFY
                 communityListWidthChanged)
    Q_PROPERTY(bool touchInputModeEnabled READ touchInputModeEnabled WRITE setTouchInputModeEnabled
                 NOTIFY touchInputModeEnabledChanged)
    Q_PROPERTY(bool uiInputTouchSwipeGesturesEnabled READ uiInputTouchSwipeGesturesEnabled WRITE
                 setUiInputTouchSwipeGesturesEnabled NOTIFY uiInputTouchSwipeGesturesEnabledChanged)
    Q_PROPERTY(double scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY scaleFactorChanged)
    Q_PROPERTY(double fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(QString font READ font WRITE setFontFamily NOTIFY fontChanged)
    Q_PROPERTY(QString emojiFont READ emojiFont WRITE setEmojiFontFamily NOTIFY emojiFontChanged)
    Q_PROPERTY(Presence presence READ presence WRITE setPresence NOTIFY presenceChanged)
    Q_PROPERTY(ShowImage showImage READ showImage WRITE setShowImage NOTIFY showImageChanged)
    Q_PROPERTY(QString ringtone READ ringtone WRITE setRingtone NOTIFY ringtoneChanged)
    Q_PROPERTY(QString microphone READ microphone WRITE setMicrophone NOTIFY microphoneChanged)
    Q_PROPERTY(QString camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(QString cameraResolution READ cameraResolution WRITE setCameraResolution NOTIFY
                 cameraResolutionChanged)
    Q_PROPERTY(QString cameraFrameRate READ cameraFrameRate WRITE setCameraFrameRate NOTIFY
                 cameraFrameRateChanged)
    Q_PROPERTY(int screenShareFrameRate READ screenShareFrameRate WRITE setScreenShareFrameRate
                 NOTIFY screenShareFrameRateChanged)
    Q_PROPERTY(
      bool screenSharePiP READ screenSharePiP WRITE setScreenSharePiP NOTIFY screenSharePiPChanged)
    Q_PROPERTY(bool screenShareRemoteVideo READ screenShareRemoteVideo WRITE
                 setScreenShareRemoteVideo NOTIFY screenShareRemoteVideoChanged)
    Q_PROPERTY(bool screenShareShowCursor READ screenShareShowCursor WRITE setScreenShareShowCursor
                 NOTIFY screenShareShowCursorChanged)
    Q_PROPERTY(bool callsRelayUseFallbackServer READ callsRelayUseFallbackServer WRITE
                 setCallsRelayUseFallbackServer NOTIFY callsRelayUseFallbackServerChanged)
    Q_PROPERTY(bool callsLegacyEnabled READ callsLegacyEnabled WRITE setCallsLegacyEnabled NOTIFY
                 callsLegacyEnabledChanged)
    Q_PROPERTY(bool encryptionKeySharingOnlyVerifiedUsers READ encryptionKeySharingOnlyVerifiedUsers
                 WRITE setEncryptionKeySharingOnlyVerifiedUsers NOTIFY
                   encryptionKeySharingOnlyVerifiedUsersChanged)
    Q_PROPERTY(
      bool encryptionKeySharingShareWithTrusted READ encryptionKeySharingShareWithTrusted WRITE
        setEncryptionKeySharingShareWithTrusted NOTIFY encryptionKeySharingShareWithTrustedChanged)
    Q_PROPERTY(bool encryptionBackupOnlineEnabled READ encryptionBackupOnlineEnabled WRITE
                 setEncryptionBackupOnlineEnabled NOTIFY encryptionBackupOnlineEnabledChanged)
    Q_PROPERTY(QString profile READ profile WRITE setProfile NOTIFY profileChanged)
    Q_PROPERTY(QString userId READ userId WRITE setUserId NOTIFY userIdChanged)
    Q_PROPERTY(QString accessToken READ accessToken WRITE setAccessToken NOTIFY accessTokenChanged)
    Q_PROPERTY(QString deviceId READ deviceId WRITE setDeviceId NOTIFY deviceIdChanged)
    Q_PROPERTY(QString homeserver READ homeserver WRITE setHomeserver NOTIFY homeserverChanged)
    Q_PROPERTY(bool certificateValidationEnabled READ certificateValidationEnabled WRITE
                 setCertificateValidationEnabled NOTIFY certificateValidationEnabledChanged)
    Q_PROPERTY(bool identiconFallbackEnabled READ identiconFallbackEnabled WRITE
                 setIdenticonFallbackEnabled NOTIFY identiconFallbackEnabledChanged)
    Q_PROPERTY(bool openImagesInExternalApp READ openImagesInExternalApp WRITE
                 setOpenImagesInExternalApp NOTIFY openImagesInExternalAppChanged)
    Q_PROPERTY(bool openVideosInExternalApp READ openVideosInExternalApp WRITE
                 setOpenVideosInExternalApp NOTIFY openVideosInExternalAppChanged)
    Q_PROPERTY(QString integrationsLinksBrowserCommand READ integrationsLinksBrowserCommand WRITE
                 setIntegrationsLinksBrowserCommand NOTIFY integrationsLinksBrowserCommandChanged)
    Q_PROPERTY(int integrationsDbusApiAccess READ integrationsDbusApiAccess WRITE
                 setIntegrationsDbusApiAccess NOTIFY integrationsDbusApiAccessChanged)

    Q_PROPERTY(QStringList hiddenPins READ hiddenPins WRITE setHiddenPins NOTIFY hiddenPinsChanged)
    Q_PROPERTY(QStringList recentReactions READ recentReactions WRITE setRecentReactions NOTIFY
                 recentReactionsChanged)
    Q_PROPERTY(QStringList hiddenWidgets READ hiddenWidgets WRITE setHiddenWidgets NOTIFY
                 hiddenWidgetsChanged)
    Q_PROPERTY(bool updateSpaceVias READ updateSpaceVias WRITE setUpdateSpaceVias NOTIFY
                 updateSpaceViasChanged)
    Q_PROPERTY(bool expireEvents READ expireEvents WRITE setExpireEvents NOTIFY expireEventsChanged)

    // Window geometry (not exposed to QML, used internally)
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowHeightChanged)

    // Database settings (internal, auto-adjusted)
    Q_PROPERTY(qulonglong maxDbSize READ maxDbSize WRITE setMaxDbSize NOTIFY maxDbSizeChanged)
    Q_PROPERTY(uint maxStores READ maxStores WRITE setMaxStores NOTIFY maxStoresChanged)

    // Experimental features
    Q_PROPERTY(bool http3Enabled READ http3Enabled WRITE setHttp3Enabled NOTIFY http3EnabledChanged)

    UserSettings();

public:
    static QSharedPointer<UserSettings> instance();
    static void initialize(std::optional<QString> profile);
    static void initialize(std::optional<QString> profile, const YAML::Node &configRoot);
    static UserSettings *create(QQmlEngine *qmlEngine, QJSEngine *)
    {
        // The instance has to exist before it is used. We cannot replace it.
        Q_ASSERT(instance());

        // The engine has to have the same thread affinity as the singleton.
        Q_ASSERT(qmlEngine->thread() == instance()->thread());

        // There can only be one engine accessing the singleton.
        static QJSEngine *s_engine = nullptr;
        if (s_engine)
            Q_ASSERT(qmlEngine == s_engine);
        else
            s_engine = qmlEngine;

        QJSEngine::setObjectOwnership(instance().get(), QJSEngine::CppOwnership);
        return instance().get();
    }

    enum class Presence
    {
        AutomaticPresence,
        Online,
        Unavailable,
        Offline,
    };
    Q_ENUM(Presence)

    enum class ShowImage
    {
        Always,
        OnlyPrivate,
        Never,
    };
    Q_ENUM(ShowImage)

    enum class ShowSenderUsername
    {
        Always,
        OnlyInLargeRooms,
        Never,
    };
    Q_ENUM(ShowSenderUsername)

    enum class AutoReplaceEmoji
    {
        Always,
        OnlyAtEnd,
        Never,
    };
    Q_ENUM(AutoReplaceEmoji)

    enum class SendMessageKey
    {
        Enter,
        ShiftEnter,
        CtrlEnter,
    };
    Q_ENUM(SendMessageKey)

    enum class TimelineMessageActionsPolicy
    {
        OnHover,
        ActionsButton,
        Never,
    };
    Q_ENUM(TimelineMessageActionsPolicy)

    enum class TimelineMessageLayout
    {
        Minimal,
        Bubbles,
    };
    Q_ENUM(TimelineMessageLayout)

    enum class RoomSortOrder
    {
        UnreadFirst_Recent, // Unread first, then by recent activity
        UnreadFirst_Alpha,  // Unread first, then alphabetically
        Recent,             // By recent activity only
        Alphabetical,       // Alphabetically only
    };
    Q_ENUM(RoomSortOrder)

    enum class LastMessagePreview
    {
        Always,          // Always show message previews
        OnlyUnencrypted, // Only show in unencrypted rooms
        Never,           // Never show message previews
    };
    Q_ENUM(LastMessagePreview)

    enum class NotificationMessageContentPolicy
    {
        Never,
        UnencryptedOnly,
        WheneverAvailable,
    };
    Q_ENUM(NotificationMessageContentPolicy)

    struct SessionSnapshot
    {
        QString userId;
        QString accessToken;
        QString deviceId;
        QString homeserver;
    };

    void save();
    void load(std::optional<QString> profile);
    void load(std::optional<QString> profile, const YAML::Node &configRoot);
    void applyTheme();
    void setTheme(QString theme);
    void setTimelineMessagesHoverHighlight(bool state);
    void setTimelineMessagesEmojiOnlyEnlarge(bool state);
    void setIntegrationsSystemTrayEnabled(bool state);
    void setIntegrationsSystemTrayAutostart(bool state);
    void setTouchInputModeEnabled(bool mode);
    void setUiInputTouchSwipeGesturesEnabled(bool mode);
    void setScaleFactor(double factor);
    void setFontSize(double size);
    void setFontFamily(QString family);
    void setEmojiFontFamily(QString family);
    void setCommunitiesSidebarVisible(bool state);
    void setRoomListScrollbarsVisible(bool state);
    void setMarkdownEnabled(bool state);
    void setSendMessageKey(SendMessageKey key);
    void setAutoReplaceEmoji(AutoReplaceEmoji state);
    void setTimelineMessageLayout(TimelineMessageLayout layout);
    void setTimelineSmallAvatarsEnabled(bool state);
    void setStickersEnabled(bool state);
    void setTimelineShowOwnAvatarInBubbleLayout(bool state);
    void setPinnedReactions(QString value);
    void setTimelineMessagesSenderUsername(ShowSenderUsername state);
    void setTimelineMediaAnimateOnHover(bool state);
    void setTimelineReadReceiptsEnabled(bool state);
    void setComposerTypingSendEnabled(bool state);
    void setTimelineTypingShowEnabled(bool state);
    void setSidebarsRoomListSort(RoomSortOrder order);
    void setTimelineMessageActionsPolicy(TimelineMessageActionsPolicy policy);
    void setMaxContentWidth(int state);
    void setMaxTimelineWidth(int state);
    void setCommunityListWidth(int state);
    void setRoomListWidth(int state);
    void setNotificationsEnabled(bool state);
    void setNotificationsAttentionOnIncoming(bool state);
    void setCircularAvatarsEnabled(bool state);
    void setNotificationMessageContentPolicy(NotificationMessageContentPolicy policy);
    void setCommunityNotificationCountsVisible(bool state);
    void setCompactRoomList(bool state);
    void setRoomListShowLastMessageTime(bool state);
    void setSidebarsRoomListLastMessagePreview(LastMessagePreview style);
    void setTimelineMediaEffectsEnabled(bool state);
    void setUiMotionAnimationsEnabled(bool state);
    void setWindowFocusBlurEnabled(bool state);
    void setWindowFocusBlurDelaySeconds(int state);
    void setPresence(Presence state);
    void setShowImage(ShowImage state);
    void setRingtone(QString ringtone);
    void setMicrophone(QString microphone);
    void setCamera(QString camera);
    void setCameraResolution(QString resolution);
    void setCameraFrameRate(QString frameRate);
    void setScreenShareFrameRate(int frameRate);
    void setScreenSharePiP(bool state);
    void setScreenShareRemoteVideo(bool state);
    void setScreenShareShowCursor(bool state);
    void setCallsRelayUseFallbackServer(bool state);
    void setCallsLegacyEnabled(bool state);
    void setEncryptionKeySharingOnlyVerifiedUsers(bool state);
    void setEncryptionKeySharingShareWithTrusted(bool state);
    void setEncryptionBackupOnlineEnabled(bool state);
    void setEncryptionBackupOnlineEnabledFromConfig(bool state);
    void setProfile(QString profile);
    void setUserId(QString userId);
    void setAccessToken(QString accessToken);
    void setDeviceId(QString deviceId);
    void setCurrentTagId(QString currentTagId);
    void setHomeserver(QString homeserver);
    void setCertificateValidationEnabled(bool enabled);
    void setHiddenTags(const QStringList &hiddenTags);
    void setMutedTags(const QStringList &mutedTags);
    void setHiddenPins(const QStringList &hiddenTags);
    void setHiddenWidgets(const QStringList &hiddenTags);
    void setRecentReactions(QStringList recent);
    void setIdenticonFallbackEnabled(bool state);
    void setOpenImagesInExternalApp(bool state);
    void setOpenVideosInExternalApp(bool state);
    void setIntegrationsLinksBrowserCommand(QString command);
    void setCollapsedSpaces(QList<QStringList> spaces);
    void setIntegrationsDbusApiAccess(int access);
    void setUpdateSpaceVias(bool state);
    void setExpireEvents(bool state);
    void setWindowWidth(int width);
    void setWindowHeight(int height);
    void setMaxDbSize(qulonglong size);
    void setMaxStores(uint count);
    void setHttp3Enabled(bool state);
    void clearAuth();
    bool hasPersistedSessionIdentity() const;
    bool hasActiveSession() const;
    SessionSnapshot sessionSnapshot() const;
    // Persist full auth/session material even if fields are unchanged in memory.
    // This keeps file/keychain storage repaired after partial deletion/corruption.
    bool persistSessionSnapshot(const SessionSnapshot &snapshot);
    // Load session identity fields from persisted storage without triggering settings save.
    void setSessionSnapshot(const SessionSnapshot &snapshot);
    void applyLoadedSecrets(const QString &accessToken, const QMap<QString, QString> &secrets);
    void clearAuthInMemory();
    void notifyProfileChanged();
    void setUsesFileSecretsProvider(bool usesFileSecretsProvider);
    [[nodiscard]] bool hasResolvedProfilePaths() const;
    [[nodiscard]] const QString &profileId() const;
    [[nodiscard]] const QString &profileDirPath() const;
    [[nodiscard]] const QString &configFilePath() const;
    [[nodiscard]] const QString &stateFilePath() const;
    [[nodiscard]] const QString &sessionFilePath() const;
    [[nodiscard]] const QString &secretsFilePath() const;
    [[nodiscard]] const QMap<QString, QString> &secretsMap() const;

    // Secrets storage helpers (for fallback mode)
    QString secret(const QString &name) const;
    void setSecret(const QString &name, const QString &value);
    void removeSecret(const QString &name);
    void setPersistenceSuspended(bool suspended);
    // Internal settings lifecycle hooks used by SettingsController.
    void applyProfilePathState(const QString &profile);
    void setPersistenceScopeReadyForAuth(bool ready);

    // Theme helpers for QML (used on the Welcome page)
    Q_INVOKABLE int themeVariantIndex() const;
    Q_INVOKABLE void setThemeVariantByIndex(int index);
    Q_INVOKABLE QStringList themeNamesForCurrentVariant() const;
    Q_INVOKABLE int themeIndexInCurrentVariant() const;
    Q_INVOKABLE void setThemeByVariantIndex(int index);

#include "settings/ui/facade/UserSettingsGetters.inc"

signals:
    void communitiesSidebarVisibleChanged(bool state);
    void roomListScrollbarsVisibleChanged(bool state);
    void sidebarsRoomListSortChanged(RoomSortOrder order);
    void themeChanged(QString state);
    void timelineMessagesHoverHighlightChanged(bool state);
    void timelineMessagesEmojiOnlyEnlargeChanged(bool state);
    void integrationsSystemTrayEnabledChanged(bool state);
    void integrationsSystemTrayAutostartChanged(bool state);
    void markdownEnabledChanged(bool state);
    void sendMessageKeyChanged(SendMessageKey key);
    void autoReplaceEmojiChanged(AutoReplaceEmoji state);
    void timelineMessageLayoutChanged(TimelineMessageLayout layout);
    void timelineSmallAvatarsEnabledChanged(bool state);
    void stickersEnabledChanged(bool state);
    void timelineShowOwnAvatarInBubbleLayoutChanged(bool state);
    void pinnedReactionsChanged(const QString &value);
    void timelineMessagesSenderUsernameChanged(ShowSenderUsername state);
    void timelineMediaAnimateOnHoverChanged(bool state);
    void composerTypingSendEnabledChanged(bool state);
    void timelineTypingShowEnabledChanged(bool state);
    void timelineMessageActionsPolicyChanged(TimelineMessageActionsPolicy policy);
    void timelineReadReceiptsEnabledChanged(bool state);
    void notificationsEnabledChanged(bool state);
    void notificationsAttentionOnIncomingChanged(bool state);
    void circularAvatarsEnabledChanged(bool state);
    void notificationMessageContentPolicyChanged(NotificationMessageContentPolicy policy);
    void communityNotificationCountsVisibleChanged(bool state);
    void compactRoomListChanged(bool state);
    void roomListShowLastMessageTimeChanged(bool state);
    void sidebarsRoomListLastMessagePreviewChanged(LastMessagePreview style);
    void timelineMediaEffectsEnabledChanged(bool state);
    void uiMotionAnimationsEnabledChanged(bool state);
    void windowFocusBlurEnabledChanged(bool state);
    void windowFocusBlurDelaySecondsChanged(int state);
    void maxContentWidthChanged(int state);
    void maxTimelineWidthChanged(int state);
    void roomListWidthChanged(int state);
    void communityListWidthChanged(int state);
    void touchInputModeEnabledChanged(bool mode);
    void uiInputTouchSwipeGesturesEnabledChanged(bool state);
    void scaleFactorChanged(double factor);
    void fontSizeChanged(double state);
    void fontChanged(QString state);
    void emojiFontChanged(QString state);
    void presenceChanged(Presence state);
    void showImageChanged(ShowImage state);
    void ringtoneChanged(QString ringtone);
    void microphoneChanged(QString microphone);
    void cameraChanged(QString camera);
    void cameraResolutionChanged(QString resolution);
    void cameraFrameRateChanged(QString frameRate);
    void screenShareFrameRateChanged(int frameRate);
    void screenSharePiPChanged(bool state);
    void screenShareRemoteVideoChanged(bool state);
    void screenShareShowCursorChanged(bool state);
    void callsRelayUseFallbackServerChanged(bool state);
    void callsLegacyEnabledChanged(bool state);
    void encryptionKeySharingOnlyVerifiedUsersChanged(bool state);
    void encryptionKeySharingShareWithTrustedChanged(bool state);
    void encryptionBackupOnlineEnabledChanged(bool state);
    void profileChanged(QString profile);
    void userIdChanged(QString userId);
    void accessTokenChanged(QString accessToken);
    void deviceIdChanged(QString deviceId);
    void homeserverChanged(QString homeserver);
    void certificateValidationEnabledChanged(bool enabled);
    void identiconFallbackEnabledChanged(bool state);
    void openImagesInExternalAppChanged(bool state);
    void openVideosInExternalAppChanged(bool state);
    void hiddenPinsChanged();
    void hiddenWidgetsChanged();
    void recentReactionsChanged();
    void integrationsDbusApiAccessChanged(int state);
    void integrationsLinksBrowserCommandChanged(QString command);
    void updateSpaceViasChanged(bool state);
    void expireEventsChanged(bool state);
    void windowWidthChanged(int width);
    void windowHeightChanged(int height);
    void maxDbSizeChanged(qulonglong size);
    void maxStoresChanged(uint count);
    void http3EnabledChanged(bool state);

private:
    template<typename T, typename Signal>
    void setSetting(T &member, const T &value, Signal signal)
    {
        if (member == value)
            return;
        member = value;
        emit(this->*signal)(value);
        save();
    }
    bool setCoreValue(settings::core::SettingId id,
                      settings::core::SettingsStore::Value value,
                      const char *settingName);

#include "settings/ui/facade/UserSettingsPagePrivateMembers.h"
};

#include "settings/ui/UserSettingsModel.h"
