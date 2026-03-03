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

    Q_PROPERTY(QString uiThemeSlug READ uiThemeSlug WRITE setUiThemeSlug NOTIFY uiThemeSlugChanged)
    Q_PROPERTY(bool timelineMessagesHoverHighlight READ timelineMessagesHoverHighlight WRITE
                 setTimelineMessagesHoverHighlight NOTIFY timelineMessagesHoverHighlightChanged)
    Q_PROPERTY(bool timelineMessagesEmojiOnlyEnlarge READ timelineMessagesEmojiOnlyEnlarge WRITE
                 setTimelineMessagesEmojiOnlyEnlarge NOTIFY timelineMessagesEmojiOnlyEnlargeChanged)
    Q_PROPERTY(
      bool timelineFormattedCodeSyntaxHighlighting READ timelineFormattedCodeSyntaxHighlighting
        WRITE setTimelineFormattedCodeSyntaxHighlighting NOTIFY
          timelineFormattedCodeSyntaxHighlightingChanged)
    Q_PROPERTY(bool integrationsSystemTrayEnabled READ integrationsSystemTrayEnabled WRITE
                 setIntegrationsSystemTrayEnabled NOTIFY integrationsSystemTrayEnabledChanged)
    Q_PROPERTY(bool integrationsSystemTrayAutostart READ integrationsSystemTrayAutostart WRITE
                 setIntegrationsSystemTrayAutostart NOTIFY integrationsSystemTrayAutostartChanged)
    Q_PROPERTY(bool sidebarsCommunitiesVisible READ sidebarsCommunitiesVisible WRITE
                 setSidebarsCommunitiesVisible NOTIFY sidebarsCommunitiesVisibleChanged)
    Q_PROPERTY(
      bool sidebarsRoomListScrollbarsEnabled READ sidebarsRoomListScrollbarsEnabled WRITE
        setSidebarsRoomListScrollbarsEnabled NOTIFY sidebarsRoomListScrollbarsEnabledChanged)
    Q_PROPERTY(
      bool composerInputMarkdownToHtmlEnabled READ composerInputMarkdownToHtmlEnabled WRITE
        setComposerInputMarkdownToHtmlEnabled NOTIFY composerInputMarkdownToHtmlEnabledChanged)
    Q_PROPERTY(SendMessageKey composerInputSendKey READ composerInputSendKey WRITE
                 setComposerInputSendKey NOTIFY composerInputSendKeyChanged)
    Q_PROPERTY(AutoReplaceEmoji composerInputAutoReplaceEmoji READ composerInputAutoReplaceEmoji
                 WRITE setComposerInputAutoReplaceEmoji NOTIFY composerInputAutoReplaceEmojiChanged)
    Q_PROPERTY(TimelineMessagesStyle timelineMessagesStyle READ timelineMessagesStyle WRITE
                 setTimelineMessagesStyle NOTIFY timelineMessagesStyleChanged)
    Q_PROPERTY(
      TimelineMessagesPositioning timelineMessagesPositioning READ timelineMessagesPositioning WRITE
        setTimelineMessagesPositioning NOTIFY timelineMessagesPositioningChanged)
    Q_PROPERTY(
      TimelineUserColorCodingPolicy timelineUserColorCodingPolicy READ timelineUserColorCodingPolicy
        WRITE setTimelineUserColorCodingPolicy NOTIFY timelineUserColorCodingPolicyChanged)
    Q_PROPERTY(
      bool timelineMessagesLayoutSmallAvatars READ timelineMessagesLayoutSmallAvatars WRITE
        setTimelineMessagesLayoutSmallAvatars NOTIFY timelineMessagesLayoutSmallAvatarsChanged)
    Q_PROPERTY(bool composerExtrasStickersEnabled READ composerExtrasStickersEnabled WRITE
                 setComposerExtrasStickersEnabled NOTIFY composerExtrasStickersEnabledChanged)
    Q_PROPERTY(
      bool timelineMessagesLayoutShowOwnAvatar READ timelineMessagesLayoutShowOwnAvatar WRITE
        setTimelineMessagesLayoutShowOwnAvatar NOTIFY timelineMessagesLayoutShowOwnAvatarChanged)
    Q_PROPERTY(
      QString timelineMessageActionsPinnedReactions READ timelineMessageActionsPinnedReactions WRITE
        setTimelineMessageActionsPinnedReactions NOTIFY
          timelineMessageActionsPinnedReactionsChanged)
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
      TimelineMessageActionsActivationPolicy timelineMessageActionsActivationPolicy READ
        timelineMessageActionsActivationPolicy WRITE setTimelineMessageActionsActivationPolicy
          NOTIFY timelineMessageActionsActivationPolicyChanged)
    Q_PROPERTY(bool timelineReadReceiptsEnabled READ timelineReadReceiptsEnabled WRITE
                 setTimelineReadReceiptsEnabled NOTIFY timelineReadReceiptsEnabledChanged)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled
                 NOTIFY notificationsEnabledChanged)
    Q_PROPERTY(bool notificationsAttentionOnIncoming READ notificationsAttentionOnIncoming WRITE
                 setNotificationsAttentionOnIncoming NOTIFY notificationsAttentionOnIncomingChanged)
    Q_PROPERTY(bool uiAvatarsCircular READ uiAvatarsCircular WRITE setUiAvatarsCircular NOTIFY
                 uiAvatarsCircularChanged)
    Q_PROPERTY(NotificationMessageContentPolicy notificationsMessageContentPolicy READ
                 notificationsMessageContentPolicy WRITE setNotificationsMessageContentPolicy NOTIFY
                   notificationsMessageContentPolicyChanged)
    Q_PROPERTY(
      bool sidebarsRoomListShowCommunityCounts READ sidebarsRoomListShowCommunityCounts WRITE
        setSidebarsRoomListShowCommunityCounts NOTIFY sidebarsRoomListShowCommunityCountsChanged)
    Q_PROPERTY(bool uiLayoutCompactMode READ uiLayoutCompactMode WRITE setUiLayoutCompactMode NOTIFY
                 uiLayoutCompactModeChanged)
    Q_PROPERTY(
      bool sidebarsRoomListShowLastMessageTime READ sidebarsRoomListShowLastMessageTime WRITE
        setSidebarsRoomListShowLastMessageTime NOTIFY sidebarsRoomListShowLastMessageTimeChanged)
    Q_PROPERTY(LastMessagePreview sidebarsRoomListLastMessagePreview READ
                 sidebarsRoomListLastMessagePreview WRITE setSidebarsRoomListLastMessagePreview
                   NOTIFY sidebarsRoomListLastMessagePreviewChanged)
    Q_PROPERTY(bool timelineMediaEffectsEnabled READ timelineMediaEffectsEnabled WRITE
                 setTimelineMediaEffectsEnabled NOTIFY timelineMediaEffectsEnabledChanged)
    Q_PROPERTY(bool uiMotionAnimationsEnabled READ uiMotionAnimationsEnabled WRITE
                 setUiMotionAnimationsEnabled NOTIFY uiMotionAnimationsEnabledChanged)
    Q_PROPERTY(bool privacyWindowFocusBlurEnabled READ privacyWindowFocusBlurEnabled WRITE
                 setPrivacyWindowFocusBlurEnabled NOTIFY privacyWindowFocusBlurEnabledChanged)
    Q_PROPERTY(
      int privacyWindowFocusBlurDelaySeconds READ privacyWindowFocusBlurDelaySeconds WRITE
        setPrivacyWindowFocusBlurDelaySeconds NOTIFY privacyWindowFocusBlurDelaySecondsChanged)
    Q_PROPERTY(int uiLayoutContentMaxWidthPx READ uiLayoutContentMaxWidthPx WRITE
                 setUiLayoutContentMaxWidthPx NOTIFY uiLayoutContentMaxWidthPxChanged)
    Q_PROPERTY(int uiLayoutContentMaxWidthEffectivePx READ uiLayoutContentMaxWidthEffectivePx NOTIFY
                 uiLayoutContentMaxWidthPxChanged)
    Q_PROPERTY(
      int uiLayoutContentMaxWidthMinEffectivePx READ uiLayoutContentMaxWidthMinEffectivePx CONSTANT)
    Q_PROPERTY(int sidebarsRoomListWidthPx READ sidebarsRoomListWidthPx WRITE
                 setSidebarsRoomListWidthPx NOTIFY sidebarsRoomListWidthPxChanged)
    Q_PROPERTY(int sidebarsCommunitiesWidthPx READ sidebarsCommunitiesWidthPx WRITE
                 setSidebarsCommunitiesWidthPx NOTIFY sidebarsCommunitiesWidthPxChanged)
    Q_PROPERTY(bool uiInputMode READ uiInputMode WRITE setUiInputMode NOTIFY uiInputModeChanged)
    Q_PROPERTY(bool uiInputTouchSwipeGesturesEnabled READ uiInputTouchSwipeGesturesEnabled WRITE
                 setUiInputTouchSwipeGesturesEnabled NOTIFY uiInputTouchSwipeGesturesEnabledChanged)
    Q_PROPERTY(
      double uiScaleFactor READ uiScaleFactor WRITE setUiScaleFactor NOTIFY uiScaleFactorChanged)
    Q_PROPERTY(
      double uiFontSizePt READ uiFontSizePt WRITE setUiFontSizePt NOTIFY uiFontSizePtChanged)
    Q_PROPERTY(
      QString uiFontFamily READ uiFontFamily WRITE setUiFontFamily NOTIFY uiFontFamilyChanged)
    Q_PROPERTY(QString uiFontEmojiFamily READ uiFontEmojiFamily WRITE setUiFontEmojiFamily NOTIFY
                 uiFontEmojiFamilyChanged)
    Q_PROPERTY(Presence networkPresenceStatusPolicy READ networkPresenceStatusPolicy WRITE
                 setNetworkPresenceStatusPolicy NOTIFY networkPresenceStatusPolicyChanged)
    Q_PROPERTY(ShowImage timelineMediaImageDisplay READ timelineMediaImageDisplay WRITE
                 setTimelineMediaImageDisplay NOTIFY timelineMediaImageDisplayChanged)
    Q_PROPERTY(QString callsAudioRingtone READ callsAudioRingtone WRITE setCallsAudioRingtone NOTIFY
                 callsAudioRingtoneChanged)
    Q_PROPERTY(QString callsDevicesMicrophone READ callsDevicesMicrophone WRITE
                 setCallsDevicesMicrophone NOTIFY callsDevicesMicrophoneChanged)
    Q_PROPERTY(QString callsDevicesCamera READ callsDevicesCamera WRITE setCallsDevicesCamera NOTIFY
                 callsDevicesCameraChanged)
    Q_PROPERTY(QString callsDevicesCameraResolution READ callsDevicesCameraResolution WRITE
                 setCallsDevicesCameraResolution NOTIFY callsDevicesCameraResolutionChanged)
    Q_PROPERTY(QString callsDevicesCameraFrameRate READ callsDevicesCameraFrameRate WRITE
                 setCallsDevicesCameraFrameRate NOTIFY callsDevicesCameraFrameRateChanged)
    Q_PROPERTY(int callsScreenshareFrameRate READ callsScreenshareFrameRate WRITE
                 setCallsScreenshareFrameRate NOTIFY callsScreenshareFrameRateChanged)
    Q_PROPERTY(bool callsScreensharePictureInPicture READ callsScreensharePictureInPicture WRITE
                 setCallsScreensharePictureInPicture NOTIFY callsScreensharePictureInPictureChanged)
    Q_PROPERTY(
      bool callsScreenshareIncludeRemoteVideo READ callsScreenshareIncludeRemoteVideo WRITE
        setCallsScreenshareIncludeRemoteVideo NOTIFY callsScreenshareIncludeRemoteVideoChanged)
    Q_PROPERTY(bool callsScreenshareShowCursor READ callsScreenshareShowCursor WRITE
                 setCallsScreenshareShowCursor NOTIFY callsScreenshareShowCursorChanged)
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
    Q_PROPERTY(bool hasActiveSession READ hasActiveSession NOTIFY sessionAuthStateChanged)
    Q_PROPERTY(bool secretsProviderFallbackWarningVisible READ secretsProviderFallbackWarningVisible
                 NOTIFY secretsProviderFallbackWarningVisibleChanged)
    Q_PROPERTY(bool networkTlsEnableCertificateValidation READ networkTlsEnableCertificateValidation
                 WRITE setNetworkTlsEnableCertificateValidation NOTIFY
                   networkTlsEnableCertificateValidationChanged)
    Q_PROPERTY(bool uiAvatarsIdenticonFallback READ uiAvatarsIdenticonFallback WRITE
                 setUiAvatarsIdenticonFallback NOTIFY uiAvatarsIdenticonFallbackChanged)
    Q_PROPERTY(bool timelineMediaOpenImagesExternal READ timelineMediaOpenImagesExternal WRITE
                 setTimelineMediaOpenImagesExternal NOTIFY timelineMediaOpenImagesExternalChanged)
    Q_PROPERTY(bool timelineMediaOpenVideosExternal READ timelineMediaOpenVideosExternal WRITE
                 setTimelineMediaOpenVideosExternal NOTIFY timelineMediaOpenVideosExternalChanged)
    Q_PROPERTY(QString integrationsBrowserCommand READ integrationsBrowserCommand WRITE
                 setIntegrationsBrowserCommand NOTIFY integrationsBrowserCommandChanged)
    Q_PROPERTY(int integrationsDbusApiAccess READ integrationsDbusApiAccess WRITE
                 setIntegrationsDbusApiAccess NOTIFY integrationsDbusApiAccessChanged)

    Q_PROPERTY(QStringList hiddenPins READ hiddenPins WRITE setHiddenPins NOTIFY hiddenPinsChanged)
    Q_PROPERTY(QStringList recentReactions READ recentReactions WRITE setRecentReactions NOTIFY
                 recentReactionsChanged)
    Q_PROPERTY(QStringList hiddenWidgets READ hiddenWidgets WRITE setHiddenWidgets NOTIFY
                 hiddenWidgetsChanged)
    Q_PROPERTY(
      bool privacyMaintenanceUpdateSpaceVias READ privacyMaintenanceUpdateSpaceVias WRITE
        setPrivacyMaintenanceUpdateSpaceVias NOTIFY privacyMaintenanceUpdateSpaceViasChanged)
    Q_PROPERTY(bool privacyMaintenanceExpireEvents READ privacyMaintenanceExpireEvents WRITE
                 setPrivacyMaintenanceExpireEvents NOTIFY privacyMaintenanceExpireEventsChanged)

    // Window geometry (not exposed to QML, used internally)
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowHeightChanged)

    // Database settings (internal, auto-adjusted)
    Q_PROPERTY(qulonglong dbMaxSizeBytes READ dbMaxSizeBytes WRITE setDbMaxSizeBytes NOTIFY
                 dbMaxSizeBytesChanged)
    Q_PROPERTY(uint dbMaxStores READ dbMaxStores WRITE setDbMaxStores NOTIFY dbMaxStoresChanged)

    // Experimental features
    Q_PROPERTY(bool networkHttp3Enabled READ networkHttp3Enabled WRITE setNetworkHttp3Enabled NOTIFY
                 networkHttp3EnabledChanged)

    UserSettings();

