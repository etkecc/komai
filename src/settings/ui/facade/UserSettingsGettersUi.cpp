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
UserSettings::sidebarsRoomListLastMessagePreview() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::SidebarsRoomListLastMessagePreview);
        value.has_value() && *value >= static_cast<int>(UserSettings::LastMessagePreview::Always) &&
        *value <= static_cast<int>(UserSettings::LastMessagePreview::Never))
        return static_cast<UserSettings::LastMessagePreview>(*value);
    return sidebarsRoomListLastMessagePreview_;
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

UserSettings::RoomSortOrder
UserSettings::sidebarsRoomListSort() const
{
    if (const auto value = coreStore_.valueAs<int>(settings::core::SettingId::SidebarsRoomListSort);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::RoomSortOrder::UnreadFirst_Recent) &&
        *value <= static_cast<int>(UserSettings::RoomSortOrder::Alphabetical))
        return static_cast<UserSettings::RoomSortOrder>(*value);
    return sidebarsRoomListSort_;
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
