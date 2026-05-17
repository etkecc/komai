// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

QString uiThemeSlugDefault_ = QString::fromLatin1(settings::core::definitions::kDefaultUiThemeSlug);
QString uiThemeSlug_;
bool timelineMessagesHoverHighlight_;
bool timelineMessagesDragSelect_;
bool timelineMessagesEmojiOnlyEnlarge_;
bool timelineFormattedCodeSyntaxHighlighting_;
bool desktopSystemTrayEnabled_;
bool desktopSystemTrayAutostart_;
bool desktopSystemTrayFirstClosePrompted_      = false;
bool navigationCommunitiesFilterFavourites_    = true;
bool navigationCommunitiesFilterPeople_        = true;
bool navigationCommunitiesFilterBots_          = true;
bool navigationCommunitiesFilterGroups_        = true;
bool navigationCommunitiesFilterServerNotices_ = true;
bool navigationCommunitiesFilterLowPriority_   = true;
ScrollbarPolicy uiScrollbarPolicy_             = ScrollbarPolicy::WhenNeeded;
bool composerInputMarkdownToHtmlEnabled_;
SendMessageKey composerInputSendKey_;
AutoReplaceEmoji composerInputAutoReplaceEmoji_;
EmojiPreferredGender composerInputEmojiPreferredGender_     = EmojiPreferredGender::NoPreference;
EmojiPreferredSkinTone composerInputEmojiPreferredSkinTone_ = EmojiPreferredSkinTone::NoPreference;
bool composerInputInlineEmojiPickerEnabled_                 = true;
bool composerInputInlineRoomPickerEnabled_                  = true;
bool composerInputInlineUserPickerEnabled_                  = true;
bool composerInputTranscriptionEnabled_                     = true;
bool composerInputSpellcheckEnabled_                        = true;
QStringList composerInputSpellcheckLanguages_;
bool composerAttachmentsStripImageMetadata_  = true;
TimelineMessagesStyle timelineMessagesStyle_ = TimelineMessagesStyle::Bubbles;
TimelineMessagesLayoutPositioning timelineMessagesLayoutPositioning_ =
  TimelineMessagesLayoutPositioning::OpposingBySender;
TimelineUserColorCodingPolicy timelineUserColorCodingPolicy_ =
  TimelineUserColorCodingPolicy::AdaptiveByRoomSize;
AvatarSize timelineMessagesLayoutAvatarSize_{};
bool timelineMessagesLayoutShowOwnAvatar_;
int timelineMessagesLayoutMaxWidthPercent_ = 80;
QString timelineMessageActionsPinnedReactions_;
ShowSenderUsername timelineMessagesSenderUsername_;
bool timelineMediaAnimateOnHover_;
bool composerTypingSendEnabled_;
bool timelineTypingShowEnabled_;
RoomSortOrder navigationRoomListSort_;
RoomListOpeningPolicy navigationRoomListOpeningPolicy_ = RoomListOpeningPolicy::ReuseActiveTab;
TabPinButtonVisibility navigationTabsShowPinButton_    = TabPinButtonVisibility::Never;
TabLabelDisplay navigationTabsPinnedTabLabel_          = TabLabelDisplay::AvatarOnly;
TabLabelDisplay navigationTabsTabLabel_                = TabLabelDisplay::AvatarAndLabel;
int navigationTabsPreferredWidthPx_                    = 200;
int navigationTabsMinimumWidthPx_                      = 120;
int navigationTabsMaxRecentlyClosedTimelines_          = 3;
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
bool desktopAttentionWindowTitleEnabled_ = true;
bool desktopAttentionAppBadgeEnabled_    = true;
bool uiAvatarsCircular_;
NotificationMessageContentPolicy desktopNotificationsMessageContentPolicy_ =
  NotificationMessageContentPolicy::WheneverAvailable;
