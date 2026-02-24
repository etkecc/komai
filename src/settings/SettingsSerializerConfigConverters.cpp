// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerConfigConverters.h"

#include <array>

#include "settings/SettingKeys.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::serializer::config {

QString
toStorageValue(UserSettings::Presence value)
{
    switch (value) {
    case UserSettings::Presence::AutomaticPresence:
        return QStringLiteral("automatic_presence");
    case UserSettings::Presence::Online:
        return QStringLiteral("online");
    case UserSettings::Presence::Unavailable:
        return QStringLiteral("unavailable");
    case UserSettings::Presence::Offline:
        return QStringLiteral("offline");
    }
    return QStringLiteral("automatic_presence");
}

UserSettings::Presence
presenceFromStorage(const QString &value, UserSettings::Presence fallback)
{
    if (value == QLatin1String("automatic_presence"))
        return UserSettings::Presence::AutomaticPresence;
    if (value == QLatin1String("online"))
        return UserSettings::Presence::Online;
    if (value == QLatin1String("unavailable"))
        return UserSettings::Presence::Unavailable;
    if (value == QLatin1String("offline"))
        return UserSettings::Presence::Offline;
    return fallback;
}

QString
toStorageValue(UserSettings::ShowImage value)
{
    switch (value) {
    case UserSettings::ShowImage::Always:
        return QStringLiteral("always");
    case UserSettings::ShowImage::OnlyPrivate:
        return QStringLiteral("only_private");
    case UserSettings::ShowImage::Never:
        return QStringLiteral("never");
    }
    return QStringLiteral("always");
}

UserSettings::ShowImage
showImageFromStorage(const QString &value, UserSettings::ShowImage fallback)
{
    if (value == QLatin1String("always"))
        return UserSettings::ShowImage::Always;
    if (value == QLatin1String("only_private"))
        return UserSettings::ShowImage::OnlyPrivate;
    if (value == QLatin1String("never"))
        return UserSettings::ShowImage::Never;
    return fallback;
}

QString
toStorageValue(UserSettings::ShowSenderUsername value)
{
    switch (value) {
    case UserSettings::ShowSenderUsername::Always:
        return QStringLiteral("always");
    case UserSettings::ShowSenderUsername::OnlyInLargeRooms:
        return QStringLiteral("only_in_large_rooms");
    case UserSettings::ShowSenderUsername::Never:
        return QStringLiteral("never");
    }
    return QStringLiteral("only_in_large_rooms");
}

UserSettings::ShowSenderUsername
showSenderUsernameFromStorage(const QString &value, UserSettings::ShowSenderUsername fallback)
{
    if (value == QLatin1String("always"))
        return UserSettings::ShowSenderUsername::Always;
    if (value == QLatin1String("only_in_large_rooms"))
        return UserSettings::ShowSenderUsername::OnlyInLargeRooms;
    if (value == QLatin1String("never"))
        return UserSettings::ShowSenderUsername::Never;
    return fallback;
}

QString
toStorageValue(UserSettings::AutoReplaceEmoji value)
{
    switch (value) {
    case UserSettings::AutoReplaceEmoji::Always:
        return QStringLiteral("always");
    case UserSettings::AutoReplaceEmoji::OnlyAtEnd:
        return QStringLiteral("only_at_end");
    case UserSettings::AutoReplaceEmoji::Never:
        return QStringLiteral("never");
    }
    return QStringLiteral("always");
}

UserSettings::AutoReplaceEmoji
autoReplaceEmojiFromStorage(const QString &value, UserSettings::AutoReplaceEmoji fallback)
{
    if (value == QLatin1String("always"))
        return UserSettings::AutoReplaceEmoji::Always;
    if (value == QLatin1String("only_at_end"))
        return UserSettings::AutoReplaceEmoji::OnlyAtEnd;
    if (value == QLatin1String("never"))
        return UserSettings::AutoReplaceEmoji::Never;
    return fallback;
}

QString
toStorageValue(UserSettings::SendMessageKey value)
{
    switch (value) {
    case UserSettings::SendMessageKey::Enter:
        return QStringLiteral("enter");
    case UserSettings::SendMessageKey::ShiftEnter:
        return QStringLiteral("shift_enter");
    case UserSettings::SendMessageKey::CtrlEnter:
        return QStringLiteral("ctrl_enter");
    }
    return QStringLiteral("enter");
}

