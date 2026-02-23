// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Default to system theme if QT_QPA_PLATFORMTHEME var is set.
QString defaultTheme_ = QProcessEnvironment::systemEnvironment()
                            .value(QStringLiteral("QT_QPA_PLATFORMTHEME"), QLatin1String(""))
                            .isEmpty()
                          ? "komai-light"
                          : "komai-light";
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
bool timelineBubblesEnabled_;
bool timelineSmallAvatarsEnabled_;
bool stickersEnabled_;
bool timelineShowOwnAvatarInBubbleLayout_;
QString pinnedReactions_;
ShowSenderUsername showSenderUsername_;
bool animateImagesOnHover_;
bool typingNotificationsEnabled_;
RoomSortOrder roomSortOrder_;
bool timelineMessageActionsEnabled_;
bool readReceiptsEnabled_;
bool desktopNotificationsEnabled_;
bool alertOnIncomingMessages_;
bool circularAvatarsEnabled_;
bool decryptNotifications_;
bool communityNotificationCountsVisible_;
bool compactRoomList_;
bool roomListShowLastMessageTime_;
LastMessagePreview showLastMessagePreview_;
bool timelineMediaEffectsEnabled_;
bool uiAnimationsEnabled_;
bool privacyScreen_;
int privacyScreenTimeoutSeconds_;
bool shareKeysWithTrustedUsers_;
bool onlyShareKeysWithVerifiedUsers_;
bool onlineKeyBackupEnabled_;
bool textSelectionEnabled_;
bool swipeGesturesEnabled_;
int maxTimelineWidth_;
int roomListWidth_      = -1;
int communityListWidth_ = 200;
double scaleFactor_     = -1.0;
double baseFontSize_    = 13.0;
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
bool screenShareHideCursor_;
bool fallbackCallRelayServerEnabled_;
bool legacyCallsEnabled_;
bool certificateValidationEnabled_ = true;
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
int integrationsDbusApiAccess_ = 0;
QString integrationsLinksBrowserCommand_;
bool updateSpaceVias_;
bool expireEvents_;
int windowWidth_                 = 0;
int windowHeight_                = 0;
qulonglong maxDbSize_            = 0;
uint maxDbs_                     = 0;
bool secretsFileProviderEnabled_ = false;
bool http3Enabled_               = false;
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
