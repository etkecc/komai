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
    Q_PROPERTY(
      ThemeMode uiThemeMode READ uiThemeMode WRITE setUiThemeMode NOTIFY uiThemeModeChanged)
    Q_PROPERTY(bool timelineMessagesHoverHighlight READ timelineMessagesHoverHighlight WRITE
                 setTimelineMessagesHoverHighlight NOTIFY timelineMessagesHoverHighlightChanged)
    Q_PROPERTY(bool timelineMessagesDragSelect READ timelineMessagesDragSelect WRITE
                 setTimelineMessagesDragSelect NOTIFY timelineMessagesDragSelectChanged)
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
    Q_PROPERTY(DesktopSystemTrayIconStyle desktopSystemTrayIconStyle READ desktopSystemTrayIconStyle
                 WRITE setDesktopSystemTrayIconStyle NOTIFY desktopSystemTrayIconStyleChanged)
    Q_PROPERTY(
      bool desktopSystemTrayFirstClosePrompted READ desktopSystemTrayFirstClosePrompted WRITE
        setDesktopSystemTrayFirstClosePrompted NOTIFY desktopSystemTrayFirstClosePromptedChanged)
    Q_PROPERTY(bool navigationCommunitiesFilterFavourites READ navigationCommunitiesFilterFavourites
                 WRITE setNavigationCommunitiesFilterFavourites NOTIFY
                   navigationCommunitiesFilterFavouritesChanged)
    Q_PROPERTY(
      bool navigationCommunitiesFilterPeople READ navigationCommunitiesFilterPeople WRITE
        setNavigationCommunitiesFilterPeople NOTIFY navigationCommunitiesFilterPeopleChanged)
    Q_PROPERTY(bool navigationCommunitiesFilterBots READ navigationCommunitiesFilterBots WRITE
                 setNavigationCommunitiesFilterBots NOTIFY navigationCommunitiesFilterBotsChanged)
    Q_PROPERTY(
      bool navigationCommunitiesFilterServerNotices READ navigationCommunitiesFilterServerNotices
        WRITE setNavigationCommunitiesFilterServerNotices NOTIFY
          navigationCommunitiesFilterServerNoticesChanged)
    Q_PROPERTY(
      bool navigationCommunitiesFilterGroups READ navigationCommunitiesFilterGroups WRITE
        setNavigationCommunitiesFilterGroups NOTIFY navigationCommunitiesFilterGroupsChanged)
    Q_PROPERTY(
      bool navigationCommunitiesFilterLowPriority READ navigationCommunitiesFilterLowPriority WRITE
        setNavigationCommunitiesFilterLowPriority NOTIFY
          navigationCommunitiesFilterLowPriorityChanged)
    Q_PROPERTY(ScrollbarPolicy uiScrollbarPolicy READ uiScrollbarPolicy WRITE setUiScrollbarPolicy
                 NOTIFY uiScrollbarPolicyChanged)
    Q_PROPERTY(
      RoomListOpeningPolicy navigationRoomListOpeningPolicy READ navigationRoomListOpeningPolicy
        WRITE setNavigationRoomListOpeningPolicy NOTIFY navigationRoomListOpeningPolicyChanged)
    Q_PROPERTY(bool navigationTabsAutoHideSingle READ navigationTabsAutoHideSingle WRITE
                 setNavigationTabsAutoHideSingle NOTIFY navigationTabsAutoHideSingleChanged)
    Q_PROPERTY(TabPinButtonVisibility navigationTabsShowPinButton READ navigationTabsShowPinButton
                 WRITE setNavigationTabsShowPinButton NOTIFY navigationTabsShowPinButtonChanged)
    Q_PROPERTY(TabLabelDisplay navigationTabsPinnedTabLabel READ navigationTabsPinnedTabLabel WRITE
                 setNavigationTabsPinnedTabLabel NOTIFY navigationTabsPinnedTabLabelChanged)
    Q_PROPERTY(TabLabelDisplay navigationTabsTabLabel READ navigationTabsTabLabel WRITE
                 setNavigationTabsTabLabel NOTIFY navigationTabsTabLabelChanged)
    Q_PROPERTY(int navigationTabsPreferredWidthPx READ navigationTabsPreferredWidthPx WRITE
                 setNavigationTabsPreferredWidthPx NOTIFY navigationTabsPreferredWidthPxChanged)
    Q_PROPERTY(int navigationTabsMinimumWidthPx READ navigationTabsMinimumWidthPx WRITE
                 setNavigationTabsMinimumWidthPx NOTIFY navigationTabsMinimumWidthPxChanged)
    Q_PROPERTY(
      int navigationTabsMaxRecentlyClosedTimelines READ navigationTabsMaxRecentlyClosedTimelines
        WRITE setNavigationTabsMaxRecentlyClosedTimelines NOTIFY
          navigationTabsMaxRecentlyClosedTimelinesChanged)
    Q_PROPERTY(
      bool composerInputMarkdownToHtmlEnabled READ composerInputMarkdownToHtmlEnabled WRITE
        setComposerInputMarkdownToHtmlEnabled NOTIFY composerInputMarkdownToHtmlEnabledChanged)
    Q_PROPERTY(SendMessageKey composerInputSendKey READ composerInputSendKey WRITE
                 setComposerInputSendKey NOTIFY composerInputSendKeyChanged)
    Q_PROPERTY(QString composerInputSendKeyLabel READ composerInputSendKeyLabel NOTIFY
                 composerInputSendKeyLabelChanged)
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
    Q_PROPERTY(bool composerInputSelectionFormattingToolbarEnabled READ
                 composerInputSelectionFormattingToolbarEnabled WRITE
                   setComposerInputSelectionFormattingToolbarEnabled NOTIFY
                     composerInputSelectionFormattingToolbarEnabledChanged)
    Q_PROPERTY(
      bool composerInputTranscriptionEnabled READ composerInputTranscriptionEnabled WRITE
        setComposerInputTranscriptionEnabled NOTIFY composerInputTranscriptionEnabledChanged)
    Q_PROPERTY(bool composerInputSpellcheckEnabled READ composerInputSpellcheckEnabled WRITE
                 setComposerInputSpellcheckEnabled NOTIFY composerInputSpellcheckEnabledChanged)
    Q_PROPERTY(
      QStringList composerInputSpellcheckLanguages READ composerInputSpellcheckLanguages WRITE
        setComposerInputSpellcheckLanguages NOTIFY composerInputSpellcheckLanguagesChanged)
    Q_PROPERTY(bool composerAttachmentsStripImageMetadata READ composerAttachmentsStripImageMetadata
                 WRITE setComposerAttachmentsStripImageMetadata NOTIFY
                   composerAttachmentsStripImageMetadataChanged)
    Q_PROPERTY(TimelineMessagesStyle timelineMessagesStyle READ timelineMessagesStyle WRITE
                 setTimelineMessagesStyle NOTIFY timelineMessagesStyleChanged)
    Q_PROPERTY(
      RoomHeaderButtonLabels timelineRoomHeaderButtonLabels READ timelineRoomHeaderButtonLabels
        WRITE setTimelineRoomHeaderButtonLabels NOTIFY timelineRoomHeaderButtonLabelsChanged)
    Q_PROPERTY(TimelineMessagesLayoutPositioning timelineMessagesLayoutPositioning READ
                 timelineMessagesLayoutPositioning WRITE setTimelineMessagesLayoutPositioning NOTIFY
                   timelineMessagesLayoutPositioningChanged)
    Q_PROPERTY(
      TimelineUserColorCodingPolicy timelineUserColorCodingPolicy READ timelineUserColorCodingPolicy
        WRITE setTimelineUserColorCodingPolicy NOTIFY timelineUserColorCodingPolicyChanged)
    Q_PROPERTY(
      AvatarSize timelineMessagesLayoutAvatarSize READ timelineMessagesLayoutAvatarSize WRITE
        setTimelineMessagesLayoutAvatarSize NOTIFY timelineMessagesLayoutAvatarSizeChanged)
    Q_PROPERTY(
      bool timelineMessagesLayoutShowOwnAvatar READ timelineMessagesLayoutShowOwnAvatar WRITE
        setTimelineMessagesLayoutShowOwnAvatar NOTIFY timelineMessagesLayoutShowOwnAvatarChanged)
    Q_PROPERTY(int timelineMessagesLayoutMaxWidthPercent READ timelineMessagesLayoutMaxWidthPercent
                 WRITE setTimelineMessagesLayoutMaxWidthPercent NOTIFY
                   timelineMessagesLayoutMaxWidthPercentChanged)
    Q_PROPERTY(int timelineMessagesLayoutAdaptivePositioningBreakpointPx READ
                 timelineMessagesLayoutAdaptivePositioningBreakpointPx WRITE
                   setTimelineMessagesLayoutAdaptivePositioningBreakpointPx NOTIFY
                     timelineMessagesLayoutAdaptivePositioningBreakpointPxChanged)
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
    Q_PROPERTY(RoomSortOrder navigationRoomListSort READ navigationRoomListSort WRITE
                 setNavigationRoomListSort NOTIFY navigationRoomListSortChanged)
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
      bool navigationRoomListShowUnreadIndicators READ navigationRoomListShowUnreadIndicators WRITE
        setNavigationRoomListShowUnreadIndicators NOTIFY
          navigationRoomListShowUnreadIndicatorsChanged)
    Q_PROPERTY(
      bool navigationCommunitiesShowUnreadIndicators READ navigationCommunitiesShowUnreadIndicators
        WRITE setNavigationCommunitiesShowUnreadIndicators NOTIFY
          navigationCommunitiesShowUnreadIndicatorsChanged)
    Q_PROPERTY(Density uiLayoutDensity READ uiLayoutDensity WRITE setUiLayoutDensity NOTIFY
                 uiLayoutDensityChanged)
    Q_PROPERTY(bool navigationRoomListShowLastMessageTime READ navigationRoomListShowLastMessageTime
                 WRITE setNavigationRoomListShowLastMessageTime NOTIFY
                   navigationRoomListShowLastMessageTimeChanged)
    Q_PROPERTY(LastMessagePreview navigationRoomListLastMessagePreview READ
                 navigationRoomListLastMessagePreview WRITE setNavigationRoomListLastMessagePreview
                   NOTIFY navigationRoomListLastMessagePreviewChanged)
    Q_PROPERTY(bool timelineMediaEffectsEnabled READ timelineMediaEffectsEnabled WRITE
                 setTimelineMediaEffectsEnabled NOTIFY timelineMediaEffectsEnabledChanged)
    Q_PROPERTY(bool timelineDateDividersEnabled READ timelineDateDividersEnabled WRITE
                 setTimelineDateDividersEnabled NOTIFY timelineDateDividersEnabledChanged)
    Q_PROPERTY(bool uiMotionAnimationsEnabled READ uiMotionAnimationsEnabled WRITE
                 setUiMotionAnimationsEnabled NOTIFY uiMotionAnimationsEnabledChanged)
    Q_PROPERTY(bool desktopWindowFocusBlurEnabled READ desktopWindowFocusBlurEnabled WRITE
                 setDesktopWindowFocusBlurEnabled NOTIFY desktopWindowFocusBlurEnabledChanged)
    Q_PROPERTY(
      int desktopWindowFocusBlurDelaySeconds READ desktopWindowFocusBlurDelaySeconds WRITE
        setDesktopWindowFocusBlurDelaySeconds NOTIFY desktopWindowFocusBlurDelaySecondsChanged)
    Q_PROPERTY(int navigationRoomListWidthPx READ navigationRoomListWidthPx WRITE
                 setNavigationRoomListWidthPx NOTIFY navigationRoomListWidthPxChanged)
    Q_PROPERTY(int navigationCommunitiesWidthPx READ navigationCommunitiesWidthPx WRITE
                 setNavigationCommunitiesWidthPx NOTIFY navigationCommunitiesWidthPxChanged)
    Q_PROPERTY(
      double uiScaleFactor READ uiScaleFactor WRITE setUiScaleFactor NOTIFY uiScaleFactorChanged)
    Q_PROPERTY(
      double uiFontSizePt READ uiFontSizePt WRITE setUiFontSizePt NOTIFY uiFontSizePtChanged)
    Q_PROPERTY(
      QString uiFontFamily READ uiFontFamily WRITE setUiFontFamily NOTIFY uiFontFamilyChanged)
    Q_PROPERTY(QString uiFontEmojiFamily READ uiFontEmojiFamily WRITE setUiFontEmojiFamily NOTIFY
                 uiFontEmojiFamilyChanged)
    Q_PROPERTY(QString uiLanguage READ uiLanguage WRITE setUiLanguage NOTIFY uiLanguageChanged)
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
    Q_PROPERTY(bool callsElementEnabled READ callsElementEnabled WRITE setCallsElementEnabled NOTIFY
                 callsElementEnabledChanged)
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
    Q_PROPERTY(bool timelineThreadsCollapseReplies READ timelineThreadsCollapseReplies WRITE
                 setTimelineThreadsCollapseReplies NOTIFY timelineThreadsCollapseRepliesChanged)
    Q_PROPERTY(
      double timelineMediaDefaultAudioPlaybackSpeed READ timelineMediaDefaultAudioPlaybackSpeed
        WRITE setTimelineMediaDefaultAudioPlaybackSpeed NOTIFY
          timelineMediaDefaultAudioPlaybackSpeedChanged)
    Q_PROPERTY(QString integrationsBrowserCommand READ integrationsBrowserCommand WRITE
                 setIntegrationsBrowserCommand NOTIFY integrationsBrowserCommandChanged)
    Q_PROPERTY(int integrationsDbusApiAccess READ integrationsDbusApiAccess WRITE
                 setIntegrationsDbusApiAccess NOTIFY integrationsDbusApiAccessChanged)
    Q_PROPERTY(
      QString integrationsTranscriptionProvider READ integrationsTranscriptionProvider WRITE
        setIntegrationsTranscriptionProvider NOTIFY integrationsTranscriptionProviderChanged)
    Q_PROPERTY(QString integrationsTranscriptionApiUrl READ integrationsTranscriptionApiUrl WRITE
                 setIntegrationsTranscriptionApiUrl NOTIFY integrationsTranscriptionApiUrlChanged)
    Q_PROPERTY(QString integrationsTranscriptionModel READ integrationsTranscriptionModel WRITE
                 setIntegrationsTranscriptionModel NOTIFY integrationsTranscriptionModelChanged)
    Q_PROPERTY(
      QString integrationsTranscriptionLanguage READ integrationsTranscriptionLanguage WRITE
        setIntegrationsTranscriptionLanguage NOTIFY integrationsTranscriptionLanguageChanged)
    Q_PROPERTY(QString integrationsTranscriptionPrompt READ integrationsTranscriptionPrompt WRITE
                 setIntegrationsTranscriptionPrompt NOTIFY integrationsTranscriptionPromptChanged)

    Q_PROPERTY(QStringList hiddenPins READ hiddenPins WRITE setHiddenPins NOTIFY hiddenPinsChanged)
    Q_PROPERTY(QStringList openTabs READ openTabs WRITE setOpenTabs NOTIFY openTabsChanged)
    Q_PROPERTY(QStringList pinnedTabs READ pinnedTabs WRITE setPinnedTabs NOTIFY pinnedTabsChanged)
    Q_PROPERTY(QStringList hiddenWidgets READ hiddenWidgets WRITE setHiddenWidgets NOTIFY
                 hiddenWidgetsChanged)
    Q_PROPERTY(
      QStringList hiddenSpaces READ hiddenSpaces WRITE setHiddenSpaces NOTIFY hiddenSpacesChanged)
    Q_PROPERTY(QString sponsoringStatus READ sponsoringStatus WRITE setSponsoringStatus NOTIFY
                 sponsoringStatusChanged)
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

    enum class RoomHeaderButtonLabels
    {
        Adaptive,
        Never,
    };
    Q_ENUM(RoomHeaderButtonLabels)

    enum class DesktopSystemTrayIconStyle
    {
        Colorized,
        MonochromeLight,
        MonochromeDark,
    };
    Q_ENUM(DesktopSystemTrayIconStyle)

    enum class TimelineMessagesLayoutPositioning
    {
        Adaptive,
        OpposingBySender,
        AllLeft,
        AllRight,
    };
    Q_ENUM(TimelineMessagesLayoutPositioning)

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

    enum class Density
    {
        Spacious, // Default spacing and icon sizes
        Compact,  // Tighter spacing and smaller icons/avatars
        Dense,    // One-line rows with inline last-message preview
    };
    Q_ENUM(Density)

    enum class ScrollbarPolicy
    {
        WhenNeeded, // Show scrollbars only when content overflows
        Never,      // Never show scrollbars
        Always,     // Always show scrollbars
    };
    Q_ENUM(ScrollbarPolicy)

    enum class ThemeMode
    {
        Light, // Force the light member of the current theme family
        Dark,  // Force the dark member
        Auto,  // Follow the OS colour scheme, repaint live when it flips
    };
    Q_ENUM(ThemeMode)

    enum class RoomListOpeningPolicy
    {
        ReuseActiveTab, // Navigate in the current active tab
        OpenNewTab,     // Open a new tab for each room
    };
    Q_ENUM(RoomListOpeningPolicy)

    enum class TabPinButtonVisibility
    {
        Always, // Always show the pin button
        Never,  // Never show the pin button
    };
    Q_ENUM(TabPinButtonVisibility)

    enum class TabLabelDisplay
    {
        AvatarAndLabel, // Show avatar and room name
        AvatarOnly,     // Show avatar only
    };
    Q_ENUM(TabLabelDisplay)

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
    void setTimelineMessagesDragSelect(bool state);
    void setTimelineMessagesEmojiOnlyEnlarge(bool state);
    void setTimelineFormattedCodeSyntaxHighlighting(bool state);
    void setDesktopSystemTrayEnabled(bool state);
    void setDesktopSystemTrayAutostart(bool state);
    void setDesktopSystemTrayIconStyle(DesktopSystemTrayIconStyle style);
    void setDesktopSystemTrayFirstClosePrompted(bool state);
    void setUiScaleFactor(double factor);
    void setUiFontSizePt(double size);
    void setUiFontFamily(QString family);
    void setUiFontEmojiFamily(QString family);
    void setUiLanguage(QString code);
    void setNavigationCommunitiesFilterFavourites(bool state);
    void setNavigationCommunitiesFilterPeople(bool state);
    void setNavigationCommunitiesFilterBots(bool state);
    void setNavigationCommunitiesFilterGroups(bool state);
    void setNavigationCommunitiesFilterServerNotices(bool state);
    void setNavigationCommunitiesFilterLowPriority(bool state);
    void setUiScrollbarPolicy(ScrollbarPolicy policy);
    void setUiThemeMode(ThemeMode mode);
    void setComposerInputMarkdownToHtmlEnabled(bool state);
    void setComposerInputSendKey(SendMessageKey key);
    void setComposerInputAutoReplaceEmoji(AutoReplaceEmoji state);
    void setComposerInputEmojiPreferredGender(EmojiPreferredGender state);
    void setComposerInputEmojiPreferredSkinTone(EmojiPreferredSkinTone state);
    void setComposerInputInlineEmojiPickerEnabled(bool state);
    void setComposerInputInlineRoomPickerEnabled(bool state);
    void setComposerInputInlineUserPickerEnabled(bool state);
    void setComposerInputSelectionFormattingToolbarEnabled(bool state);
    void setComposerInputTranscriptionEnabled(bool state);
    void setComposerInputSpellcheckEnabled(bool state);
    void setComposerInputSpellcheckLanguages(QStringList languages);
    void setComposerAttachmentsStripImageMetadata(bool state);
    void setTimelineMessagesStyle(TimelineMessagesStyle style);
    void setTimelineRoomHeaderButtonLabels(RoomHeaderButtonLabels value);
    void setTimelineMessagesLayoutPositioning(TimelineMessagesLayoutPositioning positioning);
    void setTimelineUserColorCodingPolicy(TimelineUserColorCodingPolicy policy);
    void setTimelineMessagesLayoutAvatarSize(AvatarSize size);
    void setTimelineMessagesLayoutShowOwnAvatar(bool state);
    void setTimelineMessagesLayoutMaxWidthPercent(int value);
    void setTimelineMessagesLayoutAdaptivePositioningBreakpointPx(int value);
    void setTimelineMessageActionsPinnedReactions(QString value);
    void setTimelineMessagesSenderUsername(ShowSenderUsername state);
    void setTimelineMediaAnimateOnHover(bool state);
    void setTimelineReadReceiptsEnabled(bool state);
    void setComposerTypingSendEnabled(bool state);
    void setTimelineTypingShowEnabled(bool state);
    void setNavigationRoomListSort(RoomSortOrder order);
    void setTimelineMessageActionsActivationPolicy(TimelineMessageActionsActivationPolicy policy);
    void setNavigationCommunitiesWidthPx(int state);
    void setNavigationRoomListWidthPx(int state);
    void setNotificationsAccountEnabled(bool state);
    void setDesktopNotificationsEnabled(bool state);
    void setDesktopNotificationsAttentionOnIncoming(bool state);
    void setDesktopAttentionWindowTitleEnabled(bool state);
    void setDesktopAttentionAppBadgeEnabled(bool state);
    void setUiAvatarsCircular(bool state);
    void setDesktopNotificationsMessageContentPolicy(NotificationMessageContentPolicy policy);
    void setNavigationRoomListShowUnreadIndicators(bool state);
    void setNavigationCommunitiesShowUnreadIndicators(bool state);
    void setUiLayoutDensity(Density density);
    void setNavigationRoomListShowLastMessageTime(bool state);
    void setNavigationRoomListLastMessagePreview(LastMessagePreview style);
    void setNavigationRoomListOpeningPolicy(RoomListOpeningPolicy policy);
    void setNavigationTabsAutoHideSingle(bool state);
    void setNavigationTabsShowPinButton(TabPinButtonVisibility policy);
    void setNavigationTabsPinnedTabLabel(TabLabelDisplay display);
    void setNavigationTabsTabLabel(TabLabelDisplay display);
    void setNavigationTabsPreferredWidthPx(int px);
    void setNavigationTabsMinimumWidthPx(int px);
    void setNavigationTabsMaxRecentlyClosedTimelines(int count);
    void setTimelineMediaEffectsEnabled(bool state);
    void setTimelineDateDividersEnabled(bool state);
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
    void setCallsElementEnabled(bool state);
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
    void setUnreadIndicatorsHiddenFilters(const QStringList &unreadIndicatorsHiddenFilters);
    void setHiddenPins(const QStringList &hiddenTags);
    void setHiddenWidgets(const QStringList &hiddenTags);
    void setHiddenSpaces(const QStringList &hiddenSpaces);
    void setOpenTabs(const QStringList &openTabs);
    void setPinnedTabs(const QStringList &pinnedTabs);
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
    void setTimelineThreadsCollapseReplies(bool state);
    Q_INVOKABLE QMap<QString, bool> timelineThreadsCollapseRepliesByRoom() const;
    Q_INVOKABLE void setTimelineThreadsCollapseRepliesByRoom(const QMap<QString, bool> &byRoom);
    Q_INVOKABLE void setTimelineThreadsCollapseRepliesForRoom(const QString &roomId, bool value);
    Q_INVOKABLE void removeTimelineThreadsCollapseRepliesForRoom(const QString &roomId);
    Q_INVOKABLE QVariant timelineThreadsCollapseRepliesOverrideForRoom(const QString &roomId) const;
    Q_INVOKABLE bool resolvedTimelineThreadsCollapseReplies(const QString &roomId) const;

    // Per-room overrides for "Show others when I'm typing"
    // (`composer.typing.send.global` + `composer.typing.send.by_room`).
    // Mirrors the threads-collapse pattern above. The Q_PROPERTY
    // `composerTypingSendEnabled` is unchanged (it represents the global).
    Q_INVOKABLE QMap<QString, bool> composerTypingSendEnabledByRoom() const;
    Q_INVOKABLE void setComposerTypingSendEnabledByRoom(const QMap<QString, bool> &byRoom);
    Q_INVOKABLE void setComposerTypingSendEnabledForRoom(const QString &roomId, bool value);
    Q_INVOKABLE void removeComposerTypingSendEnabledForRoom(const QString &roomId);
    Q_INVOKABLE QVariant composerTypingSendEnabledOverrideForRoom(const QString &roomId) const;
    Q_INVOKABLE bool resolvedComposerTypingSendEnabled(const QString &roomId) const;

    // Per-room overrides for "Show others when I've read their messages"
    // (`timeline.read_receipts.global` + `timeline.read_receipts.by_room`).
    // The Q_PROPERTY `timelineReadReceiptsEnabled` is unchanged (it
    // represents the global value).
    Q_INVOKABLE QMap<QString, bool> timelineReadReceiptsEnabledByRoom() const;
    Q_INVOKABLE void setTimelineReadReceiptsEnabledByRoom(const QMap<QString, bool> &byRoom);
    Q_INVOKABLE void setTimelineReadReceiptsEnabledForRoom(const QString &roomId, bool value);
    Q_INVOKABLE void removeTimelineReadReceiptsEnabledForRoom(const QString &roomId);
    Q_INVOKABLE QVariant timelineReadReceiptsEnabledOverrideForRoom(const QString &roomId) const;
    Q_INVOKABLE bool resolvedTimelineReadReceiptsEnabled(const QString &roomId) const;

    // Per-room transcription overrides. The value type is intentionally a
    // 5-string-fields-per-room shape (unlike the per-room-bool collapse
    // replies pattern), so we expose the QML side through a sparse
    // QVariantMap getter and per-field setters/clearers. Field names match
    // the YAML schema: "provider", "api_url", "model", "language", "prompt".
    // `provider` carries a token string ("openai_batch" / "openai_realtime").
    // Keys absent from the returned map mean "inherit the global value".
    Q_INVOKABLE QVariantMap integrationsTranscriptionOverridesForRoom(const QString &roomId) const;
    Q_INVOKABLE bool hasIntegrationsTranscriptionOverrideForRoom(const QString &roomId,
                                                                 const QString &fieldName) const;
    Q_INVOKABLE void setIntegrationsTranscriptionOverrideForRoom(const QString &roomId,
                                                                 const QString &fieldName,
                                                                 const QString &value);
    Q_INVOKABLE void
    clearIntegrationsTranscriptionOverrideForRoom(const QString &roomId, const QString &fieldName);
    Q_INVOKABLE void clearIntegrationsTranscriptionOverridesForRoom(const QString &roomId);
    QMap<QString, QMap<QString, QString>> integrationsTranscriptionOverridesByRoom() const;
    void setIntegrationsTranscriptionOverridesByRoom(
      const QMap<QString, QMap<QString, QString>> &byRoom);
    void setIntegrationsBrowserCommand(QString command);
    void setIntegrationsTranscriptionProvider(QString provider);
    void setIntegrationsTranscriptionApiUrl(QString url);
    void setIntegrationsTranscriptionModel(QString model);
    void setIntegrationsTranscriptionLanguage(QString language);
    void setIntegrationsTranscriptionPrompt(QString prompt);
    void setCollapsedSpaces(QStringList spaces);
    void setIntegrationsDbusApiAccess(int access);
    void setSponsoringStatus(QString status);
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
    void applyOsColorScheme();

    // Language helpers for QML (used on the Welcome page).
    // Index 0 in the dropdown is the translated "Use system" entry; subsequent
    // entries are the languages whose .qm bundles are present in :/translations.
    Q_INVOKABLE QStringList languageDropdownLabels() const;
    Q_INVOKABLE int languageDropdownIndex() const;
    Q_INVOKABLE void setLanguageByDropdownIndex(int index);

    // Localized label for the current composer-send-key setting (e.g. "Enter",
    // "Shift+Enter", "Ctrl+Enter"). Reuses the same translations the settings
    // dropdown shows so QML callers (composer Send tooltip, slash-command
    // hint) stay consistent with the settings screen, and refreshes on both
    // setting changes and runtime language switches.
    QString composerInputSendKeyLabel() const;

