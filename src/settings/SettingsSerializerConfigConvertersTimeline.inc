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

constexpr std::array<std::pair<UserSettings::TimelineMessagesStyle, const char *>, 2>
  kTimelineStyleTokens{{
    {UserSettings::TimelineMessagesStyle::Plain, "plain"},
    {UserSettings::TimelineMessagesStyle::Bubbles, "bubbles"},
  }};

constexpr std::array<std::pair<UserSettings::TimelineMessagesLayoutPositioning, const char *>, 4>
  kTimelinePositioningTokens{{
    {UserSettings::TimelineMessagesLayoutPositioning::Adaptive, "adaptive"},
    {UserSettings::TimelineMessagesLayoutPositioning::OpposingBySender, "opposing_by_sender"},
    {UserSettings::TimelineMessagesLayoutPositioning::AllLeft, "all_left"},
    {UserSettings::TimelineMessagesLayoutPositioning::AllRight, "all_right"},
  }};

constexpr std::array<std::pair<UserSettings::TimelineUserColorCodingPolicy, const char *>, 2>
  kTimelineUserColorCodingPolicyTokens{{
    {UserSettings::TimelineUserColorCodingPolicy::AdaptiveByRoomSize,
     "adaptive_by_room_size"},
    {UserSettings::TimelineUserColorCodingPolicy::MeVsOthers, "me_vs_others"},
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
toStorageValue(UserSettings::TimelineMessagesStyle value)
{
    return valueToStorageToken(value, kTimelineStyleTokens, "bubbles");
}

UserSettings::TimelineMessagesStyle
timelineMessagesStyleFromStorage(const QString &value,
                                       UserSettings::TimelineMessagesStyle fallback)
{
    return valueFromStorageToken(value, fallback, kTimelineStyleTokens);
}

QString
toStorageValue(UserSettings::TimelineMessagesLayoutPositioning value)
{
    return valueToStorageToken(value, kTimelinePositioningTokens, "adaptive");
}

UserSettings::TimelineMessagesLayoutPositioning
timelineMessagesLayoutPositioningFromStorage(
  const QString &value,
  UserSettings::TimelineMessagesLayoutPositioning fallback)
{
    return valueFromStorageToken(value, fallback, kTimelinePositioningTokens);
}

QString
toStorageValue(UserSettings::TimelineUserColorCodingPolicy value)
{
    return valueToStorageToken(
      value, kTimelineUserColorCodingPolicyTokens, "adaptive_by_room_size");
}

UserSettings::TimelineUserColorCodingPolicy
timelineUserColorCodingPolicyFromStorage(
  const QString &value,
  UserSettings::TimelineUserColorCodingPolicy fallback)
{
    return valueFromStorageToken(value, fallback, kTimelineUserColorCodingPolicyTokens);
}
