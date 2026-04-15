// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <algorithm>
#include <optional>

#include <QCoreApplication>
#include <QDateTime>
#include <QTimer>

#include "matrix/MatrixMediaUri.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/StateEventText.h"
#include "timeline/roomlist/RoomlistPreviewSelection.h"
#include "utils/Utils.h"

namespace {
QString
roomListPreviewSenderName(const komai::MatrixRoomSummary &room)
{
    const auto displayName = room.lastMessageSenderDisplayName.trimmed();
    if (!displayName.isEmpty())
        return displayName;

    return room.lastMessageSenderId.trimmed();
}

QString
formatMatrixRoomListPreview(const komai::MatrixRoomSummary &room)
{
    if (room.lastMessage.isEmpty() && room.lastMessageKind.isEmpty())
        return {};

    // Translate the preview body based on the kind key.  For non-content
    // kinds (event type labels, state events) this returns a translated
    // label; for content kinds it returns the original lastMessage.
    const auto body =
      StateEventText::translateRoomListPreview(room.lastMessageKind, room.lastMessage);
    if (body.isEmpty())
        return {};

    if (room.lastMessageKind == QStringLiteral("emote")) {
        const auto senderName = roomListPreviewSenderName(room);
        if (senderName.isEmpty())
            return body;

        return QStringLiteral("* %1 %2").arg(senderName, body);
    }

    if (room.lastMessageKind == QStringLiteral("membership_change") ||
        room.lastMessageKind == QStringLiteral("profile_change") ||
        room.lastMessageKind == QStringLiteral("other_state") ||
        room.lastMessageKind == QStringLiteral("failed_to_parse_state")) {
        return body;
    }

    const auto senderName = roomListPreviewSenderName(room);
    if (senderName.isEmpty())
        return body;

    const auto localUserId = utils::localUser().trimmed();
    const bool isLocal =
      !localUserId.isEmpty() && room.lastMessageSenderId.trimmed() == localUserId;

    return isLocal ? QCoreApplication::translate("message-description sent:", "You: %1").arg(body)
                   : QCoreApplication::translate("message-description sent:", "%1: %2")
                       .arg(senderName, body);
}
} // namespace

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
        if (room.isInvite) {
            if (!room.inviterDisplayName.isEmpty())
                return tr("Invited by %1").arg(room.inviterDisplayName);
            if (!room.inviterUserId.isEmpty())
                return tr("Invited by %1").arg(room.inviterUserId);
            return tr("Pending invite");
        }
        const auto style = UserSettings::instance()->navigationRoomListLastMessagePreview();
        const bool previewsEnabled =
          style == UserSettings::LastMessagePreview::Always ||
          (style == UserSettings::LastMessagePreview::OnlyUnencrypted && !room.isEncrypted);
        return previewsEnabled ? formatMatrixRoomListPreview(room) : QString{};
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
    case Roles::UnreadCount:
        return static_cast<int>(room.unreadMessages);
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
    case Roles::UnreadCount:
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
    case Roles::UnreadCount:
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
    case Roles::UnreadCount:
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
