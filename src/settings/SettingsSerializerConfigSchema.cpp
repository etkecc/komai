// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerConfigSchema.h"

#include <array>

#include <QString>

#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::serializer::config {

namespace {

const std::array<BoolSettingDescriptor, 40> BoolSettings{
  BoolSettingDescriptor{SettingKey::IntegrationsSystemTrayEnabled,
                        false,
                        &UserSettings::systemTrayEnabled,
                        &UserSettings::setSystemTrayEnabled},
  BoolSettingDescriptor{SettingKey::IntegrationsSystemTrayAutostart,
                        false,
                        &UserSettings::systemTrayAutostart,
                        &UserSettings::setSystemTrayAutostart},
  BoolSettingDescriptor{SettingKey::NotificationsDesktopEnabled,
                        true,
                        &UserSettings::desktopNotificationsEnabled,
                        &UserSettings::setDesktopNotificationsEnabled},
  BoolSettingDescriptor{SettingKey::NotificationsDesktopAlertOnIncoming,
                        false,
                        &UserSettings::alertOnIncomingMessages,
                        &UserSettings::setAlertOnIncomingMessages},
  BoolSettingDescriptor{SettingKey::NotificationsDesktopDecryptMessages,
                        true,
                        &UserSettings::decryptNotifications,
                        &UserSettings::setDecryptNotifications},
  BoolSettingDescriptor{SettingKey::SidebarsCommunitiesVisible,
                        true,
                        &UserSettings::communitiesSidebarVisible,
                        &UserSettings::setCommunitiesSidebarVisible},
  BoolSettingDescriptor{SettingKey::SidebarsRoomListScrollbarsEnabled,
                        true,
                        &UserSettings::roomListScrollbarsVisible,
                        &UserSettings::setRoomListScrollbarsVisible},
  BoolSettingDescriptor{SettingKey::TimelineMessageActionsEnabled,
                        true,
                        &UserSettings::timelineMessageActionsEnabled,
                        &UserSettings::setTimelineMessageActionsEnabled},
  BoolSettingDescriptor{SettingKey::TimelineMessagesHoverHighlight,
                        false,
                        &UserSettings::messageHoverHighlight,
                        &UserSettings::setMessageHoverHighlight},
  BoolSettingDescriptor{SettingKey::TimelineMessagesEmojiOnlyEnlarge,
                        true,
                        &UserSettings::enlargeEmojiOnlyMessages,
                        &UserSettings::setEnlargeEmojiOnlyMessages},
  BoolSettingDescriptor{SettingKey::TimelineMediaAnimateOnHover,
                        false,
                        &UserSettings::animateImagesOnHover,
                        &UserSettings::setAnimateImagesOnHover},
  BoolSettingDescriptor{SettingKey::ComposerFeedbackTypingNotifications,
                        true,
                        &UserSettings::typingNotificationsEnabled,
                        &UserSettings::setTypingNotificationsEnabled},
  BoolSettingDescriptor{SettingKey::ComposerFeedbackReadReceipts,
                        true,
                        &UserSettings::readReceiptsEnabled,
                        &UserSettings::setReadReceiptsEnabled},
  BoolSettingDescriptor{SettingKey::ComposerExtrasStickersEnabled,
                        false,
                        &UserSettings::stickersEnabled,
                        &UserSettings::setStickersEnabled},
  BoolSettingDescriptor{SettingKey::TimelineMediaEffectsEnabled,
                        true,
                        &UserSettings::timelineMediaEffectsEnabled,
                        &UserSettings::setTimelineMediaEffectsEnabled},
  BoolSettingDescriptor{SettingKey::UiAvatarsCircular,
                        false,
                        &UserSettings::circularAvatarsEnabled,
                        &UserSettings::setCircularAvatarsEnabled},
  BoolSettingDescriptor{SettingKey::UiAvatarsIdenticonFallback,
                        true,
                        &UserSettings::identiconFallbackEnabled,
                        &UserSettings::setIdenticonFallbackEnabled},
  BoolSettingDescriptor{SettingKey::SidebarsRoomListShowCommunityCounts,
                        true,
                        &UserSettings::communityNotificationCountsVisible,
                        &UserSettings::setCommunityNotificationCountsVisible},
  BoolSettingDescriptor{SettingKey::SidebarsRoomListCompact,
                        false,
                        &UserSettings::compactRoomList,
                        &UserSettings::setCompactRoomList},
  BoolSettingDescriptor{SettingKey::SidebarsRoomListShowLastMessageTime,
                        true,
                        &UserSettings::roomListShowLastMessageTime,
                        &UserSettings::setRoomListShowLastMessageTime},
  BoolSettingDescriptor{SettingKey::PrivacyScreenLockEnabled,
                        false,
                        &UserSettings::privacyScreen,
                        &UserSettings::setPrivacyScreen},
  BoolSettingDescriptor{SettingKey::EncryptionKeySharingOnlyVerifiedUsers,
                        false,
                        &UserSettings::onlyShareKeysWithVerifiedUsers,
                        &UserSettings::setOnlyShareKeysWithVerifiedUsers},
  BoolSettingDescriptor{SettingKey::EncryptionKeySharingShareWithTrusted,
                        false,
                        &UserSettings::shareKeysWithTrustedUsers,
                        &UserSettings::setShareKeysWithTrustedUsers},
  BoolSettingDescriptor{SettingKey::EncryptionBackupOnlineEnabled,
                        true,
                        &UserSettings::onlineKeyBackupEnabled,
                        &UserSettings::setOnlineKeyBackupEnabledFromConfig},
  BoolSettingDescriptor{SettingKey::NetworkTlsEnableCertificateValidation,
                        kDefaultCertificateValidationEnabled,
                        &UserSettings::certificateValidationEnabled,
                        &UserSettings::setCertificateValidationEnabled},
  BoolSettingDescriptor{SettingKey::NetworkHttp3Enabled,
                        kDefaultNetworkHttp3Enabled,
                        &UserSettings::http3Enabled,
                        &UserSettings::setHttp3Enabled},
  BoolSettingDescriptor{SettingKey::UiInputSwipeGestures,
                        false,
                        &UserSettings::swipeGesturesEnabled,
                        &UserSettings::setSwipeGesturesEnabled},
  BoolSettingDescriptor{SettingKey::TimelineMessagesLayoutBubbles,
                        true,
                        &UserSettings::timelineBubblesEnabled,
                        &UserSettings::setTimelineBubblesEnabled},
  BoolSettingDescriptor{SettingKey::TimelineMessagesLayoutSmallAvatars,
                        false,
                        &UserSettings::timelineSmallAvatarsEnabled,
                        &UserSettings::setTimelineSmallAvatarsEnabled},
  BoolSettingDescriptor{SettingKey::TimelineMessagesLayoutShowOwnAvatar,
                        true,
                        &UserSettings::timelineShowOwnAvatarInBubbleLayout,
                        &UserSettings::setTimelineShowOwnAvatarInBubbleLayout},
  BoolSettingDescriptor{SettingKey::CallsLegacyEnabled,
                        false,
                        &UserSettings::legacyCallsEnabled,
                        &UserSettings::setLegacyCallsEnabled},
  BoolSettingDescriptor{SettingKey::CallsScreensharePictureInPicture,
                        true,
                        &UserSettings::screenSharePiP,
                        &UserSettings::setScreenSharePiP},
  BoolSettingDescriptor{SettingKey::CallsScreenshareIncludeRemoteVideo,
                        false,
                        &UserSettings::screenShareRemoteVideo,
                        &UserSettings::setScreenShareRemoteVideo},
  BoolSettingDescriptor{SettingKey::CallsScreenshareHideCursor,
                        false,
                        &UserSettings::screenShareHideCursor,
                        &UserSettings::setScreenShareHideCursor},
  BoolSettingDescriptor{SettingKey::CallsRelayUseFallbackServer,
                        false,
                        &UserSettings::fallbackCallRelayServerEnabled,
                        &UserSettings::setFallbackCallRelayServerEnabled},
  BoolSettingDescriptor{SettingKey::ComposerInputMarkdownEnabled,
                        true,
                        &UserSettings::markdownEnabled,
                        &UserSettings::setMarkdownEnabled},
  BoolSettingDescriptor{SettingKey::TimelineMediaOpenImagesExternal,
                        false,
                        &UserSettings::openImagesInExternalApp,
                        &UserSettings::setOpenImagesInExternalApp},
  BoolSettingDescriptor{SettingKey::TimelineMediaOpenVideosExternal,
                        false,
                        &UserSettings::openVideosInExternalApp,
                        &UserSettings::setOpenVideosInExternalApp},
  BoolSettingDescriptor{SettingKey::PrivacyMaintenanceUpdateSpaceVias,
                        true,
                        &UserSettings::updateSpaceVias,
                        &UserSettings::setUpdateSpaceVias},
  BoolSettingDescriptor{SettingKey::PrivacyMaintenanceExpireEvents,
                        false,
                        &UserSettings::expireEvents,
                        &UserSettings::setExpireEvents},
};

const std::array<IntSettingDescriptor, 4> IntSettings{
  IntSettingDescriptor{SettingKey::TimelineMessagesMaxWidthPx,
                       kDefaultTimelineMaxWidthPx,
                       &UserSettings::maxTimelineWidth,
                       &UserSettings::setMaxTimelineWidth},
  IntSettingDescriptor{SettingKey::PrivacyScreenLockTimeoutSeconds,
                       kDefaultPrivacyScreenTimeoutSeconds,
                       &UserSettings::privacyScreenTimeoutSeconds,
                       &UserSettings::setPrivacyScreenTimeoutSeconds},
  IntSettingDescriptor{SettingKey::CallsScreenshareFrameRate,
                       kDefaultScreenShareFrameRate,
                       &UserSettings::screenShareFrameRate,
                       &UserSettings::setScreenShareFrameRate},
  IntSettingDescriptor{SettingKey::IntegrationsDbusApiAccess,
                       static_cast<int>(kDefaultDbusApiAccess),
                       &UserSettings::integrationsDbusApiAccess,
                       &UserSettings::setIntegrationsDbusApiAccess},
};

const std::array<UintSettingDescriptor, 1> UintSettings{
  UintSettingDescriptor{SettingKey::DbMaxFiles,
                        kDefaultMaxDbs,
                        &UserSettings::maxDbs,
                        &UserSettings::setMaxDbs},
};

const std::array<ULongLongSettingDescriptor, 1> ULongLongSettings{
  ULongLongSettingDescriptor{SettingKey::DbMaxSizeBytes,
                             kDefaultMaxDbSizeBytes,
                             &UserSettings::maxDbSize,
                             &UserSettings::setMaxDbSize},
};

const std::array<DoubleSettingDescriptor, 1> DoubleSettings{
  DoubleSettingDescriptor{SettingKey::UiFontSizePt,
                          kDefaultFontSizePt,
                          &UserSettings::fontSize,
                          &UserSettings::setFontSize},
};

const std::array<StringSettingDescriptor, 9> StringSettings{
  StringSettingDescriptor{SettingKey::UiFontFamily,
                          QString(),
                          &UserSettings::font,
                          &UserSettings::setFontFamily},
  StringSettingDescriptor{SettingKey::UiFontEmojiFamily,
                          QString(),
                          &UserSettings::emojiFontFamily,
                          &UserSettings::setEmojiFontFamily},
  StringSettingDescriptor{SettingKey::TimelineMessageActionsPinnedReactions,
                          QStringLiteral("👍️,👎️,😀,🤣,❤️"),
                          &UserSettings::pinnedReactions,
                          &UserSettings::setPinnedReactions},
  StringSettingDescriptor{SettingKey::CallsAudioRingtone,
                          QStringLiteral("Default"),
                          &UserSettings::ringtone,
                          &UserSettings::setRingtone},
  StringSettingDescriptor{SettingKey::CallsDevicesMicrophone,
                          QString(),
                          &UserSettings::microphone,
                          &UserSettings::setMicrophone},
  StringSettingDescriptor{SettingKey::CallsDevicesCamera,
                          QString(),
                          &UserSettings::camera,
                          &UserSettings::setCamera},
  StringSettingDescriptor{SettingKey::CallsDevicesCameraResolution,
                          QString(),
                          &UserSettings::cameraResolution,
                          &UserSettings::setCameraResolution},
  StringSettingDescriptor{SettingKey::CallsDevicesCameraFrameRate,
                          QString(),
                          &UserSettings::cameraFrameRate,
                          &UserSettings::setCameraFrameRate},
  StringSettingDescriptor{SettingKey::IntegrationsBrowserCommand,
                          QString(),
                          &UserSettings::integrationsLinksBrowserCommand,
                          &UserSettings::setIntegrationsLinksBrowserCommand},
};

} // namespace

std::span<const BoolSettingDescriptor>
boolConfigSettings()
{
    return BoolSettings;
}

std::span<const IntSettingDescriptor>
intConfigSettings()
{
    return IntSettings;
}

std::span<const UintSettingDescriptor>
uintConfigSettings()
{
    return UintSettings;
}

std::span<const ULongLongSettingDescriptor>
ulonglongConfigSettings()
{
    return ULongLongSettings;
}

std::span<const DoubleSettingDescriptor>
doubleConfigSettings()
{
    return DoubleSettings;
}

std::span<const StringSettingDescriptor>
stringConfigSettings()
{
    return StringSettings;
}

} // namespace settings::serializer::config
