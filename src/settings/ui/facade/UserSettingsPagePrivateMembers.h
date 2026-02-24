// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

QString defaultTheme_ = QString::fromLatin1(settings::core::definitions::kDefaultUiThemeSlug);
QString theme_;
bool messageHoverHighlight_;
bool enlargeEmojiOnlyMessages_;
bool systemTrayEnabled_;
bool systemTrayAutostart_;
bool communitiesSidebarVisible_;
bool roomListScrollbarsVisible_;
bool markdownEnabled_;
SendMessageKey sendMessageKey_;
AutoReplaceEmoji autoReplaceEmoji_;
TimelineMessageLayout timelineMessageLayout_ = TimelineMessageLayout::Bubbles;
bool timelineSmallAvatarsEnabled_;
bool stickersEnabled_;
bool timelineShowOwnAvatarInBubbleLayout_;
QString pinnedReactions_;
ShowSenderUsername showSenderUsername_;
bool animateImagesOnHover_;
bool sendTypingNotificationsEnabled_;
bool showTypingNotificationsEnabled_;
RoomSortOrder roomSortOrder_;
TimelineMessageActionsPolicy timelineMessageActionsPolicy_ =
  TimelineMessageActionsPolicy::ActionsButton;
bool readReceiptsEnabled_;
bool notificationsEnabled_;
bool notificationsAttentionOnIncoming_;
bool circularAvatarsEnabled_;
NotificationMessageContentPolicy notificationMessageContentPolicy_ =
  NotificationMessageContentPolicy::WheneverAvailable;
bool communityNotificationCountsVisible_;
bool compactRoomList_;
bool roomListShowLastMessageTime_;
LastMessagePreview showLastMessagePreview_;
bool timelineMediaEffectsEnabled_;
bool uiAnimationsEnabled_;
bool windowFocusBlurEnabled_;
int windowFocusBlurDelaySeconds_;
bool shareKeysWithTrustedUsers_;
bool onlyShareKeysWithVerifiedUsers_;
bool onlineKeyBackupEnabled_;
bool touchInputModeEnabled_;
bool swipeGesturesEnabled_;
int maxContentWidth_  = settings::core::definitions::kDefaultUiLayoutContentMaxWidthPx;
int maxTimelineWidth_;
int roomListWidth_      = settings::core::definitions::kDefaultSidebarsRoomListWidthPx;
int communityListWidth_ = settings::core::definitions::kDefaultSidebarsCommunitiesWidthPx;
double scaleFactor_     = settings::core::definitions::kDefaultScaleFactor;
double baseFontSize_    = settings::core::definitions::kDefaultFontSizePt;
QString font_;
QString emojiFont_;
Presence presence_;
ShowImage showImage_;
QString ringtone_;
QString microphone_;
QString camera_;
QString cameraResolution_;
QString cameraFrameRate_;
int screenShareFrameRate_;
bool screenSharePiP_;
bool screenShareRemoteVideo_;
bool screenShareShowCursor_ = settings::core::definitions::kDefaultScreenShareShowCursor;
bool fallbackCallRelayServerEnabled_;
bool legacyCallsEnabled_;
bool certificateValidationEnabled_ =
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
bool identiconFallbackEnabled_;
bool openImagesInExternalApp_;
bool openVideosInExternalApp_;
int integrationsDbusApiAccess_ = settings::core::definitions::kDefaultIntegrationsDbusApiAccess;
QString integrationsLinksBrowserCommand_;
bool updateSpaceVias_;
bool expireEvents_;
int windowWidth_              = settings::core::definitions::kDefaultWindowWidthPx;
int windowHeight_             = settings::core::definitions::kDefaultWindowHeightPx;
qulonglong maxDbSize_         = settings::core::definitions::kDefaultMaxDbSizeBytes;
uint maxStores_               = settings::core::definitions::kDefaultMaxStores;
bool usesFileSecretsProvider_ = false;
bool http3Enabled_            = settings::core::definitions::kDefaultNetworkHttp3Enabled;
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
