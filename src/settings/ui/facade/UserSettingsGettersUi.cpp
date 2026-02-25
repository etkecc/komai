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
UserSettings::integrationsSystemTrayEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::IntegrationsSystemTrayEnabled);
        value.has_value())
        return *value;
    return integrationsSystemTrayEnabled_;
}

bool
UserSettings::integrationsSystemTrayAutostart() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::IntegrationsSystemTrayAutostart);
        value.has_value())
        return *value;
    return integrationsSystemTrayAutostart_;
}

bool
UserSettings::sidebarsCommunitiesVisible() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsCommunitiesVisible);
        value.has_value())
        return *value;
    return sidebarsCommunitiesVisible_;
}

bool
UserSettings::sidebarsRoomListScrollbarsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsRoomListScrollbarsEnabled);
        value.has_value())
        return *value;
    return sidebarsRoomListScrollbarsEnabled_;
}

bool
UserSettings::uiAvatarsCircular() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::UiAvatarsCircular);
        value.has_value())
        return *value;
    return uiAvatarsCircular_;
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
UserSettings::sidebarsRoomListShowCommunityCounts() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsRoomListShowCommunityCounts);
        value.has_value())
        return *value;
    return sidebarsRoomListShowCommunityCounts_;
}

bool
UserSettings::sidebarsRoomListCompact() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsRoomListCompact);
        value.has_value())
        return *value;
    return sidebarsRoomListCompact_;
}

bool
UserSettings::sidebarsRoomListShowLastMessageTime() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsRoomListShowLastMessageTime);
        value.has_value())
        return *value;
    return sidebarsRoomListShowLastMessageTime_;
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
UserSettings::uiMotionAnimationsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::UiMotionAnimationsEnabled);
        value.has_value())
        return *value;
    return uiMotionAnimationsEnabled_;
}

bool
UserSettings::privacyWindowFocusBlurEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::PrivacyWindowFocusBlurEnabled);
        value.has_value())
        return *value;
    return privacyWindowFocusBlurEnabled_;
}

int
UserSettings::privacyWindowFocusBlurDelaySeconds() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::PrivacyWindowFocusBlurDelaySeconds);
        value.has_value())
        return *value;
    return privacyWindowFocusBlurDelaySeconds_;
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
UserSettings::uiInputModeTouchEnabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::UiInputMode);
        value.has_value())
        return *value;
    return uiInputModeTouchEnabled_;
}

bool
UserSettings::uiInputTouchSwipeGesturesEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::UiInputTouchSwipeGesturesEnabled);
        value.has_value())
        return *value;
    return uiInputTouchSwipeGesturesEnabled_;
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
UserSettings::uiLayoutContentMaxWidthPx() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::UiLayoutContentMaxWidthPx);
        value.has_value())
        return settings::core::definitions::normalizeUiLayoutContentMaxWidthPx(*value);
    return settings::core::definitions::normalizeUiLayoutContentMaxWidthPx(
      uiLayoutContentMaxWidthPx_);
}