UserSettings::SendMessageKey
sendMessageKeyFromStorage(const QString &value, UserSettings::SendMessageKey fallback)
{
    if (value == QLatin1String("enter"))
        return UserSettings::SendMessageKey::Enter;
    if (value == QLatin1String("shift_enter"))
        return UserSettings::SendMessageKey::ShiftEnter;
    if (value == QLatin1String("ctrl_enter"))
        return UserSettings::SendMessageKey::CtrlEnter;
    return fallback;
}

QString
toStorageValue(UserSettings::RoomSortOrder value)
{
    switch (value) {
    case UserSettings::RoomSortOrder::UnreadFirst_Recent:
        return QStringLiteral("unread_first_recent");
    case UserSettings::RoomSortOrder::UnreadFirst_Alpha:
        return QStringLiteral("unread_first_alpha");
    case UserSettings::RoomSortOrder::Recent:
        return QStringLiteral("recent");
    case UserSettings::RoomSortOrder::Alphabetical:
        return QStringLiteral("alphabetical");
    }
    return QStringLiteral("unread_first_recent");
}

UserSettings::RoomSortOrder
roomSortOrderFromStorage(const QString &value, UserSettings::RoomSortOrder fallback)
{
    if (value == QLatin1String("unread_first_recent"))
        return UserSettings::RoomSortOrder::UnreadFirst_Recent;
    if (value == QLatin1String("unread_first_alpha"))
        return UserSettings::RoomSortOrder::UnreadFirst_Alpha;
    if (value == QLatin1String("recent"))
        return UserSettings::RoomSortOrder::Recent;
    if (value == QLatin1String("alphabetical"))
        return UserSettings::RoomSortOrder::Alphabetical;
    return fallback;
}

QString
toStorageValue(UserSettings::LastMessagePreview value)
{
    switch (value) {
    case UserSettings::LastMessagePreview::Always:
        return QStringLiteral("always");
    case UserSettings::LastMessagePreview::OnlyUnencrypted:
        return QStringLiteral("only_unencrypted");
    case UserSettings::LastMessagePreview::Never:
        return QStringLiteral("never");
    }
    return QStringLiteral("always");
}

UserSettings::LastMessagePreview
lastMessagePreviewFromStorage(const QString &value, UserSettings::LastMessagePreview fallback)
{
    if (value == QLatin1String("always"))
        return UserSettings::LastMessagePreview::Always;
    if (value == QLatin1String("only_unencrypted"))
        return UserSettings::LastMessagePreview::OnlyUnencrypted;
    if (value == QLatin1String("never"))
        return UserSettings::LastMessagePreview::Never;
    return fallback;
}

QString
toStorageValue(UserSettings::TimelineMessageActionsPolicy value)
{
    switch (value) {
    case UserSettings::TimelineMessageActionsPolicy::OnHover:
        return QStringLiteral("on_message_hover");
    case UserSettings::TimelineMessageActionsPolicy::ActionsButton:
        return QStringLiteral("on_button_click");
    case UserSettings::TimelineMessageActionsPolicy::Never:
        return QStringLiteral("never");
    }
    return QStringLiteral("on_button_click");
}

UserSettings::TimelineMessageActionsPolicy
timelineMessageActionsPolicyFromStorage(const QString &value,
                                        UserSettings::TimelineMessageActionsPolicy fallback)
{
    if (value == QLatin1String("on_message_hover"))
        return UserSettings::TimelineMessageActionsPolicy::OnHover;
    if (value == QLatin1String("on_button_click"))
        return UserSettings::TimelineMessageActionsPolicy::ActionsButton;
    if (value == QLatin1String("never"))
        return UserSettings::TimelineMessageActionsPolicy::Never;
    return fallback;
}

QString
toStorageValue(UserSettings::TimelineMessageLayout value)
{
    switch (value) {
    case UserSettings::TimelineMessageLayout::Minimal:
        return QStringLiteral("minimal");
    case UserSettings::TimelineMessageLayout::Bubbles:
        return QStringLiteral("bubbles");
    }
    return QStringLiteral("bubbles");
}

UserSettings::TimelineMessageLayout
timelineMessageLayoutFromStorage(const QString &value, UserSettings::TimelineMessageLayout fallback)
{
    if (value == QLatin1String("minimal"))
        return UserSettings::TimelineMessageLayout::Minimal;
    if (value == QLatin1String("bubbles"))
        return UserSettings::TimelineMessageLayout::Bubbles;
    return fallback;
}

