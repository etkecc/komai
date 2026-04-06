// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

QString uiThemeSlugDefault_ = QString::fromLatin1(settings::core::definitions::kDefaultUiThemeSlug);
QString uiThemeSlug_;
bool timelineMessagesHoverHighlight_;
bool timelineMessagesEmojiOnlyEnlarge_;
bool timelineFormattedCodeSyntaxHighlighting_;
bool desktopSystemTrayEnabled_;
bool desktopSystemTrayAutostart_;
bool sidebarsCommunitiesVisible_;
bool sidebarsCommunitiesFilterFavourites_    = true;
bool sidebarsCommunitiesFilterPeople_        = true;
bool sidebarsCommunitiesFilterBots_          = true;
bool sidebarsCommunitiesFilterGroups_        = true;
bool sidebarsCommunitiesFilterServerNotices_ = true;
bool sidebarsCommunitiesFilterLowPriority_   = true;
ScrollbarPolicy uiScrollbarPolicy_           = ScrollbarPolicy::WhenNeeded;
bool composerInputMarkdownToHtmlEnabled_;
SendMessageKey composerInputSendKey_;
AutoReplaceEmoji composerInputAutoReplaceEmoji_;
EmojiPreferredGender composerInputEmojiPreferredGender_     = EmojiPreferredGender::NoPreference;
EmojiPreferredSkinTone composerInputEmojiPreferredSkinTone_ = EmojiPreferredSkinTone::NoPreference;
bool composerInputInlineEmojiPickerEnabled_                 = true;
bool composerInputInlineRoomPickerEnabled_                  = true;
bool composerInputInlineUserPickerEnabled_                  = true;
TimelineMessagesStyle timelineMessagesStyle_                = TimelineMessagesStyle::Bubbles;
TimelineMessagesPositioning timelineMessagesPositioning_ =
  TimelineMessagesPositioning::OpposingBySender;
TimelineUserColorCodingPolicy timelineUserColorCodingPolicy_ =
  TimelineUserColorCodingPolicy::AdaptiveByRoomSize;
bool timelineMessagesLayoutSmallAvatars_;
bool composerExtrasStickersEnabled_;
bool timelineMessagesLayoutShowOwnAvatar_;
QString timelineMessageActionsPinnedReactions_;
ShowSenderUsername timelineMessagesSenderUsername_;
bool timelineMediaAnimateOnHover_;
bool composerTypingSendEnabled_;
bool timelineTypingShowEnabled_;
RoomSortOrder sidebarsRoomListSort_;
UnreadDetectionPolicy sidebarsRoomListUnreadDetectionPolicy_ = UnreadDetectionPolicy::AnyEvent;
TimelineMessageActionsActivationPolicy timelineMessageActionsActivationPolicy_ =
  TimelineMessageActionsActivationPolicy::ActionsButton;
bool timelineReadReceiptsEnabled_;
bool notificationsAccountEnabled_                   = true;
bool notificationsAccountEnabledLoaded_             = false;
bool notificationsAccountEnabledInFlight_           = false;
std::uint64_t notificationsAccountEnabledRequestId_ = 0;
NotificationsAccountHandleProvider notificationsAccountHandleProvider_;
NotificationsAccountFetchFn notificationsAccountFetchFn_;
NotificationsAccountSetFn notificationsAccountSetFn_;
bool desktopNotificationsEnabled_;
bool desktopNotificationsAttentionOnIncoming_;
bool desktopAttentionWindowTitleEnabled_ =
  settings::core::definitions::kDefaultDesktopAttentionWindowTitleEnabled;
bool desktopAttentionAppBadgeEnabled_ =
  settings::core::definitions::kDefaultDesktopAttentionAppBadgeEnabled;
bool uiAvatarsCircular_;
NotificationMessageContentPolicy desktopNotificationsMessageContentPolicy_ =
  NotificationMessageContentPolicy::WheneverAvailable;
