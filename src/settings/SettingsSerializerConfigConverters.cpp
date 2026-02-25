// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerConfigConverters.h"

#include <array>

#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::serializer::config {

namespace {

template<typename ValueT, std::size_t N>
QString
valueToStorageToken(ValueT value,
                    const std::array<std::pair<ValueT, const char *>, N> &tokenMap,
                    const char *fallbackToken)
{
    for (const auto &[candidate, token] : tokenMap) {
        if (candidate == value)
            return QString::fromLatin1(token);
    }

    return QString::fromLatin1(fallbackToken);
}

template<typename ValueT, std::size_t N>
ValueT
valueFromStorageToken(const QString &value,
                      ValueT fallback,
                      const std::array<std::pair<ValueT, const char *>, N> &tokenMap)
{
    for (const auto &[candidate, token] : tokenMap) {
        if (value == QLatin1String(token))
            return candidate;
    }

    return fallback;
}

constexpr std::array<std::pair<UserSettings::Presence, const char *>, 4> kPresenceTokens{{
  {UserSettings::Presence::AutomaticPresence, "automatic_presence"},
  {UserSettings::Presence::Online, "online"},
  {UserSettings::Presence::Unavailable, "unavailable"},
  {UserSettings::Presence::Offline, "offline"},
}};

constexpr std::array<std::pair<UserSettings::ShowImage, const char *>, 3> kShowImageTokens{{
  {UserSettings::ShowImage::Always, "always"},
  {UserSettings::ShowImage::OnlyPrivate, "only_private"},
  {UserSettings::ShowImage::Never, "never"},
}};

constexpr std::array<std::pair<UserSettings::ShowSenderUsername, const char *>, 3>
  kShowSenderUsernameTokens{{
    {UserSettings::ShowSenderUsername::Always, "always"},
    {UserSettings::ShowSenderUsername::OnlyInLargeRooms, "only_in_large_rooms"},
    {UserSettings::ShowSenderUsername::Never, "never"},
  }};

constexpr std::array<std::pair<UserSettings::AutoReplaceEmoji, const char *>, 3>
  kAutoReplaceEmojiTokens{{
    {UserSettings::AutoReplaceEmoji::Always, "always"},
    {UserSettings::AutoReplaceEmoji::OnlyAtEnd, "only_at_end"},
    {UserSettings::AutoReplaceEmoji::Never, "never"},
  }};

constexpr std::array<std::pair<UserSettings::SendMessageKey, const char *>, 3>
  kSendMessageKeyTokens{{
    {UserSettings::SendMessageKey::Enter, "enter"},
    {UserSettings::SendMessageKey::ShiftEnter, "shift_enter"},
    {UserSettings::SendMessageKey::CtrlEnter, "ctrl_enter"},
  }};

constexpr std::array<std::pair<UserSettings::RoomSortOrder, const char *>, 4> kRoomSortOrderTokens{{
  {UserSettings::RoomSortOrder::UnreadFirst_Recent, "unread_first_recent"},
  {UserSettings::RoomSortOrder::UnreadFirst_Alpha, "unread_first_alpha"},
  {UserSettings::RoomSortOrder::Recent, "recent"},
  {UserSettings::RoomSortOrder::Alphabetical, "alphabetical"},
}};

constexpr std::array<std::pair<UserSettings::LastMessagePreview, const char *>, 3>
  kLastMessagePreviewTokens{{
    {UserSettings::LastMessagePreview::Always, "always"},
    {UserSettings::LastMessagePreview::OnlyUnencrypted, "only_unencrypted"},
    {UserSettings::LastMessagePreview::Never, "never"},
  }};

constexpr std::array<std::pair<UserSettings::TimelineMessageActionsPolicy, const char *>, 3>
  kTimelineActionsPolicyTokens{{
    {UserSettings::TimelineMessageActionsPolicy::OnHover, "on_message_hover"},
    {UserSettings::TimelineMessageActionsPolicy::ActionsButton, "on_button_click"},
    {UserSettings::TimelineMessageActionsPolicy::Never, "never"},
  }};

constexpr std::array<std::pair<UserSettings::TimelineMessageLayout, const char *>, 2>
  kTimelineLayoutTokens{{
    {UserSettings::TimelineMessageLayout::Minimal, "minimal"},
    {UserSettings::TimelineMessageLayout::Bubbles, "bubbles"},
  }};

constexpr std::array<std::pair<UserSettings::NotificationMessageContentPolicy, const char *>, 3>
  kNotificationMessageContentPolicyTokens{{
    {UserSettings::NotificationMessageContentPolicy::Never, "never"},
    {UserSettings::NotificationMessageContentPolicy::UnencryptedOnly, "unencrypted_only"},
    {UserSettings::NotificationMessageContentPolicy::WheneverAvailable, "whenever_available"},
  }};

constexpr std::array<std::pair<int, const char *>, 3> kDbusAccessTokens{{
  {IntegrationsDbusAccessNone, "none"},
  {IntegrationsDbusAccessReadOnly, "read_only"},
  {IntegrationsDbusAccessReadWrite, "read_write"},
}};

} // namespace