bool navigationRoomListShowUnreadIndicators_;
bool navigationCommunitiesShowUnreadIndicators_;
Density uiLayoutDensity_ = Density::Spacious;
bool navigationRoomListShowLastMessageTime_;
LastMessagePreview navigationRoomListLastMessagePreview_;
bool timelineMediaEffectsEnabled_;
bool timelineDateDividersEnabled_;
bool uiMotionAnimationsEnabled_;
bool desktopWindowFocusBlurEnabled_;
int desktopWindowFocusBlurDelaySeconds_;
bool encryptionKeySharingShareWithTrusted_;
bool encryptionKeySharingOnlyVerifiedUsers_;
bool encryptionBackupOnlineEnabled_;
int navigationRoomListWidthPx_ = settings::core::definitions::kDefaultNavigationRoomListWidthPx;
int navigationCommunitiesWidthPx_ =
  settings::core::definitions::kDefaultNavigationCommunitiesWidthPx;
double uiScaleFactor_ = 1.0;
double uiFontSizePt_  = settings::core::definitions::kDefaultFontSizePt;
QString uiFontFamily_;
QString uiFontEmojiFamily_;
QString uiLanguage_;
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
bool callsScreenshareShowCursor_ = true;
bool callsRelayUseFallbackServer_;
bool callsLegacyEnabled_;
bool networkTlsEnableCertificateValidation_ = true;
QString profile_;
QString userId_;
QString accessToken_;
QString deviceId_;
QString currentFilterId_;
QString currentRoomId_;
QString homeserver_;
QStringList globalExcludes_;
QStringList unreadIndicatorsHiddenFilters_;
QStringList hiddenPins_;
QStringList openTabs_;
QStringList pinnedTabs_;
QStringList hiddenWidgets_;
QStringList hiddenTimelineEventTypes_;
QMap<QString, QStringList> hiddenTimelineEventTypesByRoom_;
QMap<QString, QString> composerDraftsByRoom_;
QStringList collapsedSpaces_;
QStringList hiddenSpaces_;
DefaultAvatarStyle uiAvatarsDefaultAvatarStyle_ = DefaultAvatarStyle::BoringAvatarsBauhaus;
bool timelineMediaOpenImagesExternal_;
bool timelineMediaOpenVideosExternal_;
bool timelineMediaAutoplayGifVideos_ = true;
bool timelineMediaOpenAudioExternal_;
double timelineMediaDefaultAudioPlaybackSpeed_ = 1.0;
bool timelineThreadsCollapseReplies_           = false;
QMap<QString, bool> timelineThreadsCollapseRepliesByRoom_;
QMap<QString, bool> composerTypingSendEnabledByRoom_;
QMap<QString, bool> timelineReadReceiptsEnabledByRoom_;
int integrationsDbusApiAccess_ = 0;
QString integrationsBrowserCommand_;
QString integrationsTranscriptionProvider_;
QString integrationsTranscriptionApiUrl_;
QString integrationsTranscriptionModel_;
QString integrationsTranscriptionLanguage_;
QString integrationsTranscriptionPrompt_;
// Per-room overrides for the 5 non-secret transcription fields. The
// outer key is the raw Matrix room id. The inner map only contains
// field names that have an override set ("provider", "api_url",
// "model", "language", "prompt"); absent inner keys inherit the
// corresponding global value.
QMap<QString, QMap<QString, QString>> integrationsTranscriptionOverridesByRoom_;
QString sponsoringStatus_                   = QStringLiteral("visible");
int windowWidth_                            = settings::core::definitions::kDefaultWindowWidthPx;
int windowHeight_                           = settings::core::definitions::kDefaultWindowHeightPx;
bool usesFileSecretsProvider_               = false;
bool secretsProviderFallbackWarningVisible_ = false;
bool networkMrsEnabled_                     = true;
QString networkMrsServerName_ =
  QString::fromLatin1(settings::core::definitions::kDefaultNetworkMrsServerName);
bool networkHttp3Enabled_ = false;
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
