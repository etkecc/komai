// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/RoomsModel.h"

#include <algorithm>

#include <QUrl>

#include "models/CompletionModelRoles.h"
#include "settings/ui/facade/UserSettingsPage.h"

RoomsModel::RoomsModel(const QHash<QString, komai::MatrixRoomSummary> &matrixRooms, QObject *parent)
  : QAbstractListModel(parent)
{
    rooms_.reserve(matrixRooms.size());
    for (auto it = matrixRooms.cbegin(); it != matrixRooms.cend(); ++it) {
        const auto &room = it.value();
        rooms_.push_back(RoomEntry{
          .roomId      = room.roomId,
          .displayName = room.displayName,
          .avatarUrl   = room.avatarUrl,
          .roomAlias   = room.roomAlias,
          .isSpace     = room.isSpace,
          .timestamp   = room.timestamp,
        });
    }
    std::ranges::sort(rooms_, [](const RoomEntry &a, const RoomEntry &b) {
        if (a.timestamp != b.timestamp)
            return a.timestamp > b.timestamp;
        return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
    });
}

QHash<int, QByteArray>
RoomsModel::roleNames() const
{
    return {
      {CompletionModel::CompletionRole, "completionRole"},
      {CompletionModel::SearchRole, "searchRole"},
      {CompletionModel::SearchRole2, "searchRole2"},
      {Roles::RoomAlias, "roomAlias"},
      {Roles::AvatarUrl, "avatarUrl"},
      {Roles::RoomID, "roomid"},
      {Roles::RawRoomID, "rawroomid"},
      {Roles::RoomName, "roomName"},
      {Roles::IsTombstoned, "isTombstoned"},
      {Roles::IsSpace, "isSpace"},
    };
}

QVariant
RoomsModel::data(const QModelIndex &index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return {};

    const auto &room = rooms_.at(index.row());

    switch (role) {
    case CompletionModel::CompletionRole: {
        // Use canonical alias if available, otherwise fall back to room ID.
        const auto &identifier = room.roomAlias.isEmpty() ? room.roomId : room.roomAlias;
        if (UserSettings::instance()->composerInputMarkdownToHtmlEnabled()) {
            QString percentEncoding = QUrl::toPercentEncoding(identifier);
            return QStringLiteral("[%1](https://matrix.to/#/%2)")
              .arg(identifier.toHtmlEscaped(), percentEncoding);
        } else {
            return identifier;
        }
    }
    case CompletionModel::SearchRole:
    case Qt::DisplayRole:
    case Roles::RoomName:
        return room.displayName;
    case CompletionModel::SearchRole2:
    case Roles::RoomAlias:
        return room.roomAlias.isEmpty() ? room.roomId : room.roomAlias;
    case CompletionModel::SearchRole3:
        return room.roomId;
    case Roles::AvatarUrl:
        return room.avatarUrl;
    case Roles::RoomID:
        return room.roomId.toHtmlEscaped();
    case Roles::RawRoomID:
        return room.roomId;
    case Roles::IsTombstoned:
        return false;
    case Roles::IsSpace:
        return room.isSpace;
    }
    return {};
}
