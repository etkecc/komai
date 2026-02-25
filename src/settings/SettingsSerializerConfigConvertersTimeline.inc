// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

constexpr std::array<std::pair<UserSettings::TimelineMessageActionsActivationPolicy, const char *>,
                     3>
  kTimelineActionsPolicyTokens{{
    {UserSettings::TimelineMessageActionsActivationPolicy::OnHover, "on_message_hover"},
    {UserSettings::TimelineMessageActionsActivationPolicy::ActionsButton, "on_button_click"},
    {UserSettings::TimelineMessageActionsActivationPolicy::Never, "never"},
  }};

constexpr std::array<std::pair<UserSettings::TimelineMessagesLayoutStyle, const char *>, 2>
  kTimelineLayoutTokens{{
    {UserSettings::TimelineMessagesLayoutStyle::Minimal, "minimal"},
    {UserSettings::TimelineMessagesLayoutStyle::Bubbles, "bubbles"},
  }};

QString
toStorageValue(UserSettings::TimelineMessageActionsActivationPolicy value)
{
    return valueToStorageToken(value, kTimelineActionsPolicyTokens, "on_button_click");
}

UserSettings::TimelineMessageActionsActivationPolicy
timelineMessageActionsActivationPolicyFromStorage(
  const QString &value,
  UserSettings::TimelineMessageActionsActivationPolicy fallback)
{
    return valueFromStorageToken(value, fallback, kTimelineActionsPolicyTokens);
}

QString
toStorageValue(UserSettings::TimelineMessagesLayoutStyle value)
{
    return valueToStorageToken(value, kTimelineLayoutTokens, "bubbles");
}

UserSettings::TimelineMessagesLayoutStyle
timelineMessagesLayoutStyleFromStorage(const QString &value,
                                       UserSettings::TimelineMessagesLayoutStyle fallback)
{
    return valueFromStorageToken(value, fallback, kTimelineLayoutTokens);
}