bool sidebarsRoomListShowCommunityCounts_;
bool uiLayoutCompactMode_;
bool sidebarsRoomListShowLastMessageTime_;
LastMessagePreview sidebarsRoomListLastMessagePreview_;
bool timelineMediaEffectsEnabled_;
bool uiMotionAnimationsEnabled_;
bool desktopWindowFocusBlurEnabled_;
int desktopWindowFocusBlurDelaySeconds_;
bool encryptionKeySharingShareWithTrusted_;
bool encryptionKeySharingOnlyVerifiedUsers_;
bool encryptionBackupOnlineEnabled_;
bool uiInputMode_;
bool uiInputTouchSwipeGesturesEnabled_;
int uiLayoutContentMaxWidthPx_  = settings::core::definitions::kDefaultUiLayoutContentMaxWidthPx;
int sidebarsRoomListWidthPx_    = settings::core::definitions::kDefaultSidebarsRoomListWidthPx;
int sidebarsCommunitiesWidthPx_ = settings::core::definitions::kDefaultSidebarsCommunitiesWidthPx;
double uiScaleFactor_           = settings::core::definitions::kDefaultScaleFactor;
double uiFontSizePt_            = settings::core::definitions::kDefaultFontSizePt;
QString uiFontFamily_;
QString uiFontEmojiFamily_;
Presence networkPresenceStatusPolicy_;
ShowImage timelineMediaImageDisplay_;
QString callsAudioRingtone_;
QString callsDevicesMicrophone_;
QString callsDevicesCamera_;
QString callsDevicesCameraResolution_;
QString callsDevicesCameraFrameRate_;
int callsScreenshareFrameRate_;
bool callsScreensharePictureInPicture_;
bool callsScreenshareIncludeRemoteVideo_;
bool callsScreenshareShowCursor_ = settings::core::definitions::kDefaultScreenShareShowCursor;
bool callsRelayUseFallbackServer_;
bool callsLegacyEnabled_;
bool networkTlsEnableCertificateValidation_ =
  settings::core::definitions::kDefaultCertificateValidationEnabled;
QString profile_;
QString userId_;
QString accessToken_;
QString deviceId_;
QString currentFilterId_;
QString currentRoomId_;
QString homeserver_;
QStringList globalExcludes_;
QStringList badgesHiddenFilters_;
QStringList hiddenPins_;
QStringList hiddenWidgets_;
QStringList hiddenTimelineEventTypes_;
QMap<QString, QStringList> hiddenTimelineEventTypesByRoom_;
QMap<QString, QString> composerDraftsByRoom_;
QStringList collapsedSpaces_;
DefaultAvatarStyle uiAvatarsDefaultAvatarStyle_ = DefaultAvatarStyle::BoringAvatarsBauhaus;
bool timelineMediaOpenImagesExternal_;
bool timelineMediaOpenVideosExternal_;
bool timelineMediaAutoplayGifVideos_ = true;
bool timelineMediaOpenAudioExternal_;
double timelineMediaDefaultAudioPlaybackSpeed_ =
  settings::core::definitions::kDefaultTimelineMediaAudioPlaybackSpeed;
int integrationsDbusApiAccess_ = settings::core::definitions::kDefaultIntegrationsDbusApiAccess;
QString integrationsBrowserCommand_;
QString donationStatus_                     = QStringLiteral("visible");
int windowWidth_                            = settings::core::definitions::kDefaultWindowWidthPx;
int windowHeight_                           = settings::core::definitions::kDefaultWindowHeightPx;
bool usesFileSecretsProvider_               = false;
bool secretsProviderFallbackWarningVisible_ = false;
bool networkMrsEnabled_ = settings::core::definitions::kDefaultNetworkMrsEnabled;
QString networkMrsServerName_ =
  QString::fromLatin1(settings::core::definitions::kDefaultNetworkMrsServerName);
bool networkHttp3Enabled_ = settings::core::definitions::kDefaultNetworkHttp3Enabled;
QMap<QString, QString> secrets_;
settings::core::SettingsStore coreStore_;
bool suppressSettingsSave_ = false;
QTimer deferredStateSaveTimer_;
bool deferredStateSavePending_ = false;

enum class StartupPersistenceScope
{
    ConfigOnly,
    Full,
};

StartupPersistenceScope startupPersistenceScope_ = StartupPersistenceScope::ConfigOnly;

// Paths to the per-profile settings directory and files.
QString profileDirPath_;
QString configFilePath_;
QString stateFilePath_;
QString sessionFilePath_;
QString secretsFilePath_;
std::optional<::rust::Box<::komai::rust::SettingsProfileHandle>> rustSettingsProfileHandle_;

static QSharedPointer<UserSettings> instance_;
