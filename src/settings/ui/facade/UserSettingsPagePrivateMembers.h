// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

QString defaultTheme_ = QString::fromLatin1(settings::core::definitions::kDefaultUiThemeSlug);
QString theme_;
bool timelineMessagesHoverHighlight_;
bool timelineMessagesEmojiOnlyEnlarge_;
bool integrationsSystemTrayEnabled_;
bool integrationsSystemTrayAutostart_;
bool sidebarsCommunitiesVisible_;
bool sidebarsRoomListScrollbarsEnabled_;
bool composerInputMarkdownEnabled_;
SendMessageKey composerInputSendKey_;
AutoReplaceEmoji composerInputAutoReplaceEmoji_;
TimelineMessageLayout timelineMessageLayout_ = TimelineMessageLayout::Bubbles;
bool timelineMessagesLayoutSmallAvatars_;
bool composerExtrasStickersEnabled_;
bool timelineMessagesLayoutShowOwnAvatar_;
QString pinnedReactions_;
ShowSenderUsername timelineMessagesSenderUsername_;
bool timelineMediaAnimateOnHover_;
bool composerTypingSendEnabled_;
bool timelineTypingShowEnabled_;
RoomSortOrder sidebarsRoomListSort_;
TimelineMessageActionsPolicy timelineMessageActionsPolicy_ =
  TimelineMessageActionsPolicy::ActionsButton;
bool timelineReadReceiptsEnabled_;
bool notificationsEnabled_;
bool notificationsAttentionOnIncoming_;
bool uiAvatarsCircular_;
NotificationMessageContentPolicy notificationMessageContentPolicy_ =
  NotificationMessageContentPolicy::WheneverAvailable;
bool sidebarsRoomListShowCommunityCounts_;
bool sidebarsRoomListCompact_;
bool sidebarsRoomListShowLastMessageTime_;
LastMessagePreview sidebarsRoomListLastMessagePreview_;
bool timelineMediaEffectsEnabled_;
bool uiMotionAnimationsEnabled_;
bool privacyWindowFocusBlurEnabled_;
int privacyWindowFocusBlurDelaySeconds_;
bool encryptionKeySharingShareWithTrusted_;
bool encryptionKeySharingOnlyVerifiedUsers_;
bool encryptionBackupOnlineEnabled_;
bool uiInputModeTouchEnabled_;
bool uiInputTouchSwipeGesturesEnabled_;
int uiLayoutContentMaxWidthPx_ = settings::core::definitions::kDefaultUiLayoutContentMaxWidthPx;
int timelineMessagesMaxWidthPx_;
int sidebarsRoomListWidthPx_    = settings::core::definitions::kDefaultSidebarsRoomListWidthPx;
int sidebarsCommunitiesWidthPx_ = settings::core::definitions::kDefaultSidebarsCommunitiesWidthPx;
double uiScaleFactor_           = settings::core::definitions::kDefaultScaleFactor;
double baseFontSize_            = settings::core::definitions::kDefaultFontSizePt;
QString uiFontFamily_;
QString uiEmojiFontFamily_;
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
QString homeserver_;
QStringList hiddenTags_;
QStringList mutedTags_;
QStringList hiddenPins_;
QStringList hiddenWidgets_;
QStringList recentReactions_;
QList<QStringList> collapsedSpaces_;
bool uiAvatarsIdenticonFallback_;
bool timelineMediaOpenImagesExternal_;
bool timelineMediaOpenVideosExternal_;
int integrationsDbusApiAccess_ = settings::core::definitions::kDefaultIntegrationsDbusApiAccess;
QString integrationsBrowserCommand_;
bool privacyMaintenanceUpdateSpaceVias_;
bool privacyMaintenanceExpireEvents_;
int windowWidth_              = settings::core::definitions::kDefaultWindowWidthPx;
int windowHeight_             = settings::core::definitions::kDefaultWindowHeightPx;
qulonglong maxDbSize_         = settings::core::definitions::kDefaultMaxDbSizeBytes;
uint maxStores_               = settings::core::definitions::kDefaultMaxStores;
bool usesFileSecretsProvider_ = false;
bool networkHttp3Enabled_     = settings::core::definitions::kDefaultNetworkHttp3Enabled;
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
