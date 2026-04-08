// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QProcessEnvironment>
#include <QQmlEngine>
#include <QSharedPointer>
#include <QTimer>

#include "komai-rust-cxxbridge/ffi.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "settings/core/SettingsDefinitions.h"
#include "settings/core/SettingsStore.h"

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
    Q_PROPERTY(bool desktopSystemTrayEnabled READ desktopSystemTrayEnabled WRITE
                 setDesktopSystemTrayEnabled NOTIFY desktopSystemTrayEnabledChanged)
    Q_PROPERTY(bool desktopSystemTrayAutostart READ desktopSystemTrayAutostart WRITE
                 setDesktopSystemTrayAutostart NOTIFY desktopSystemTrayAutostartChanged)
    Q_PROPERTY(bool sidebarsCommunitiesVisible READ sidebarsCommunitiesVisible WRITE
                 setSidebarsCommunitiesVisible NOTIFY sidebarsCommunitiesVisibleChanged)
    Q_PROPERTY(
      bool sidebarsCommunitiesFilterFavourites READ sidebarsCommunitiesFilterFavourites WRITE
        setSidebarsCommunitiesFilterFavourites NOTIFY sidebarsCommunitiesFilterFavouritesChanged)
    Q_PROPERTY(bool sidebarsCommunitiesFilterPeople READ sidebarsCommunitiesFilterPeople WRITE
                 setSidebarsCommunitiesFilterPeople NOTIFY sidebarsCommunitiesFilterPeopleChanged)
    Q_PROPERTY(bool sidebarsCommunitiesFilterBots READ sidebarsCommunitiesFilterBots WRITE
                 setSidebarsCommunitiesFilterBots NOTIFY sidebarsCommunitiesFilterBotsChanged)
    Q_PROPERTY(
      bool sidebarsCommunitiesFilterServerNotices READ sidebarsCommunitiesFilterServerNotices WRITE
        setSidebarsCommunitiesFilterServerNotices NOTIFY
          sidebarsCommunitiesFilterServerNoticesChanged)
    Q_PROPERTY(bool sidebarsCommunitiesFilterGroups READ sidebarsCommunitiesFilterGroups WRITE
                 setSidebarsCommunitiesFilterGroups NOTIFY sidebarsCommunitiesFilterGroupsChanged)
    Q_PROPERTY(
      bool sidebarsCommunitiesFilterLowPriority READ sidebarsCommunitiesFilterLowPriority WRITE
        setSidebarsCommunitiesFilterLowPriority NOTIFY sidebarsCommunitiesFilterLowPriorityChanged)
    Q_PROPERTY(ScrollbarPolicy uiScrollbarPolicy READ uiScrollbarPolicy WRITE setUiScrollbarPolicy
                 NOTIFY uiScrollbarPolicyChanged)
    Q_PROPERTY(
      bool composerInputMarkdownToHtmlEnabled READ composerInputMarkdownToHtmlEnabled WRITE
        setComposerInputMarkdownToHtmlEnabled NOTIFY composerInputMarkdownToHtmlEnabledChanged)
    Q_PROPERTY(SendMessageKey composerInputSendKey READ composerInputSendKey WRITE
                 setComposerInputSendKey NOTIFY composerInputSendKeyChanged)
    Q_PROPERTY(AutoReplaceEmoji composerInputAutoReplaceEmoji READ composerInputAutoReplaceEmoji
                 WRITE setComposerInputAutoReplaceEmoji NOTIFY composerInputAutoReplaceEmojiChanged)
    Q_PROPERTY(
      EmojiPreferredGender composerInputEmojiPreferredGender READ composerInputEmojiPreferredGender
        WRITE setComposerInputEmojiPreferredGender NOTIFY composerInputEmojiPreferredGenderChanged)
    Q_PROPERTY(EmojiPreferredSkinTone composerInputEmojiPreferredSkinTone READ
                 composerInputEmojiPreferredSkinTone WRITE setComposerInputEmojiPreferredSkinTone
                   NOTIFY composerInputEmojiPreferredSkinToneChanged)
    Q_PROPERTY(bool composerInputInlineEmojiPickerEnabled READ composerInputInlineEmojiPickerEnabled
                 WRITE setComposerInputInlineEmojiPickerEnabled NOTIFY
                   composerInputInlineEmojiPickerEnabledChanged)
    Q_PROPERTY(
      bool composerInputInlineRoomPickerEnabled READ composerInputInlineRoomPickerEnabled WRITE
        setComposerInputInlineRoomPickerEnabled NOTIFY composerInputInlineRoomPickerEnabledChanged)
    Q_PROPERTY(
      bool composerInputInlineUserPickerEnabled READ composerInputInlineUserPickerEnabled WRITE
        setComposerInputInlineUserPickerEnabled NOTIFY composerInputInlineUserPickerEnabledChanged)
    Q_PROPERTY(TimelineMessagesStyle timelineMessagesStyle READ timelineMessagesStyle WRITE
                 setTimelineMessagesStyle NOTIFY timelineMessagesStyleChanged)
    Q_PROPERTY(
      TimelineMessagesPositioning timelineMessagesPositioning READ timelineMessagesPositioning WRITE
        setTimelineMessagesPositioning NOTIFY timelineMessagesPositioningChanged)
    Q_PROPERTY(
      TimelineUserColorCodingPolicy timelineUserColorCodingPolicy READ timelineUserColorCodingPolicy
        WRITE setTimelineUserColorCodingPolicy NOTIFY timelineUserColorCodingPolicyChanged)
    Q_PROPERTY(
      AvatarSize timelineMessagesLayoutAvatarSize READ timelineMessagesLayoutAvatarSize WRITE
        setTimelineMessagesLayoutAvatarSize NOTIFY timelineMessagesLayoutAvatarSizeChanged)
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
      UnreadDetectionPolicy sidebarsRoomListUnreadDetectionPolicy READ
        sidebarsRoomListUnreadDetectionPolicy WRITE setSidebarsRoomListUnreadDetectionPolicy NOTIFY
          sidebarsRoomListUnreadDetectionPolicyChanged)
    Q_PROPERTY(
      TimelineMessageActionsActivationPolicy timelineMessageActionsActivationPolicy READ
        timelineMessageActionsActivationPolicy WRITE setTimelineMessageActionsActivationPolicy
          NOTIFY timelineMessageActionsActivationPolicyChanged)
    Q_PROPERTY(bool timelineReadReceiptsEnabled READ timelineReadReceiptsEnabled WRITE
                 setTimelineReadReceiptsEnabled NOTIFY timelineReadReceiptsEnabledChanged)
    Q_PROPERTY(bool desktopNotificationsEnabled READ desktopNotificationsEnabled WRITE
                 setDesktopNotificationsEnabled NOTIFY desktopNotificationsEnabledChanged)
    Q_PROPERTY(bool notificationsAccountEnabled READ notificationsAccountEnabled WRITE
                 setNotificationsAccountEnabled NOTIFY notificationsAccountEnabledChanged)
    Q_PROPERTY(
      bool desktopNotificationsAttentionOnIncoming READ desktopNotificationsAttentionOnIncoming
        WRITE setDesktopNotificationsAttentionOnIncoming NOTIFY
          desktopNotificationsAttentionOnIncomingChanged)
    Q_PROPERTY(
      bool desktopAttentionWindowTitleEnabled READ desktopAttentionWindowTitleEnabled WRITE
        setDesktopAttentionWindowTitleEnabled NOTIFY desktopAttentionWindowTitleEnabledChanged)
    Q_PROPERTY(bool desktopAttentionAppBadgeEnabled READ desktopAttentionAppBadgeEnabled WRITE
                 setDesktopAttentionAppBadgeEnabled NOTIFY desktopAttentionAppBadgeEnabledChanged)
    Q_PROPERTY(bool uiAvatarsCircular READ uiAvatarsCircular WRITE setUiAvatarsCircular NOTIFY
                 uiAvatarsCircularChanged)
    Q_PROPERTY(
      NotificationMessageContentPolicy desktopNotificationsMessageContentPolicy READ
        desktopNotificationsMessageContentPolicy WRITE setDesktopNotificationsMessageContentPolicy
          NOTIFY desktopNotificationsMessageContentPolicyChanged)
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
    Q_PROPERTY(bool desktopWindowFocusBlurEnabled READ desktopWindowFocusBlurEnabled WRITE
                 setDesktopWindowFocusBlurEnabled NOTIFY desktopWindowFocusBlurEnabledChanged)
    Q_PROPERTY(
      int desktopWindowFocusBlurDelaySeconds READ desktopWindowFocusBlurDelaySeconds WRITE
        setDesktopWindowFocusBlurDelaySeconds NOTIFY desktopWindowFocusBlurDelaySecondsChanged)
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
    Q_PROPERTY(DefaultAvatarStyle uiAvatarsDefaultAvatarStyle READ uiAvatarsDefaultAvatarStyle WRITE
                 setUiAvatarsDefaultAvatarStyle NOTIFY uiAvatarsDefaultAvatarStyleChanged)
    Q_PROPERTY(bool timelineMediaOpenImagesExternal READ timelineMediaOpenImagesExternal WRITE
                 setTimelineMediaOpenImagesExternal NOTIFY timelineMediaOpenImagesExternalChanged)
    Q_PROPERTY(bool timelineMediaOpenVideosExternal READ timelineMediaOpenVideosExternal WRITE
                 setTimelineMediaOpenVideosExternal NOTIFY timelineMediaOpenVideosExternalChanged)
    Q_PROPERTY(bool timelineMediaAutoplayGifVideos READ timelineMediaAutoplayGifVideos WRITE
                 setTimelineMediaAutoplayGifVideos NOTIFY timelineMediaAutoplayGifVideosChanged)
    Q_PROPERTY(bool timelineMediaOpenAudioExternal READ timelineMediaOpenAudioExternal WRITE
                 setTimelineMediaOpenAudioExternal NOTIFY timelineMediaOpenAudioExternalChanged)
    Q_PROPERTY(
      double timelineMediaDefaultAudioPlaybackSpeed READ timelineMediaDefaultAudioPlaybackSpeed
        WRITE setTimelineMediaDefaultAudioPlaybackSpeed NOTIFY
          timelineMediaDefaultAudioPlaybackSpeedChanged)
    Q_PROPERTY(QString integrationsBrowserCommand READ integrationsBrowserCommand WRITE
                 setIntegrationsBrowserCommand NOTIFY integrationsBrowserCommandChanged)
    Q_PROPERTY(int integrationsDbusApiAccess READ integrationsDbusApiAccess WRITE
                 setIntegrationsDbusApiAccess NOTIFY integrationsDbusApiAccessChanged)

    Q_PROPERTY(QStringList hiddenPins READ hiddenPins WRITE setHiddenPins NOTIFY hiddenPinsChanged)
    Q_PROPERTY(QStringList hiddenWidgets READ hiddenWidgets WRITE setHiddenWidgets NOTIFY
                 hiddenWidgetsChanged)
    Q_PROPERTY(QString donationStatus READ donationStatus WRITE setDonationStatus NOTIFY
                 donationStatusChanged)
    // Window geometry (not exposed to QML, used internally)
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowHeightChanged)

    // Matrix Rooms Search
    Q_PROPERTY(bool networkMrsEnabled READ networkMrsEnabled WRITE setNetworkMrsEnabled NOTIFY
                 networkMrsEnabledChanged)
    Q_PROPERTY(QString networkMrsServerName READ networkMrsServerName WRITE setNetworkMrsServerName
                 NOTIFY networkMrsServerNameChanged)

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

    using NotificationsAccountHandleProvider = std::function<std::uint64_t()>;
    using NotificationsAccountFetchFn =
      std::function<std::optional<bool>(std::uint64_t, QString *)>;
    using NotificationsAccountSetFn = std::function<bool(std::uint64_t, bool, QString *)>;

    void setNotificationsAccountRuntimeHooks(NotificationsAccountHandleProvider handleProvider,
                                             NotificationsAccountFetchFn fetchFn,
                                             NotificationsAccountSetFn setFn);

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

    enum class AvatarSize
    {
        Regular,
        Small,
        Hidden,
    };
    Q_ENUM(AvatarSize)

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

    enum class EmojiPreferredGender
    {
        NoPreference,
        Man,
        Woman,
    };
    Q_ENUM(EmojiPreferredGender)

    enum class EmojiPreferredSkinTone
    {
        NoPreference,
        Light,
        MediumLight,
        Medium,
        MediumDark,
        Dark,
    };
    Q_ENUM(EmojiPreferredSkinTone)

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

    enum class UnreadDetectionPolicy
    {
        AnyEvent,     // Any timeline event marks a room unread
        MessagesOnly, // Only message events mark a room unread
    };
    Q_ENUM(UnreadDetectionPolicy)

    enum class ScrollbarPolicy
    {
        WhenNeeded, // Show scrollbars only when content overflows
        Never,      // Never show scrollbars
        Always,     // Always show scrollbars
    };
    Q_ENUM(ScrollbarPolicy)

    enum class DefaultAvatarStyle
    {
        BoringAvatarsBauhaus,
        BoringAvatarsBeam,
        BoringAvatarsMarble,
        LetterInitial,
        UserIcon,
    };
    Q_ENUM(DefaultAvatarStyle)

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
    void applyTheme();
    void setUiThemeSlug(QString theme);
    void setTimelineMessagesHoverHighlight(bool state);
    void setTimelineMessagesEmojiOnlyEnlarge(bool state);
    void setTimelineFormattedCodeSyntaxHighlighting(bool state);
    void setDesktopSystemTrayEnabled(bool state);
    void setDesktopSystemTrayAutostart(bool state);
    void setUiInputMode(bool mode);
    void setUiInputTouchSwipeGesturesEnabled(bool mode);
    void setUiScaleFactor(double factor);
    void setUiFontSizePt(double size);
    void setUiFontFamily(QString family);
    void setUiFontEmojiFamily(QString family);
    void setSidebarsCommunitiesVisible(bool state);
    void setSidebarsCommunitiesFilterFavourites(bool state);
    void setSidebarsCommunitiesFilterPeople(bool state);
    void setSidebarsCommunitiesFilterBots(bool state);
    void setSidebarsCommunitiesFilterGroups(bool state);
    void setSidebarsCommunitiesFilterServerNotices(bool state);
    void setSidebarsCommunitiesFilterLowPriority(bool state);
    void setUiScrollbarPolicy(ScrollbarPolicy policy);
    void setComposerInputMarkdownToHtmlEnabled(bool state);
    void setComposerInputSendKey(SendMessageKey key);
    void setComposerInputAutoReplaceEmoji(AutoReplaceEmoji state);
    void setComposerInputEmojiPreferredGender(EmojiPreferredGender state);
    void setComposerInputEmojiPreferredSkinTone(EmojiPreferredSkinTone state);
    void setComposerInputInlineEmojiPickerEnabled(bool state);
    void setComposerInputInlineRoomPickerEnabled(bool state);
    void setComposerInputInlineUserPickerEnabled(bool state);
    void setTimelineMessagesStyle(TimelineMessagesStyle style);
    void setTimelineMessagesPositioning(TimelineMessagesPositioning positioning);
    void setTimelineUserColorCodingPolicy(TimelineUserColorCodingPolicy policy);
    void setTimelineMessagesLayoutAvatarSize(AvatarSize size);
    void setComposerExtrasStickersEnabled(bool state);
    void setTimelineMessagesLayoutShowOwnAvatar(bool state);
    void setTimelineMessageActionsPinnedReactions(QString value);
    void setTimelineMessagesSenderUsername(ShowSenderUsername state);
    void setTimelineMediaAnimateOnHover(bool state);
    void setTimelineReadReceiptsEnabled(bool state);
    void setComposerTypingSendEnabled(bool state);
    void setTimelineTypingShowEnabled(bool state);
    void setSidebarsRoomListSort(RoomSortOrder order);
    void setSidebarsRoomListUnreadDetectionPolicy(UnreadDetectionPolicy policy);
    void setTimelineMessageActionsActivationPolicy(TimelineMessageActionsActivationPolicy policy);
    void setUiLayoutContentMaxWidthPx(int state);
    void setSidebarsCommunitiesWidthPx(int state);
    void setSidebarsRoomListWidthPx(int state);
    void setNotificationsAccountEnabled(bool state);
    void setDesktopNotificationsEnabled(bool state);
    void setDesktopNotificationsAttentionOnIncoming(bool state);
    void setDesktopAttentionWindowTitleEnabled(bool state);
    void setDesktopAttentionAppBadgeEnabled(bool state);
    void setUiAvatarsCircular(bool state);
    void setDesktopNotificationsMessageContentPolicy(NotificationMessageContentPolicy policy);
    void setSidebarsRoomListShowCommunityCounts(bool state);
    void setUiLayoutCompactMode(bool state);
    void setSidebarsRoomListShowLastMessageTime(bool state);
    void setSidebarsRoomListLastMessagePreview(LastMessagePreview style);
    void setTimelineMediaEffectsEnabled(bool state);
    void setUiMotionAnimationsEnabled(bool state);
    void setDesktopWindowFocusBlurEnabled(bool state);
    void setDesktopWindowFocusBlurDelaySeconds(int state);
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
    void setCurrentFilterId(QString currentFilterId);
    void setCurrentRoomId(QString currentRoomId);
    void setHomeserver(QString homeserver);
    void setNetworkTlsEnableCertificateValidation(bool enabled);
    void setGlobalExcludes(const QStringList &globalExcludes);
    void setBadgesHiddenFilters(const QStringList &badgesHiddenFilters);
    void setHiddenPins(const QStringList &hiddenTags);
    void setHiddenWidgets(const QStringList &hiddenTags);
    void setHiddenTimelineEventTypes(const QStringList &eventTypes);
    void setHiddenTimelineEventTypesByRoom(const QMap<QString, QStringList> &eventTypesByRoom);
    void setHiddenTimelineEventTypesForRoom(const QString &roomId, const QStringList &eventTypes);
    void setComposerDraftsByRoom(const QMap<QString, QString> &draftsByRoom);
    void setComposerDraftForRoom(const QString &roomId, const QString &draftText);
    void clearComposerDraftForRoom(const QString &roomId);
    void clearAllComposerDrafts();
    void setUiAvatarsDefaultAvatarStyle(DefaultAvatarStyle style);
    void setTimelineMediaOpenImagesExternal(bool state);
    void setTimelineMediaOpenVideosExternal(bool state);
    void setTimelineMediaAutoplayGifVideos(bool state);
    void setTimelineMediaOpenAudioExternal(bool state);
    void setTimelineMediaDefaultAudioPlaybackSpeed(double speed);
    void setIntegrationsBrowserCommand(QString command);
    void setCollapsedSpaces(QStringList spaces);
    void setIntegrationsDbusApiAccess(int access);
    void setDonationStatus(QString status);
    void setWindowWidth(int width);
    void setWindowHeight(int height);
    void setNetworkMrsEnabled(bool state);
    void setNetworkMrsServerName(QString serverName);
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
    void scheduleDeferredStateSave();
    void flushDeferredStateSave();
    // Internal settings lifecycle hooks used by SettingsController.
    void applyProfilePathState(const QString &profile);
    void setPersistenceScopeReadyForAuth(bool ready);
    void setRustSettingsProfileHandle(::rust::Box<::komai::rust::SettingsProfileHandle> handle);
    void clearRustSettingsProfileHandle();
    [[nodiscard]] bool hasRustSettingsProfileHandle() const;
    [[nodiscard]] ::komai::rust::SettingsProfileHandle *rustSettingsProfileHandle();

    // Theme helpers for QML (used on the Welcome page)
    Q_INVOKABLE int themeVariantIndex() const;
    Q_INVOKABLE void setThemeVariantByIndex(int index);
    Q_INVOKABLE QStringList themeNamesForCurrentVariant() const;
    Q_INVOKABLE int themeIndexInCurrentVariant() const;
    Q_INVOKABLE void setThemeByVariantIndex(int index);

