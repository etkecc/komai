// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>

#include "settings/core/SettingDefinition.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::serializer::config {

struct EnumTokenAdapter
{
    settings::core::SettingId id;
    const char *key;
    const char *defaultToken;
    QString (*toStorage)(const UserSettings &settings);
    void (*applyFromStorage)(UserSettings &settings, const QString &rawToken);
};

std::span<const EnumTokenAdapter>
enumTokenAdapters();

QString
toStorageValue(UserSettings::Presence value);
UserSettings::Presence
presenceFromStorage(const QString &value, UserSettings::Presence fallback);

QString
toStorageValue(UserSettings::ShowImage value);
UserSettings::ShowImage
showImageFromStorage(const QString &value, UserSettings::ShowImage fallback);

QString
toStorageValue(UserSettings::ShowSenderUsername value);
UserSettings::ShowSenderUsername
showSenderUsernameFromStorage(const QString &value, UserSettings::ShowSenderUsername fallback);

QString
toStorageValue(UserSettings::AutoReplaceEmoji value);
UserSettings::AutoReplaceEmoji
autoReplaceEmojiFromStorage(const QString &value, UserSettings::AutoReplaceEmoji fallback);

QString
toStorageValue(UserSettings::SendMessageKey value);
UserSettings::SendMessageKey
sendMessageKeyFromStorage(const QString &value, UserSettings::SendMessageKey fallback);

QString
toStorageValue(UserSettings::RoomSortOrder value);
UserSettings::RoomSortOrder
roomSortOrderFromStorage(const QString &value, UserSettings::RoomSortOrder fallback);

QString
toStorageValue(UserSettings::LastMessagePreview value);
UserSettings::LastMessagePreview
lastMessagePreviewFromStorage(const QString &value, UserSettings::LastMessagePreview fallback);

QString
toStorageValue(UserSettings::TimelineMessageActionsActivationPolicy value);
UserSettings::TimelineMessageActionsActivationPolicy
timelineMessageActionsActivationPolicyFromStorage(
  const QString &value,
  UserSettings::TimelineMessageActionsActivationPolicy fallback);

QString
toStorageValue(UserSettings::TimelineMessagesStyle value);
UserSettings::TimelineMessagesStyle
timelineMessagesStyleFromStorage(const QString &value,
                                 UserSettings::TimelineMessagesStyle fallback);

QString
toStorageValue(UserSettings::TimelineMessagesPositioning value);
UserSettings::TimelineMessagesPositioning
timelineMessagesPositioningFromStorage(const QString &value,
                                       UserSettings::TimelineMessagesPositioning fallback);

QString
toStorageValue(UserSettings::NotificationMessageContentPolicy value);
UserSettings::NotificationMessageContentPolicy
notificationsMessageContentPolicyFromStorage(
  const QString &value,
  UserSettings::NotificationMessageContentPolicy fallback);

QString
dbusAccessToStorage(int value);
int
dbusAccessFromStorage(const QString &value, int fallback);

} // namespace settings::serializer::config