QString
toStorageValue(UserSettings::Presence value)
{
    return valueToStorageToken(value, kPresenceTokens, "automatic_presence");
}

UserSettings::Presence
presenceFromStorage(const QString &value, UserSettings::Presence fallback)
{
    return valueFromStorageToken(value, fallback, kPresenceTokens);
}

QString
toStorageValue(UserSettings::ShowImage value)
{
    return valueToStorageToken(value, kShowImageTokens, "always");
}

UserSettings::ShowImage
showImageFromStorage(const QString &value, UserSettings::ShowImage fallback)
{
    return valueFromStorageToken(value, fallback, kShowImageTokens);
}

QString
toStorageValue(UserSettings::ShowSenderUsername value)
{
    return valueToStorageToken(value, kShowSenderUsernameTokens, "only_in_large_rooms");
}

UserSettings::ShowSenderUsername
showSenderUsernameFromStorage(const QString &value, UserSettings::ShowSenderUsername fallback)
{
    return valueFromStorageToken(value, fallback, kShowSenderUsernameTokens);
}

QString
toStorageValue(UserSettings::AutoReplaceEmoji value)
{
    return valueToStorageToken(value, kAutoReplaceEmojiTokens, "always");
}

UserSettings::AutoReplaceEmoji
autoReplaceEmojiFromStorage(const QString &value, UserSettings::AutoReplaceEmoji fallback)
{
    return valueFromStorageToken(value, fallback, kAutoReplaceEmojiTokens);
}

QString
toStorageValue(UserSettings::SendMessageKey value)
{
    return valueToStorageToken(value, kSendMessageKeyTokens, "enter");
}

UserSettings::SendMessageKey
sendMessageKeyFromStorage(const QString &value, UserSettings::SendMessageKey fallback)
{
    return valueFromStorageToken(value, fallback, kSendMessageKeyTokens);
}

QString
toStorageValue(UserSettings::RoomSortOrder value)
{
    return valueToStorageToken(value, kRoomSortOrderTokens, "unread_first_recent");
}

UserSettings::RoomSortOrder
roomSortOrderFromStorage(const QString &value, UserSettings::RoomSortOrder fallback)
{
    return valueFromStorageToken(value, fallback, kRoomSortOrderTokens);
}

QString
toStorageValue(UserSettings::LastMessagePreview value)
{
    return valueToStorageToken(value, kLastMessagePreviewTokens, "always");
}

UserSettings::LastMessagePreview
lastMessagePreviewFromStorage(const QString &value, UserSettings::LastMessagePreview fallback)
{
    return valueFromStorageToken(value, fallback, kLastMessagePreviewTokens);
}

QString
toStorageValue(UserSettings::TimelineMessageActionsPolicy value)
{
    return valueToStorageToken(value, kTimelineActionsPolicyTokens, "on_button_click");
}

UserSettings::TimelineMessageActionsPolicy
timelineMessageActionsActivationPolicyFromStorage(
  const QString &value,
  UserSettings::TimelineMessageActionsPolicy fallback)
{
    return valueFromStorageToken(value, fallback, kTimelineActionsPolicyTokens);
}

QString
toStorageValue(UserSettings::TimelineMessageLayout value)
{
    return valueToStorageToken(value, kTimelineLayoutTokens, "bubbles");
}

UserSettings::TimelineMessageLayout
timelineMessagesLayoutStyleFromStorage(const QString &value,
                                       UserSettings::TimelineMessageLayout fallback)
{
    return valueFromStorageToken(value, fallback, kTimelineLayoutTokens);
}

QString
toStorageValue(UserSettings::NotificationMessageContentPolicy value)
{
    return valueToStorageToken(
      value, kNotificationMessageContentPolicyTokens, "whenever_available");
}

UserSettings::NotificationMessageContentPolicy
notificationsMessageContentPolicyFromStorage(
  const QString &value,
  UserSettings::NotificationMessageContentPolicy fallback)
{
    return valueFromStorageToken(value, fallback, kNotificationMessageContentPolicyTokens);
}

QString
dbusAccessToStorage(int value)
{
    return valueToStorageToken(value, kDbusAccessTokens, "none");
}

int
dbusAccessFromStorage(const QString &value, int fallback)
{
    return valueFromStorageToken(value, fallback, kDbusAccessTokens);
}

} // namespace settings::serializer::config