#include "settings/ui/facade/UserSettingsGetters.inc"

signals:
    void navigationCommunitiesFilterFavouritesChanged(bool state);
    void navigationCommunitiesFilterPeopleChanged(bool state);
    void navigationCommunitiesFilterBotsChanged(bool state);
    void navigationCommunitiesFilterGroupsChanged(bool state);
    void navigationCommunitiesFilterServerNoticesChanged(bool state);
    void navigationCommunitiesFilterLowPriorityChanged(bool state);
    void uiScrollbarPolicyChanged(ScrollbarPolicy policy);
    void navigationRoomListSortChanged(RoomSortOrder order);
    void uiThemeSlugChanged(QString state);
    void uiThemeModeChanged(ThemeMode mode);
    void timelineMessagesHoverHighlightChanged(bool state);
    void timelineMessagesDragSelectChanged(bool state);
    void timelineMessagesEmojiOnlyEnlargeChanged(bool state);
    void timelineFormattedCodeSyntaxHighlightingChanged(bool state);
    void desktopSystemTrayEnabledChanged(bool state);
    void desktopSystemTrayAutostartChanged(bool state);
    void desktopSystemTrayIconStyleChanged(DesktopSystemTrayIconStyle style);
    void desktopSystemTrayFirstClosePromptedChanged(bool state);
    void composerInputMarkdownToHtmlEnabledChanged(bool state);
    void composerInputSendKeyChanged(SendMessageKey key);
    void composerInputSendKeyLabelChanged();
    void composerInputAutoReplaceEmojiChanged(AutoReplaceEmoji state);
    void composerInputEmojiPreferredGenderChanged(EmojiPreferredGender state);
    void composerInputEmojiPreferredSkinToneChanged(EmojiPreferredSkinTone state);
    void composerInputInlineEmojiPickerEnabledChanged(bool state);
    void composerInputInlineRoomPickerEnabledChanged(bool state);
    void composerInputInlineUserPickerEnabledChanged(bool state);
    void composerInputSelectionFormattingToolbarEnabledChanged(bool state);
    void composerInputTranscriptionEnabledChanged(bool state);
    void composerInputSpellcheckEnabledChanged(bool state);
    void composerInputSpellcheckLanguagesChanged(QStringList languages);
    void composerAttachmentsStripImageMetadataChanged(bool state);
    void timelineMessagesStyleChanged(TimelineMessagesStyle style);
    void timelineRoomHeaderButtonLabelsChanged(RoomHeaderButtonLabels value);
    void timelineMessagesLayoutPositioningChanged(TimelineMessagesLayoutPositioning positioning);
    void timelineUserColorCodingPolicyChanged(TimelineUserColorCodingPolicy policy);
    void timelineMessagesLayoutAvatarSizeChanged(AvatarSize size);
    void timelineMessagesLayoutShowOwnAvatarChanged(bool state);
    void timelineMessagesLayoutMaxWidthPercentChanged(int value);
    void timelineMessagesLayoutAdaptivePositioningBreakpointPxChanged(int value);
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
    void navigationRoomListShowUnreadIndicatorsChanged(bool state);
    void navigationCommunitiesShowUnreadIndicatorsChanged(bool state);
    void uiLayoutDensityChanged(Density density);
    void navigationRoomListShowLastMessageTimeChanged(bool state);
    void navigationRoomListLastMessagePreviewChanged(LastMessagePreview style);
    void navigationRoomListOpeningPolicyChanged(RoomListOpeningPolicy policy);
    void navigationTabsAutoHideSingleChanged(bool state);
    void navigationTabsShowPinButtonChanged(TabPinButtonVisibility policy);
    void navigationTabsPinnedTabLabelChanged(TabLabelDisplay display);
    void navigationTabsTabLabelChanged(TabLabelDisplay display);
    void navigationTabsPreferredWidthPxChanged(int px);
    void navigationTabsMinimumWidthPxChanged(int px);
    void navigationTabsMaxRecentlyClosedTimelinesChanged(int count);
    void timelineMediaEffectsEnabledChanged(bool state);
    void timelineDateDividersEnabledChanged(bool state);
    void uiMotionAnimationsEnabledChanged(bool state);
    void desktopWindowFocusBlurEnabledChanged(bool state);
    void desktopWindowFocusBlurDelaySecondsChanged(int state);
    void navigationRoomListWidthPxChanged(int state);
    void navigationCommunitiesWidthPxChanged(int state);
    void uiScaleFactorChanged(double factor);
    void uiFontSizePtChanged(double state);
    void uiFontFamilyChanged(QString state);
    void uiFontEmojiFamilyChanged(QString state);
    void uiLanguageChanged(QString state);
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
    void callsElementEnabledChanged(bool state);
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
    void timelineThreadsCollapseRepliesChanged(bool state);
    void timelineThreadsCollapseRepliesByRoomChanged();
    void composerTypingSendEnabledByRoomChanged();
    void timelineReadReceiptsEnabledByRoomChanged();
    void hiddenPinsChanged();
    void openTabsChanged();
    void pinnedTabsChanged();
    void globalExcludesChanged();
    void hiddenWidgetsChanged();
    void hiddenSpacesChanged();
    void hiddenTimelineEventTypesChanged();
    void composerDraftsByRoomChanged();
    void integrationsDbusApiAccessChanged(int state);
    void integrationsBrowserCommandChanged(QString command);
    void integrationsTranscriptionProviderChanged(QString provider);
    void integrationsTranscriptionApiUrlChanged(QString url);
    void integrationsTranscriptionModelChanged(QString model);
    void integrationsTranscriptionLanguageChanged(QString language);
    void integrationsTranscriptionPromptChanged(QString prompt);
    void integrationsTranscriptionOverridesByRoomChanged();
    void sponsoringStatusChanged(QString sponsoringStatus);
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
    bool applyEffectiveSlug(const QString &slug);
    QString counterpartSlugForVariant(const QString &newVariant) const;
    void emitSessionAuthStateChangedIfNeeded(bool hadPersistedSessionIdentity,
                                             bool hadActiveSessionState);

#include "settings/ui/facade/UserSettingsPagePrivateMembers.h"
};

#include "settings/ui/UserSettingsModel.h"
