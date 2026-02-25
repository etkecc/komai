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