QString
toStorageValue(UserSettings::NotificationMessageContentPolicy value)
{
    switch (value) {
    case UserSettings::NotificationMessageContentPolicy::Never:
        return QStringLiteral("never");
    case UserSettings::NotificationMessageContentPolicy::UnencryptedOnly:
        return QStringLiteral("unencrypted_only");
    case UserSettings::NotificationMessageContentPolicy::WheneverAvailable:
        return QStringLiteral("whenever_available");
    }
    return QStringLiteral("whenever_available");
}

UserSettings::NotificationMessageContentPolicy
notificationMessageContentPolicyFromStorage(const QString &value,
                                            UserSettings::NotificationMessageContentPolicy fallback)
{
    if (value == QLatin1String("never"))
        return UserSettings::NotificationMessageContentPolicy::Never;
    if (value == QLatin1String("unencrypted_only"))
        return UserSettings::NotificationMessageContentPolicy::UnencryptedOnly;
    if (value == QLatin1String("whenever_available"))
        return UserSettings::NotificationMessageContentPolicy::WheneverAvailable;
    return fallback;
}

QString
dbusAccessToStorage(int value)
{
    switch (value) {
    case IntegrationsDbusAccessNone:
        return QStringLiteral("none");
    case IntegrationsDbusAccessReadOnly:
        return QStringLiteral("read_only");
    case IntegrationsDbusAccessReadWrite:
        return QStringLiteral("read_write");
    default:
        break;
    }
    return QStringLiteral("none");
}

int
dbusAccessFromStorage(const QString &value, int fallback)
{
    if (value == QLatin1String("none"))
        return IntegrationsDbusAccessNone;
    if (value == QLatin1String("read_only"))
        return IntegrationsDbusAccessReadOnly;
    if (value == QLatin1String("read_write"))
        return IntegrationsDbusAccessReadWrite;
    return fallback;
}

namespace {

QString
presenceToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.presence());
}

void
applyPresenceFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setPresence(presenceFromStorage(rawToken, UserSettings::Presence::AutomaticPresence));
}

QString
showImageToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.showImage());
}

void
applyShowImageFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setShowImage(showImageFromStorage(rawToken, UserSettings::ShowImage::Always));
}

QString
showSenderUsernameToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.showSenderUsername());
}

void
applyShowSenderUsernameFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setShowSenderUsername(
      showSenderUsernameFromStorage(rawToken, UserSettings::ShowSenderUsername::OnlyInLargeRooms));
}

QString
autoReplaceEmojiToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.autoReplaceEmoji());
}

void
applyAutoReplaceEmojiFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setAutoReplaceEmoji(
      autoReplaceEmojiFromStorage(rawToken, UserSettings::AutoReplaceEmoji::Always));
}

QString
sendMessageKeyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.sendMessageKey());
}

void
applySendMessageKeyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setSendMessageKey(
      sendMessageKeyFromStorage(rawToken, UserSettings::SendMessageKey::Enter));
}

QString
roomSortOrderToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.roomSortOrder());
}

void
applyRoomSortOrderFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setRoomSortOrder(
      roomSortOrderFromStorage(rawToken, UserSettings::RoomSortOrder::UnreadFirst_Recent));
}

QString
lastMessagePreviewToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.showLastMessagePreview());
}

void
applyLastMessagePreviewFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setShowLastMessagePreview(
      lastMessagePreviewFromStorage(rawToken, UserSettings::LastMessagePreview::Always));
}

QString
timelineActionsPolicyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineMessageActionsPolicy());
}

void
applyTimelineActionsPolicyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineMessageActionsPolicy(timelineMessageActionsPolicyFromStorage(
      rawToken, UserSettings::TimelineMessageActionsPolicy::ActionsButton));
}

QString
timelineLayoutStyleToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineMessageLayout());
}

void
applyTimelineLayoutStyleFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineMessageLayout(
      timelineMessageLayoutFromStorage(rawToken, UserSettings::TimelineMessageLayout::Bubbles));
}

QString
notificationMessageContentPolicyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.notificationMessageContentPolicy());
}

void
applyNotificationMessageContentPolicyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setNotificationMessageContentPolicy(notificationMessageContentPolicyFromStorage(
      rawToken, UserSettings::NotificationMessageContentPolicy::WheneverAvailable));
}

