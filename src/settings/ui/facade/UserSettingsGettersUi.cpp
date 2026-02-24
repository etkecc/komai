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
UserSettings::NotificationMessageContentPolicy
UserSettings::notificationMessageContentPolicy() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::NotificationsMessageContentPolicy);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::NotificationMessageContentPolicy::Never) &&
        *value <=
          static_cast<int>(UserSettings::NotificationMessageContentPolicy::WheneverAvailable))
        return static_cast<UserSettings::NotificationMessageContentPolicy>(*value);
    return notificationMessageContentPolicy_;
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
UserSettings::windowFocusBlurEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::PrivacyWindowFocusBlurEnabled);
        value.has_value())
        return *value;
    return windowFocusBlurEnabled_;
}
int
UserSettings::windowFocusBlurDelaySeconds() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::PrivacyWindowFocusBlurDelaySeconds);
        value.has_value())
        return *value;
    return windowFocusBlurDelaySeconds_;
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
UserSettings::TimelineMessageLayout
UserSettings::timelineMessageLayout() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesLayoutStyle);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TimelineMessageLayout::Minimal) &&
        *value <= static_cast<int>(UserSettings::TimelineMessageLayout::Bubbles))
        return static_cast<UserSettings::TimelineMessageLayout>(*value);
    return timelineMessageLayout_;
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
UserSettings::sendTypingNotificationsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerTypingSendEnabled);
        value.has_value())
        return *value;
    return sendTypingNotificationsEnabled_;
}
bool
UserSettings::showTypingNotificationsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineTypingShowEnabled);
        value.has_value())
        return *value;
    return showTypingNotificationsEnabled_;
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
UserSettings::TimelineMessageActionsPolicy
UserSettings::timelineMessageActionsPolicy() const
{
    if (const auto value = coreStore_.valueAs<int>(
          settings::core::SettingId::TimelineMessageActionsActivationPolicy);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TimelineMessageActionsPolicy::OnHover) &&
        *value <= static_cast<int>(UserSettings::TimelineMessageActionsPolicy::Never))
        return static_cast<UserSettings::TimelineMessageActionsPolicy>(*value);
    return timelineMessageActionsPolicy_;
}
bool
UserSettings::touchInputModeEnabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::UiInputMode);
        value.has_value())
        return *value;
    return touchInputModeEnabled_;
}
bool
UserSettings::swipeGesturesEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::UiInputTouchSwipeGesturesEnabled);
        value.has_value())
        return *value;
    return swipeGesturesEnabled_;
}
bool
UserSettings::readReceiptsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineReadReceiptsEnabled);
        value.has_value())
        return *value;
    return readReceiptsEnabled_;
}
bool
UserSettings::notificationsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::NotificationsEnabled);
        value.has_value())
        return *value;
    return notificationsEnabled_;
}
bool
UserSettings::notificationsAttentionOnIncoming() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::NotificationsAttentionOnIncoming);
        value.has_value())
        return *value;
    return notificationsAttentionOnIncoming_;
}
bool
UserSettings::hasNotifications() const
{
    return notificationsEnabled() || notificationsAttentionOnIncoming();
}
int
UserSettings::maxContentWidth() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::UiLayoutContentMaxWidthPx);
        value.has_value())
        return settings::core::definitions::normalizeUiLayoutContentMaxWidthPx(*value);
    return settings::core::definitions::normalizeUiLayoutContentMaxWidthPx(maxContentWidth_);
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
