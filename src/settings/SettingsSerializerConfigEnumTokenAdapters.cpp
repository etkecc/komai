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
    return toStorageValue(settings.networkPresenceStatusPolicy());
}

void
applyPresenceFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setNetworkPresenceStatusPolicy(
      presenceFromStorage(rawToken, UserSettings::Presence::AutomaticPresence));
}

QString
showImageToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineMediaImageDisplay());
}

void
applyShowImageFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineMediaImageDisplay(
      showImageFromStorage(rawToken, UserSettings::ShowImage::Always));
}

QString
showSenderUsernameToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineMessagesSenderUsername());
}

void
applyShowSenderUsernameFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineMessagesSenderUsername(
      showSenderUsernameFromStorage(rawToken, UserSettings::ShowSenderUsername::OnlyInLargeRooms));
}

QString
autoReplaceEmojiToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.composerInputAutoReplaceEmoji());
}

void
applyAutoReplaceEmojiFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setComposerInputAutoReplaceEmoji(
      autoReplaceEmojiFromStorage(rawToken, UserSettings::AutoReplaceEmoji::Always));
}

QString
sendMessageKeyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.composerInputSendKey());
}

void
applySendMessageKeyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setComposerInputSendKey(
      sendMessageKeyFromStorage(rawToken, UserSettings::SendMessageKey::Enter));
}

QString
roomSortOrderToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.sidebarsRoomListSort());
}

void
applyRoomSortOrderFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setSidebarsRoomListSort(
      roomSortOrderFromStorage(rawToken, UserSettings::RoomSortOrder::UnreadFirst_Recent));
}

QString
lastMessagePreviewToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.sidebarsRoomListLastMessagePreview());
}

void
applyLastMessagePreviewFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setSidebarsRoomListLastMessagePreview(
      lastMessagePreviewFromStorage(rawToken, UserSettings::LastMessagePreview::Always));
}

QString
timelineActionsPolicyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineMessageActionsActivationPolicy());
}

void
applyTimelineActionsPolicyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineMessageActionsActivationPolicy(
      timelineMessageActionsActivationPolicyFromStorage(
        rawToken, UserSettings::TimelineMessageActionsActivationPolicy::ActionsButton));
}

QString
timelineLayoutStyleToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.timelineMessagesLayoutStyle());
}

void
applyTimelineLayoutStyleFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setTimelineMessagesLayoutStyle(timelineMessagesLayoutStyleFromStorage(
      rawToken, UserSettings::TimelineMessagesLayoutStyle::Bubbles));
}

QString
notificationsMessageContentPolicyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.notificationsMessageContentPolicy());
}

void
applyNotificationMessageContentPolicyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setNotificationsMessageContentPolicy(notificationsMessageContentPolicyFromStorage(
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
#include "SettingsSerializerConfigEnumTokenAdaptersComposer.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersIntegrations.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersNetwork.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersNotifications.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersSidebars.inc"
#include "SettingsSerializerConfigEnumTokenAdaptersTimeline.inc"
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