QString
dbusAccessToStorageFromSettings(const UserSettings &settings)
{
    return dbusAccessToStorage(settings.integrationsDbusApiAccess());
}

void
applyDbusAccessFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setIntegrationsDbusApiAccess(
      dbusAccessFromStorage(rawToken, IntegrationsDbusAccessNone));
}

constexpr std::array<EnumTokenAdapter, 11> kEnumTokenAdapters{{
  {settings::core::SettingId::SidebarsRoomListLastMessagePreview,
   SettingKey::SidebarsRoomListLastMessagePreview,
   "always",
   &lastMessagePreviewToStorage,
   &applyLastMessagePreviewFromStorage},
  {settings::core::SettingId::SidebarsRoomListSort,
   SettingKey::SidebarsRoomListSort,
   "unread_first_recent",
   &roomSortOrderToStorage,
   &applyRoomSortOrderFromStorage},
  {settings::core::SettingId::NetworkPresenceStatusPolicy,
   SettingKey::NetworkPresenceStatusPolicy,
   "automatic_presence",
   &presenceToStorage,
   &applyPresenceFromStorage},
  {settings::core::SettingId::IntegrationsDbusApiAccess,
   SettingKey::IntegrationsDbusApiAccess,
   "none",
   &dbusAccessToStorageFromSettings,
   &applyDbusAccessFromStorage},
  {settings::core::SettingId::ComposerInputSendKey,
   SettingKey::ComposerInputSendKey,
   "enter",
   &sendMessageKeyToStorage,
   &applySendMessageKeyFromStorage},
  {settings::core::SettingId::ComposerInputAutoReplaceEmoji,
   SettingKey::ComposerInputAutoReplaceEmoji,
   "always",
   &autoReplaceEmojiToStorage,
   &applyAutoReplaceEmojiFromStorage},
  {settings::core::SettingId::NotificationsMessageContentPolicy,
   SettingKey::NotificationsMessageContentPolicy,
   "whenever_available",
   &notificationMessageContentPolicyToStorage,
   &applyNotificationMessageContentPolicyFromStorage},
  {settings::core::SettingId::TimelineMessagesLayoutStyle,
   SettingKey::TimelineMessagesLayoutStyle,
   "bubbles",
   &timelineLayoutStyleToStorage,
   &applyTimelineLayoutStyleFromStorage},
  {settings::core::SettingId::TimelineMessagesSenderUsername,
   SettingKey::TimelineMessagesSenderUsername,
   "only_in_large_rooms",
   &showSenderUsernameToStorage,
   &applyShowSenderUsernameFromStorage},
  {settings::core::SettingId::TimelineMessageActionsActivationPolicy,
   SettingKey::TimelineMessageActionsActivationPolicy,
   "on_button_click",
   &timelineActionsPolicyToStorage,
   &applyTimelineActionsPolicyFromStorage},
  {settings::core::SettingId::TimelineMediaImageDisplay,
   SettingKey::TimelineMediaImageDisplay,
   "always",
   &showImageToStorage,
   &applyShowImageFromStorage},
}};

constexpr bool
hasUniqueEnumTokenAdapterIds()
{
    for (std::size_t i = 0; i < kEnumTokenAdapters.size(); ++i) {
        for (std::size_t j = i + 1; j < kEnumTokenAdapters.size(); ++j) {
            if (kEnumTokenAdapters[i].id == kEnumTokenAdapters[j].id)
                return false;
        }
    }

    return true;
}

constexpr bool
hasCompleteEnumTokenAdapterCoverage()
{
    for (const auto id : settings::core::definitions::enumTokenConfigSettingIds()) {
        bool found = false;
        for (const auto &adapter : kEnumTokenAdapters) {
            if (adapter.id == id) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    for (const auto &adapter : kEnumTokenAdapters) {
        if (!settings::core::definitions::isEnumTokenConfigSettingId(adapter.id))
            return false;
    }

    return true;
}

static_assert(hasUniqueEnumTokenAdapterIds(),
              "enum token adapters must not contain duplicate SettingIds");
static_assert(hasCompleteEnumTokenAdapterCoverage(),
              "enum token adapters must match core enum-token setting definitions");

} // namespace

std::span<const EnumTokenAdapter>
enumTokenAdapters()
{
    return kEnumTokenAdapters;
}

} // namespace settings::serializer::config
