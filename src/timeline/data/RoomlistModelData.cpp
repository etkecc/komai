// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <algorithm>
#include <optional>

#include <QDateTime>
#include <QTimer>

#include "matrix/MatrixMediaUri.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/roomlist/RoomlistPreviewSelection.h"
#include "utils/Utils.h"

std::optional<QVariant>
RoomlistModel::commonRoomData(const QString &room_id, int role) const
{
    switch (role) {
    case Roles::ParentSpaces: {
        if (matrixJoinedRooms_.contains(room_id)) {
            QStringList list;
            const auto &parentSpaceRoomIds = matrixJoinedRooms_.value(room_id).parentSpaceRoomIds;
            list.reserve(parentSpaceRoomIds.size());
            for (const auto &parentSpaceRoomId : parentSpaceRoomIds)
                list.push_back(parentSpaceRoomId);
            return QVariant{list};
        }
        return QVariant{QStringList{}};
    }
    case Roles::RoomId:
        return QVariant{room_id};
    case Roles::IsDirect:
        if (matrixJoinedRooms_.contains(room_id))
            return QVariant{matrixJoinedRooms_.value(room_id).isDirect};
        return QVariant{false};
    case Roles::DirectChatOtherUserId:
        if (matrixJoinedRooms_.contains(room_id))
            return QVariant{matrixJoinedRooms_.value(room_id).directChatOtherUserId};
        return QVariant{QString{}};
    case Roles::IsBotRoom:
        if (matrixJoinedRooms_.contains(room_id))
            return QVariant{matrixJoinedRooms_.value(room_id).isBotRoom};
        return QVariant{false};
    case Roles::HasDraft:
        return QVariant{hasDraft(room_id)};
    case Roles::DraftPreview:
        return QVariant{draftPreviewText(room_id)};
    default:
        return std::nullopt;
    }
}

QVariant
RoomlistModel::dataForMatrixRoom(const QString &room_id,
                                 const komai::MatrixRoomSummary &room,
                                 int role) const
{
    switch (role) {
    case Roles::AvatarUrl:
        return komai::matrix::normalizeMxcUri(room.avatarUrl);
    case Roles::RoomName:
        return room.displayName.isEmpty() ? room_id : room.displayName;
    case Roles::LastMessage: {
        const auto style = UserSettings::instance()->sidebarsRoomListLastMessagePreview();
        const bool previewsEnabled =
          style == UserSettings::LastMessagePreview::Always ||
          (style == UserSettings::LastMessagePreview::OnlyUnencrypted && !room.isEncrypted);
        return previewsEnabled ? room.lastMessage : QString{};
    }
    case Roles::Time:
        if (room.timestamp > 0) {
            return utils::descriptiveTime(
              QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(room.timestamp)));
        }
        return QString{};
    case Roles::Timestamp:
        return QVariant::fromValue<qulonglong>(room.timestamp);
    case Roles::HasUnreadMessages:
        return room.unreadMessages > 0;
    case Roles::HasLoudNotification:
        return room.highlightCount > 0;
    case Roles::NotificationCount:
        return static_cast<int>(room.notificationCount);
    case Roles::IsInvite:
        return room.isInvite;
    case Roles::IsSpace:
        return room.isSpace;
    case Roles::IsPreview:
        return false;
    case Roles::Tags:
        return QStringList(room.tags.begin(), room.tags.end());
    case Roles::IsEncrypted:
        return room.isEncrypted;
    default:
        return {};
    }
}

QVariant
RoomlistModel::dataForMaterializedRoom(const QString &room_id,
                                       const QSharedPointer<TimelineModel> &room,
                                       int role) const
{
    Q_UNUSED(room_id);
    Q_UNUSED(room);
    Q_UNUSED(role);
    return {};
}

QVariant
RoomlistModel::dataForCachedRoom(const QString &room_id, const RoomInfo &room, int role) const
{
    Q_UNUSED(room_id);
    Q_UNUSED(room);
    Q_UNUSED(role);
    return {};
}

QVariant
RoomlistModel::dataForInviteRoom(const RoomInfo &room, int role) const
{
    switch (role) {
    case Roles::AvatarUrl:
        return komai::matrix::normalizeMxcUri(QString::fromStdString(room.avatar_url));
    case Roles::RoomName:
        return QString::fromStdString(room.name);
    case Roles::LastMessage:
        return tr("Pending invite.");
    case Roles::Time:
        return QString();
    case Roles::Timestamp:
        return QVariant{static_cast<quint64>(0)};
    case Roles::HasUnreadMessages:
    case Roles::HasLoudNotification:
        return false;
    case Roles::NotificationCount:
        return 0;
    case Roles::IsInvite:
        return true;
    case Roles::IsSpace:
        return false;
    case Roles::IsPreview:
        return false;
    case Roles::Tags:
        return QStringList();
    case Roles::IsEncrypted:
        return false; // Invites - assume unencrypted
    default:
        return {};
    }
}

QVariant
RoomlistModel::dataForPreviewRoom(const RoomInfo &room, int role) const
{
    switch (role) {
    case Roles::AvatarUrl:
        return komai::matrix::normalizeMxcUri(QString::fromStdString(room.avatar_url));
    case Roles::RoomName:
        return QString::fromStdString(room.name);
    case Roles::LastMessage:
        return tr("Previewing this room");
    case Roles::Time:
        return QString();
    case Roles::Timestamp:
        return QVariant{static_cast<quint64>(0)};
    case Roles::HasUnreadMessages:
    case Roles::HasLoudNotification:
        return false;
    case Roles::NotificationCount:
        return 0;
    case Roles::IsInvite:
        return false;
    case Roles::IsSpace:
        return room.is_space;
    case Roles::IsPreview:
        return true;
    case Roles::IsPreviewFetched:
        return true;
    case Roles::Tags:
        return QStringList();
    case Roles::IsEncrypted:
        return false; // Previews - assume unencrypted
    default:
        return {};
    }
}

QVariant
RoomlistModel::dataForUnavailablePreview(int role) const
{
    if (role == Roles::IsPreview)
        return true;
    if (role == Roles::IsPreviewFetched)
        return false;

    switch (role) {
    case Roles::AvatarUrl:
        return QString();
    case Roles::RoomName:
        return tr("No preview available");
    case Roles::LastMessage:
        return tr("This room is possibly inaccessible");
    case Roles::Time:
        return QString();
    case Roles::Timestamp:
        return QVariant{static_cast<quint64>(0)};
    case Roles::HasUnreadMessages:
    case Roles::HasLoudNotification:
        return false;
    case Roles::NotificationCount:
        return 0;
    case Roles::IsInvite:
        return false;
    case Roles::IsSpace:
        return false;
    case Roles::Tags:
        return QStringList();
    case Roles::IsEncrypted:
        return false; // Unknown rooms - assume unencrypted
    default:
        return {};
    }
}

QVariant
RoomlistModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || static_cast<size_t>(index.row()) >= roomids.size())
        return {};

    const auto room_id = roomids.at(index.row());

    if (const auto value = commonRoomData(room_id, role); value.has_value())
        return *value;

    if (matrixJoinedRooms_.contains(room_id))
        return dataForMatrixRoom(room_id, matrixJoinedRooms_.value(room_id), role);

    if (invites.contains(room_id))
        return dataForInviteRoom(invites.value(room_id), role);

    if (previewedRooms.contains(room_id)) {
        if (const auto room = previewedRooms.value(room_id); room.has_value())
            return dataForPreviewRoom(room.value(), role);
    }

    return dataForUnavailablePreview(role);
}