public:
    enum class LoadPolicy
    {
        Full,
        ConfigAndStateOnly,
    };

    static QSharedPointer<UserSettings> instance();
    static void
    initialize(std::optional<QString> profile, LoadPolicy loadPolicy = LoadPolicy::Full);
    static void initialize(std::optional<QString> profile,
                           const YAML::Node &configRoot,
                           LoadPolicy loadPolicy = LoadPolicy::Full);
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

    enum class TimelineMessageActionsActivationPolicy
    {
        OnHover,
        ActionsButton,
        Never,
    };
    Q_ENUM(TimelineMessageActionsActivationPolicy)

    enum class TimelineMessagesStyle
    {
        Plain,
        Bubbles,
    };
    Q_ENUM(TimelineMessagesStyle)

    enum class TimelineMessagesPositioning
    {
        OpposingBySender,
        AllLeft,
        AllRight,
    };
    Q_ENUM(TimelineMessagesPositioning)

    enum class TimelineUserColorCodingPolicy
    {
        AdaptiveByRoomSize,
        MeVsOthers,
    };
    Q_ENUM(TimelineUserColorCodingPolicy)

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
    void load(std::optional<QString> profile, LoadPolicy loadPolicy = LoadPolicy::Full);
    void load(std::optional<QString> profile,
              const YAML::Node &configRoot,
              LoadPolicy loadPolicy = LoadPolicy::Full);
    void applyTheme();
    void setUiThemeSlug(QString theme);
    void setTimelineMessagesHoverHighlight(bool state);
    void setTimelineMessagesEmojiOnlyEnlarge(bool state);
    void setTimelineFormattedCodeSyntaxHighlighting(bool state);
    void setIntegrationsSystemTrayEnabled(bool state);
    void setIntegrationsSystemTrayAutostart(bool state);
    void setUiInputMode(bool mode);
    void setUiInputTouchSwipeGesturesEnabled(bool mode);
    void setUiScaleFactor(double factor);
    void setUiFontSizePt(double size);
    void setUiFontFamily(QString family);
    void setUiFontEmojiFamily(QString family);
    void setSidebarsCommunitiesVisible(bool state);
    void setSidebarsRoomListScrollbarsEnabled(bool state);
    void setComposerInputMarkdownToHtmlEnabled(bool state);
    void setComposerInputSendKey(SendMessageKey key);
    void setComposerInputAutoReplaceEmoji(AutoReplaceEmoji state);
    void setTimelineMessagesStyle(TimelineMessagesStyle style);
    void setTimelineMessagesPositioning(TimelineMessagesPositioning positioning);
    void setTimelineUserColorCodingPolicy(TimelineUserColorCodingPolicy policy);
    void setTimelineMessagesLayoutSmallAvatars(bool state);
    void setComposerExtrasStickersEnabled(bool state);
    void setTimelineMessagesLayoutShowOwnAvatar(bool state);
    void setTimelineMessageActionsPinnedReactions(QString value);
    void setTimelineMessagesSenderUsername(ShowSenderUsername state);
    void setTimelineMediaAnimateOnHover(bool state);
    void setTimelineReadReceiptsEnabled(bool state);
    void setComposerTypingSendEnabled(bool state);
    void setTimelineTypingShowEnabled(bool state);
    void setSidebarsRoomListSort(RoomSortOrder order);
    void setTimelineMessageActionsActivationPolicy(TimelineMessageActionsActivationPolicy policy);
    void setUiLayoutContentMaxWidthPx(int state);
    void setSidebarsCommunitiesWidthPx(int state);
    void setSidebarsRoomListWidthPx(int state);
    void setNotificationsEnabled(bool state);
    void setNotificationsAttentionOnIncoming(bool state);
    void setUiAvatarsCircular(bool state);
    void setNotificationsMessageContentPolicy(NotificationMessageContentPolicy policy);
    void setSidebarsRoomListShowCommunityCounts(bool state);
    void setUiLayoutCompactMode(bool state);
    void setSidebarsRoomListShowLastMessageTime(bool state);
    void setSidebarsRoomListLastMessagePreview(LastMessagePreview style);
    void setTimelineMediaEffectsEnabled(bool state);
    void setUiMotionAnimationsEnabled(bool state);
    void setPrivacyWindowFocusBlurEnabled(bool state);
    void setPrivacyWindowFocusBlurDelaySeconds(int state);
    void setNetworkPresenceStatusPolicy(Presence state);
    void setTimelineMediaImageDisplay(ShowImage state);
    void setCallsAudioRingtone(QString callsAudioRingtone);
    void setCallsDevicesMicrophone(QString callsDevicesMicrophone);
    void setCallsDevicesCamera(QString callsDevicesCamera);
    void setCallsDevicesCameraResolution(QString resolution);
    void setCallsDevicesCameraFrameRate(QString frameRate);
    void setCallsScreenshareFrameRate(int frameRate);
    void setCallsScreensharePictureInPicture(bool state);
    void setCallsScreenshareIncludeRemoteVideo(bool state);
    void setCallsScreenshareShowCursor(bool state);
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
    void setCurrentRoomId(QString currentRoomId);
    void setHomeserver(QString homeserver);
    void setNetworkTlsEnableCertificateValidation(bool enabled);
    void setHiddenTags(const QStringList &hiddenTags);
    void setMutedTags(const QStringList &mutedTags);
    void setHiddenPins(const QStringList &hiddenTags);
    void setHiddenWidgets(const QStringList &hiddenTags);
    void setRecentReactions(QStringList recent);
    void setComposerDraftsByRoom(const QMap<QString, QString> &draftsByRoom);
    void setComposerDraftForRoom(const QString &roomId, const QString &draftText);
    void clearComposerDraftForRoom(const QString &roomId);
    void clearAllComposerDrafts();
    void setUiAvatarsIdenticonFallback(bool state);
    void setTimelineMediaOpenImagesExternal(bool state);
    void setTimelineMediaOpenVideosExternal(bool state);
    void setIntegrationsBrowserCommand(QString command);
    void setCollapsedSpaces(QList<QStringList> spaces);
    void setIntegrationsDbusApiAccess(int access);
    void setPrivacyMaintenanceUpdateSpaceVias(bool state);
    void setPrivacyMaintenanceExpireEvents(bool state);
    void setWindowWidth(int width);
    void setWindowHeight(int height);
    void setDbMaxSizeBytes(qulonglong size);
    void setDbMaxStores(uint count);
    void setNetworkHttp3Enabled(bool state);
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
    void setSecretsProviderFallbackWarningVisible(bool visible);
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
    void sidebarsCommunitiesVisibleChanged(bool state);
    void sidebarsRoomListScrollbarsEnabledChanged(bool state);
    void sidebarsRoomListSortChanged(RoomSortOrder order);
    void uiThemeSlugChanged(QString state);
    void timelineMessagesHoverHighlightChanged(bool state);
    void timelineMessagesEmojiOnlyEnlargeChanged(bool state);
    void timelineFormattedCodeSyntaxHighlightingChanged(bool state);
    void integrationsSystemTrayEnabledChanged(bool state);
    void integrationsSystemTrayAutostartChanged(bool state);
    void composerInputMarkdownToHtmlEnabledChanged(bool state);
    void composerInputSendKeyChanged(SendMessageKey key);
    void composerInputAutoReplaceEmojiChanged(AutoReplaceEmoji state);
    void timelineMessagesStyleChanged(TimelineMessagesStyle style);
    void timelineMessagesPositioningChanged(TimelineMessagesPositioning positioning);
    void timelineUserColorCodingPolicyChanged(TimelineUserColorCodingPolicy policy);
    void timelineMessagesLayoutSmallAvatarsChanged(bool state);
    void composerExtrasStickersEnabledChanged(bool state);
    void timelineMessagesLayoutShowOwnAvatarChanged(bool state);
    void timelineMessageActionsPinnedReactionsChanged(const QString &value);
    void timelineMessagesSenderUsernameChanged(ShowSenderUsername state);
    void timelineMediaAnimateOnHoverChanged(bool state);
    void composerTypingSendEnabledChanged(bool state);
    void timelineTypingShowEnabledChanged(bool state);
    void
    timelineMessageActionsActivationPolicyChanged(TimelineMessageActionsActivationPolicy policy);
    void timelineReadReceiptsEnabledChanged(bool state);
    void notificationsEnabledChanged(bool state);
    void notificationsAttentionOnIncomingChanged(bool state);
    void uiAvatarsCircularChanged(bool state);
    void notificationsMessageContentPolicyChanged(NotificationMessageContentPolicy policy);
    void sidebarsRoomListShowCommunityCountsChanged(bool state);
    void uiLayoutCompactModeChanged(bool state);
    void sidebarsRoomListShowLastMessageTimeChanged(bool state);
    void sidebarsRoomListLastMessagePreviewChanged(LastMessagePreview style);
    void timelineMediaEffectsEnabledChanged(bool state);
    void uiMotionAnimationsEnabledChanged(bool state);
    void privacyWindowFocusBlurEnabledChanged(bool state);
    void privacyWindowFocusBlurDelaySecondsChanged(int state);
    void uiLayoutContentMaxWidthPxChanged(int state);
    void sidebarsRoomListWidthPxChanged(int state);
    void sidebarsCommunitiesWidthPxChanged(int state);
    void uiInputModeChanged(bool mode);
    void uiInputTouchSwipeGesturesEnabledChanged(bool state);
    void uiScaleFactorChanged(double factor);
    void uiFontSizePtChanged(double state);
    void uiFontFamilyChanged(QString state);
    void uiFontEmojiFamilyChanged(QString state);
    void networkPresenceStatusPolicyChanged(Presence state);
    void timelineMediaImageDisplayChanged(ShowImage state);
    void callsAudioRingtoneChanged(QString callsAudioRingtone);
    void callsDevicesMicrophoneChanged(QString callsDevicesMicrophone);
    void callsDevicesCameraChanged(QString callsDevicesCamera);
    void callsDevicesCameraResolutionChanged(QString resolution);
    void callsDevicesCameraFrameRateChanged(QString frameRate);
    void callsScreenshareFrameRateChanged(int frameRate);
    void callsScreensharePictureInPictureChanged(bool state);
    void callsScreenshareIncludeRemoteVideoChanged(bool state);
    void callsScreenshareShowCursorChanged(bool state);
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
    void networkTlsEnableCertificateValidationChanged(bool enabled);
    void uiAvatarsIdenticonFallbackChanged(bool state);
    void timelineMediaOpenImagesExternalChanged(bool state);
    void timelineMediaOpenVideosExternalChanged(bool state);
    void hiddenPinsChanged();
    void hiddenWidgetsChanged();
    void recentReactionsChanged();
    void integrationsDbusApiAccessChanged(int state);
    void integrationsBrowserCommandChanged(QString command);
    void privacyMaintenanceUpdateSpaceViasChanged(bool state);
    void privacyMaintenanceExpireEventsChanged(bool state);
    void windowWidthChanged(int width);
    void windowHeightChanged(int height);
    void dbMaxSizeBytesChanged(qulonglong size);
    void dbMaxStoresChanged(uint count);
    void networkHttp3EnabledChanged(bool state);
    void secretsProviderFallbackWarningVisibleChanged(bool visible);
    void sessionAuthStateChanged();

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
    void emitSessionAuthStateChangedIfNeeded(bool hadPersistedSessionIdentity,
                                             bool hadActiveSessionState);

#include "settings/ui/facade/UserSettingsPagePrivateMembers.h"
};

#include "settings/ui/UserSettingsModel.h"
