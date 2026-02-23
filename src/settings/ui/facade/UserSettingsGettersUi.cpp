// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/facade/UserSettingsPage.h"

QString
UserSettings::theme() const
{
    if (const auto value = coreStore_.valueAs<std::string>(settings::core::SettingId::UiThemeSlug);
        value.has_value() && !value->empty())
        return QString::fromStdString(*value);
    return !theme_.isEmpty() ? theme_ : defaultTheme_;
}
bool
UserSettings::messageHoverHighlight() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesHoverHighlight);
        value.has_value())
        return *value;
    return messageHoverHighlight_;
}
bool
UserSettings::enlargeEmojiOnlyMessages() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesEmojiOnlyEnlarge);
        value.has_value())
        return *value;
    return enlargeEmojiOnlyMessages_;
}
bool
UserSettings::systemTrayEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::IntegrationsSystemTrayEnabled);
        value.has_value())
        return *value;
    return systemTrayEnabled_;
}
bool
UserSettings::systemTrayAutostart() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::IntegrationsSystemTrayAutostart);
        value.has_value())
        return *value;
    return systemTrayAutostart_;
}
bool
UserSettings::communitiesSidebarVisible() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsCommunitiesVisible);
        value.has_value())
        return *value;
    return communitiesSidebarVisible_;
}
bool
UserSettings::roomListScrollbarsVisible() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsRoomListScrollbarsEnabled);
        value.has_value())
        return *value;
    return roomListScrollbarsVisible_;
}
bool
UserSettings::circularAvatarsEnabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::UiAvatarsCircular);
        value.has_value())
        return *value;
    return circularAvatarsEnabled_;
}
bool
UserSettings::decryptNotifications() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::NotificationsDesktopDecryptMessages);
        value.has_value())
        return *value;
    return decryptNotifications_;
}
bool
UserSettings::communityNotificationCountsVisible() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsRoomListShowCommunityCounts);
        value.has_value())
        return *value;
    return communityNotificationCountsVisible_;
}
bool
UserSettings::compactRoomList() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsRoomListCompact);
        value.has_value())
        return *value;
    return compactRoomList_;
}
bool
UserSettings::roomListShowLastMessageTime() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsRoomListShowLastMessageTime);
        value.has_value())
        return *value;
    return roomListShowLastMessageTime_;
}
UserSettings::LastMessagePreview
UserSettings::showLastMessagePreview() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::SidebarsRoomListLastMessagePreview);
        value.has_value() && *value >= static_cast<int>(UserSettings::LastMessagePreview::Always) &&
        *value <= static_cast<int>(UserSettings::LastMessagePreview::Never))
        return static_cast<UserSettings::LastMessagePreview>(*value);
    return showLastMessagePreview_;
}
bool
UserSettings::timelineMediaEffectsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaEffectsEnabled);
        value.has_value())
        return *value;
    return timelineMediaEffectsEnabled_;
}
bool
UserSettings::uiAnimationsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::UiMotionAnimationsEnabled);
        value.has_value())
        return *value;
    return uiAnimationsEnabled_;
}
bool
UserSettings::privacyScreen() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::PrivacyScreenLockEnabled);
        value.has_value())
        return *value;
    return privacyScreen_;
}
int
UserSettings::privacyScreenTimeoutSeconds() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::PrivacyScreenLockTimeoutSeconds);
        value.has_value())
        return *value;
    return privacyScreenTimeoutSeconds_;
}
bool
UserSettings::markdownEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerInputMarkdownEnabled);
        value.has_value())
        return *value;
    return markdownEnabled_;
}
UserSettings::SendMessageKey
UserSettings::sendMessageKey() const
{
    if (const auto value = coreStore_.valueAs<int>(settings::core::SettingId::ComposerInputSendKey);
        value.has_value() && *value >= static_cast<int>(UserSettings::SendMessageKey::Enter) &&
        *value <= static_cast<int>(UserSettings::SendMessageKey::CtrlEnter))
        return static_cast<UserSettings::SendMessageKey>(*value);
    return sendMessageKey_;
}
UserSettings::AutoReplaceEmoji
UserSettings::autoReplaceEmoji() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::ComposerInputAutoReplaceEmoji);
        value.has_value() && *value >= static_cast<int>(UserSettings::AutoReplaceEmoji::Always) &&
        *value <= static_cast<int>(UserSettings::AutoReplaceEmoji::Never))
        return static_cast<UserSettings::AutoReplaceEmoji>(*value);
    return autoReplaceEmoji_;
}
bool
UserSettings::timelineBubblesEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesLayoutBubbles);
        value.has_value())
        return *value;
    return timelineBubblesEnabled_;
}
bool
UserSettings::timelineSmallAvatarsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesLayoutSmallAvatars);
        value.has_value())
        return *value;
    return timelineSmallAvatarsEnabled_;
}
bool
UserSettings::stickersEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerExtrasStickersEnabled);
        value.has_value())
        return *value;
    return stickersEnabled_;
}
bool
UserSettings::timelineShowOwnAvatarInBubbleLayout() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesLayoutShowOwnAvatar);
        value.has_value())
        return *value;
    return timelineShowOwnAvatarInBubbleLayout_;
}
QString
UserSettings::pinnedReactions() const
{
    if (const auto value = coreStore_.valueAs<std::string>(
          settings::core::SettingId::TimelineMessageActionsPinnedReactions);
        value.has_value())
        return QString::fromStdString(*value);
    return pinnedReactions_;
}
UserSettings::ShowSenderUsername
UserSettings::showSenderUsername() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesSenderUsername);
        value.has_value() && *value >= static_cast<int>(UserSettings::ShowSenderUsername::Always) &&
        *value <= static_cast<int>(UserSettings::ShowSenderUsername::Never))
        return static_cast<UserSettings::ShowSenderUsername>(*value);
    return showSenderUsername_;
}
int
UserSettings::showSenderUsernameLargeRoomThreshold() const
{
    return 16;
}
bool
UserSettings::animateImagesOnHover() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaAnimateOnHover);
        value.has_value())
        return *value;
    return animateImagesOnHover_;
}
bool
UserSettings::typingNotificationsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerFeedbackTypingNotifications);
        value.has_value())
        return *value;
    return typingNotificationsEnabled_;
}
UserSettings::RoomSortOrder
UserSettings::roomSortOrder() const
{
    if (const auto value = coreStore_.valueAs<int>(settings::core::SettingId::SidebarsRoomListSort);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::RoomSortOrder::UnreadFirst_Recent) &&
        *value <= static_cast<int>(UserSettings::RoomSortOrder::Alphabetical))
        return static_cast<UserSettings::RoomSortOrder>(*value);
    return roomSortOrder_;
}
bool
UserSettings::timelineMessageActionsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessageActionsEnabled);
        value.has_value())
        return *value;
    return timelineMessageActionsEnabled_;
}
bool
UserSettings::textSelectionEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::UiInputEnableTextSelection);
        value.has_value())
        return *value;
    return textSelectionEnabled_;
}
bool
UserSettings::swipeGesturesEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::UiInputSwipeGestures);
        value.has_value())
        return *value;
    return swipeGesturesEnabled_;
}
bool
UserSettings::readReceiptsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerFeedbackReadReceipts);
        value.has_value())
        return *value;
    return readReceiptsEnabled_;
}
bool
UserSettings::desktopNotificationsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::NotificationsDesktopEnabled);
        value.has_value())
        return *value;
    return desktopNotificationsEnabled_;
}
bool
UserSettings::alertOnIncomingMessages() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::NotificationsDesktopAlertOnIncoming);
        value.has_value())
        return *value;
    return alertOnIncomingMessages_;
}
bool
UserSettings::hasNotifications() const
{
    return desktopNotificationsEnabled() || alertOnIncomingMessages();
}
int
UserSettings::maxTimelineWidth() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesMaxWidthPx);
        value.has_value())
        return *value;
    return maxTimelineWidth_;
}
