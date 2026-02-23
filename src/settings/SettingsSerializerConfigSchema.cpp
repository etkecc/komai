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

const std::array<BoolSettingDescriptor, 38> BoolSettings{
  BoolSettingDescriptor{SettingKey::IntegrationsSystemTrayEnabled,
                        false,
                        &UserSettings::systemTrayEnabled,
                        &UserSettings::setSystemTrayEnabled},
  BoolSettingDescriptor{SettingKey::IntegrationsSystemTrayAutostart,
                        false,
                        &UserSettings::systemTrayAutostart,
                        &UserSettings::setSystemTrayAutostart},
  BoolSettingDescriptor{SettingKey::NotificationsEnabled,
                        true,
                        &UserSettings::notificationsEnabled,
                        &UserSettings::setNotificationsEnabled},
  BoolSettingDescriptor{SettingKey::NotificationsAttentionOnIncoming,
                        false,
                        &UserSettings::notificationsAttentionOnIncoming,
                        &UserSettings::setNotificationsAttentionOnIncoming},
  BoolSettingDescriptor{SettingKey::SidebarsCommunitiesVisible,
                        true,
                        &UserSettings::communitiesSidebarVisible,
                        &UserSettings::setCommunitiesSidebarVisible},
  BoolSettingDescriptor{SettingKey::SidebarsRoomListScrollbarsEnabled,
                        true,
                        &UserSettings::roomListScrollbarsVisible,
                        &UserSettings::setRoomListScrollbarsVisible},
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
  BoolSettingDescriptor{SettingKey::ComposerTypingSendEnabled,
                        true,
                        &UserSettings::sendTypingNotificationsEnabled,
                        &UserSettings::setSendTypingNotificationsEnabled},
  BoolSettingDescriptor{SettingKey::TimelineTypingShowEnabled,
                        true,
                        &UserSettings::showTypingNotificationsEnabled,
                        &UserSettings::setShowTypingNotificationsEnabled},
  BoolSettingDescriptor{SettingKey::TimelineReadReceiptsEnabled,
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
  BoolSettingDescriptor{SettingKey::PrivacyWindowFocusBlurEnabled,
                        false,
                        &UserSettings::windowFocusBlurEnabled,
                        &UserSettings::setWindowFocusBlurEnabled},
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
  BoolSettingDescriptor{SettingKey::UiInputTouchSwipeGesturesEnabled,
                        false,
                        &UserSettings::swipeGesturesEnabled,
                        &UserSettings::setSwipeGesturesEnabled},
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
  BoolSettingDescriptor{SettingKey::CallsScreenshareShowCursor,
                        settings::core::definitions::kDefaultScreenShareShowCursor,
                        &UserSettings::screenShareShowCursor,
                        &UserSettings::setScreenShareShowCursor},
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
  IntSettingDescriptor{SettingKey::PrivacyWindowFocusBlurDelaySeconds,
                       kDefaultPrivacyWindowFocusBlurDelaySeconds,
                       &UserSettings::windowFocusBlurDelaySeconds,
                       &UserSettings::setWindowFocusBlurDelaySeconds},
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
  UintSettingDescriptor{SettingKey::DbMaxStores,
                        kDefaultMaxStores,
                        &UserSettings::maxStores,
                        &UserSettings::setMaxStores},
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
                          QString::fromUtf8(settings::core::definitions::kDefaultPinnedReactions),
                          &UserSettings::pinnedReactions,
                          &UserSettings::setPinnedReactions},
  StringSettingDescriptor{
    SettingKey::CallsAudioRingtone,
    QString::fromLatin1(settings::core::definitions::kDefaultCallsAudioRingtone),
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
