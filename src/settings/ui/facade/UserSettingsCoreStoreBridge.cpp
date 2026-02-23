// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/facade/UserSettingsCoreStoreBridge.h"

#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::ui::facade {

std::optional<settings::core::SettingsStore::Value>
coreStoreValueForSettingId(const UserSettings &settings, settings::core::SettingId id)
{
    switch (id) {
    case settings::core::SettingId::UiThemeSlug:
        return settings.theme().toStdString();
    case settings::core::SettingId::UiFontFamily:
        return settings.font().toStdString();
    case settings::core::SettingId::UiFontSizePt:
        return settings.fontSize();
    case settings::core::SettingId::UiFontEmojiFamily:
        return settings.emojiFontFamily().toStdString();
    case settings::core::SettingId::UiMotionAnimationsEnabled:
        return settings.uiAnimationsEnabled();
    case settings::core::SettingId::UiInputEnableTextSelection:
        return settings.textSelectionEnabled();
    case settings::core::SettingId::UiInputSwipeGestures:
        return settings.swipeGesturesEnabled();
    case settings::core::SettingId::UiAvatarsCircular:
        return settings.circularAvatarsEnabled();
    case settings::core::SettingId::UiAvatarsIdenticonFallback:
        return settings.identiconFallbackEnabled();
    case settings::core::SettingId::SidebarsRoomListCompact:
        return settings.compactRoomList();
    case settings::core::SettingId::SidebarsRoomListShowLastMessageTime:
        return settings.roomListShowLastMessageTime();
    case settings::core::SettingId::SidebarsRoomListLastMessagePreview:
        return static_cast<int>(settings.showLastMessagePreview());
    case settings::core::SettingId::SidebarsRoomListShowCommunityCounts:
        return settings.communityNotificationCountsVisible();
    case settings::core::SettingId::SidebarsRoomListScrollbarsEnabled:
        return settings.roomListScrollbarsVisible();
    case settings::core::SettingId::SidebarsRoomListSort:
        return static_cast<int>(settings.roomSortOrder());
    case settings::core::SettingId::SidebarsCommunitiesVisible:
        return settings.communitiesSidebarVisible();
    case settings::core::SettingId::NetworkPresenceStatusPolicy:
        return static_cast<int>(settings.presence());
    case settings::core::SettingId::PrivacyMaintenanceExpireEvents:
        return settings.expireEvents();
    case settings::core::SettingId::PrivacyMaintenanceUpdateSpaceVias:
        return settings.updateSpaceVias();
    case settings::core::SettingId::PrivacyScreenLockEnabled:
        return settings.privacyScreen();
    case settings::core::SettingId::PrivacyScreenLockTimeoutSeconds:
        return settings.privacyScreenTimeoutSeconds();
    case settings::core::SettingId::IntegrationsSystemTrayEnabled:
        return settings.systemTrayEnabled();
    case settings::core::SettingId::IntegrationsSystemTrayAutostart:
        return settings.systemTrayAutostart();
    case settings::core::SettingId::IntegrationsDbusApiAccess:
        return settings.integrationsDbusApiAccess();
    case settings::core::SettingId::IntegrationsBrowserCommand:
        return settings.integrationsLinksBrowserCommand().toStdString();
    case settings::core::SettingId::ComposerInputMarkdownEnabled:
        return settings.markdownEnabled();
    case settings::core::SettingId::ComposerInputSendKey:
        return static_cast<int>(settings.sendMessageKey());
    case settings::core::SettingId::ComposerInputAutoReplaceEmoji:
        return static_cast<int>(settings.autoReplaceEmoji());
    case settings::core::SettingId::ComposerFeedbackTypingNotifications:
        return settings.typingNotificationsEnabled();
    case settings::core::SettingId::ComposerFeedbackReadReceipts:
        return settings.readReceiptsEnabled();
    case settings::core::SettingId::ComposerExtrasStickersEnabled:
        return settings.stickersEnabled();
    case settings::core::SettingId::NotificationsDesktopEnabled:
        return settings.desktopNotificationsEnabled();
    case settings::core::SettingId::NotificationsDesktopAlertOnIncoming:
        return settings.alertOnIncomingMessages();
    case settings::core::SettingId::NotificationsDesktopDecryptMessages:
        return settings.decryptNotifications();
    case settings::core::SettingId::CallsLegacyEnabled:
        return settings.legacyCallsEnabled();
    case settings::core::SettingId::CallsRelayUseFallbackServer:
        return settings.fallbackCallRelayServerEnabled();
    case settings::core::SettingId::CallsDevicesMicrophone:
        return settings.microphone().toStdString();
    case settings::core::SettingId::CallsDevicesCamera:
        return settings.camera().toStdString();
    case settings::core::SettingId::CallsDevicesCameraResolution:
        return settings.cameraResolution().toStdString();
    case settings::core::SettingId::CallsDevicesCameraFrameRate:
        return settings.cameraFrameRate().toStdString();
    case settings::core::SettingId::CallsAudioRingtone:
        return settings.ringtone().toStdString();
    case settings::core::SettingId::TimelineMessagesLayoutBubbles:
        return settings.timelineBubblesEnabled();
    case settings::core::SettingId::TimelineMessagesLayoutSmallAvatars:
        return settings.timelineSmallAvatarsEnabled();
    case settings::core::SettingId::TimelineMessagesLayoutShowOwnAvatar:
        return settings.timelineShowOwnAvatarInBubbleLayout();
    case settings::core::SettingId::TimelineMessagesSenderUsername:
        return static_cast<int>(settings.showSenderUsername());
    case settings::core::SettingId::TimelineMessagesMaxWidthPx:
        return settings.maxTimelineWidth();
    case settings::core::SettingId::TimelineMessagesEmojiOnlyEnlarge:
        return settings.enlargeEmojiOnlyMessages();
    case settings::core::SettingId::TimelineMessagesHoverHighlight:
        return settings.messageHoverHighlight();
    case settings::core::SettingId::TimelineMessageActionsEnabled:
        return settings.timelineMessageActionsEnabled();
    case settings::core::SettingId::TimelineMessageActionsPinnedReactions:
        return settings.pinnedReactions().toStdString();
    case settings::core::SettingId::TimelineMediaEffectsEnabled:
        return settings.timelineMediaEffectsEnabled();
    case settings::core::SettingId::TimelineMediaAnimateOnHover:
        return settings.animateImagesOnHover();
    case settings::core::SettingId::TimelineMediaImageDisplay:
        return static_cast<int>(settings.showImage());
    case settings::core::SettingId::TimelineMediaOpenImagesExternal:
        return settings.openImagesInExternalApp();
    case settings::core::SettingId::TimelineMediaOpenVideosExternal:
        return settings.openVideosInExternalApp();
    case settings::core::SettingId::EncryptionKeySharingOnlyVerifiedUsers:
        return settings.onlyShareKeysWithVerifiedUsers();
    case settings::core::SettingId::EncryptionKeySharingShareWithTrusted:
        return settings.shareKeysWithTrustedUsers();
    case settings::core::SettingId::EncryptionBackupOnlineEnabled:
        return settings.onlineKeyBackupEnabled();
    default:
        return std::nullopt;
    }
}

} // namespace settings::ui::facade
