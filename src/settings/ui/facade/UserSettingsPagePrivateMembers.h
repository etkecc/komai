// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

QString uiThemeSlugDefault_ = QString::fromLatin1(settings::core::definitions::kDefaultUiThemeSlug);
QString uiThemeSlug_;
bool timelineMessagesHoverHighlight_;
bool timelineMessagesEmojiOnlyEnlarge_;
bool timelineFormattedCodeSyntaxHighlighting_;
bool integrationsSystemTrayEnabled_;
bool integrationsSystemTrayAutostart_;
bool sidebarsCommunitiesVisible_;
bool sidebarsCommunitiesFilterFavourites_    = true;
bool sidebarsCommunitiesFilterPeople_        = true;
bool sidebarsCommunitiesFilterBots_          = true;
bool sidebarsCommunitiesFilterGroups_        = true;
bool sidebarsCommunitiesFilterServerNotices_ = true;
bool sidebarsCommunitiesFilterLowPriority_   = true;
bool sidebarsRoomListScrollbarsEnabled_;
bool composerInputMarkdownToHtmlEnabled_;
SendMessageKey composerInputSendKey_;
AutoReplaceEmoji composerInputAutoReplaceEmoji_;
TimelineMessagesStyle timelineMessagesStyle_ = TimelineMessagesStyle::Bubbles;
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
bool notificationsEnabled_;
bool notificationsAttentionOnIncoming_;
bool uiAvatarsCircular_;
NotificationMessageContentPolicy notificationsMessageContentPolicy_ =
  NotificationMessageContentPolicy::WheneverAvailable;
bool sidebarsRoomListShowCommunityCounts_;
bool uiLayoutCompactMode_;
bool sidebarsRoomListShowLastMessageTime_;
LastMessagePreview sidebarsRoomListLastMessagePreview_;
bool timelineMediaEffectsEnabled_;
bool uiMotionAnimationsEnabled_;
bool privacyWindowFocusBlurEnabled_;
int privacyWindowFocusBlurDelaySeconds_;
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
QString currentTagId_;
QString currentRoomId_;
QString homeserver_;
QStringList hiddenTags_;
QStringList mutedTags_;
QStringList hiddenPins_;
QStringList hiddenWidgets_;
QStringList recentReactions_;
QMap<QString, QString> composerDraftsByRoom_;
QList<QStringList> collapsedSpaces_;
DefaultAvatarStyle uiAvatarsDefaultAvatarStyle_ = DefaultAvatarStyle::BoringAvatarsBauhaus;
bool timelineMediaOpenImagesExternal_;
bool timelineMediaOpenVideosExternal_;
int integrationsDbusApiAccess_ = settings::core::definitions::kDefaultIntegrationsDbusApiAccess;
QString integrationsBrowserCommand_;
bool privacyMaintenanceUpdateSpaceVias_;
bool privacyMaintenanceExpireEvents_;
int windowWidth_                            = settings::core::definitions::kDefaultWindowWidthPx;
int windowHeight_                           = settings::core::definitions::kDefaultWindowHeightPx;
qulonglong dbMaxSizeBytes_                  = settings::core::definitions::kDefaultMaxDbSizeBytes;
uint dbMaxStores_                           = settings::core::definitions::kDefaultMaxStores;
bool usesFileSecretsProvider_               = false;
bool secretsProviderFallbackWarningVisible_ = false;
bool networkHttp3Enabled_ = settings::core::definitions::kDefaultNetworkHttp3Enabled;
QMap<QString, QString> secrets_;
settings::core::SettingsStore coreStore_;
bool suppressSettingsSave_ = false;

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

static QSharedPointer<UserSettings> instance_;
