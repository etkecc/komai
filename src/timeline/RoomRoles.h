// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>

namespace komai::timeline {
Q_NAMESPACE
QML_NAMED_ELEMENT(Room)

enum Roles
{
    Type = 0,
    TypeString,
    IsOnlyEmoji,
    Body,
    FormattedBody,
    HasFormattedBody,
    FormattedStateEvent,
    StateEventIconSource,
    IsSender,
    UserId,
    UserName,
    UserPowerlevel,
    Day,
    Timestamp,
    Url,
    ThumbnailUrl,
    Duration,
    Blurhash,
    Filename,
    Filesize,
    FilesizeBytes,
    MimeType,
    OriginalHeight,
    OriginalWidth,
    ProportionalHeight,
    EventId,
    State,
    IsEdited,
    IsEditable,
    IsEncrypted,
    IsStateEvent,
    Trustlevel,
    Notificationlevel,
    EncryptionError,
    ReplyTo,
    ThreadId,
    Reactions,
    Room,
    RoomId,
    RoomName,
    RoomTopic,
    CallType,
    Dump,
    RelatedEventCacheBuster,
    IsHiddenEvent,
    FileTypeIconSource,
};
Q_ENUM_NS(Roles)
} // namespace komai::timeline
