// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerConfigConverters.h"

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

} // namespace settings::serializer::config