#include "settings/ui/facade/UserSettingsGetters.inc"

signals:
    void sidebarsCommunitiesVisibleChanged(bool state);
    void sidebarsCommunitiesFilterFavouritesChanged(bool state);
    void sidebarsCommunitiesFilterPeopleChanged(bool state);
    void sidebarsCommunitiesFilterBotsChanged(bool state);
    void sidebarsCommunitiesFilterGroupsChanged(bool state);
    void sidebarsCommunitiesFilterServerNoticesChanged(bool state);
    void sidebarsCommunitiesFilterLowPriorityChanged(bool state);
    void uiScrollbarPolicyChanged(ScrollbarPolicy policy);
    void sidebarsRoomListSortChanged(RoomSortOrder order);
    void sidebarsRoomListUnreadDetectionPolicyChanged(UnreadDetectionPolicy policy);
    void uiThemeSlugChanged(QString state);
    void timelineMessagesHoverHighlightChanged(bool state);
    void timelineMessagesEmojiOnlyEnlargeChanged(bool state);
    void timelineFormattedCodeSyntaxHighlightingChanged(bool state);
    void desktopSystemTrayEnabledChanged(bool state);
    void desktopSystemTrayAutostartChanged(bool state);
    void composerInputMarkdownToHtmlEnabledChanged(bool state);
    void composerInputSendKeyChanged(SendMessageKey key);
    void composerInputAutoReplaceEmojiChanged(AutoReplaceEmoji state);
    void composerInputEmojiPreferredGenderChanged(EmojiPreferredGender state);
    void composerInputEmojiPreferredSkinToneChanged(EmojiPreferredSkinTone state);
    void composerInputInlineEmojiPickerEnabledChanged(bool state);
    void composerInputInlineRoomPickerEnabledChanged(bool state);
    void composerInputInlineUserPickerEnabledChanged(bool state);
    void timelineMessagesStyleChanged(TimelineMessagesStyle style);
    void timelineMessagesPositioningChanged(TimelineMessagesPositioning positioning);
    void timelineUserColorCodingPolicyChanged(TimelineUserColorCodingPolicy policy);
    void timelineMessagesLayoutAvatarSizeChanged(AvatarSize size);
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
    void desktopNotificationsEnabledChanged(bool state);
    void desktopNotificationsAttentionOnIncomingChanged(bool state);
    void desktopAttentionWindowTitleEnabledChanged(bool state);
    void desktopAttentionAppBadgeEnabledChanged(bool state);
    void uiAvatarsCircularChanged(bool state);
    void desktopNotificationsMessageContentPolicyChanged(NotificationMessageContentPolicy policy);
    void sidebarsRoomListShowCommunityCountsChanged(bool state);
    void uiLayoutCompactModeChanged(bool state);
    void sidebarsRoomListShowLastMessageTimeChanged(bool state);
    void sidebarsRoomListLastMessagePreviewChanged(LastMessagePreview style);
    void timelineMediaEffectsEnabledChanged(bool state);
    void uiMotionAnimationsEnabledChanged(bool state);
    void desktopWindowFocusBlurEnabledChanged(bool state);
    void desktopWindowFocusBlurDelaySecondsChanged(int state);
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
    void uiAvatarsDefaultAvatarStyleChanged(DefaultAvatarStyle style);
    void timelineMediaOpenImagesExternalChanged(bool state);
    void timelineMediaOpenVideosExternalChanged(bool state);
    void timelineMediaAutoplayGifVideosChanged(bool state);
    void timelineMediaOpenAudioExternalChanged(bool state);
    void timelineMediaDefaultAudioPlaybackSpeedChanged(double speed);
    void hiddenPinsChanged();
    void globalExcludesChanged();
    void hiddenWidgetsChanged();
    void hiddenTimelineEventTypesChanged();
    void composerDraftsByRoomChanged();
    void integrationsDbusApiAccessChanged(int state);
    void integrationsBrowserCommandChanged(QString command);
    void donationStatusChanged(QString donationStatus);
    void windowWidthChanged(int width);
    void windowHeightChanged(int height);
    void networkMrsEnabledChanged(bool state);
    void networkMrsServerNameChanged(QString serverName);
    void networkHttp3EnabledChanged(bool state);
    void notificationsAccountEnabledChanged(bool state);
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
