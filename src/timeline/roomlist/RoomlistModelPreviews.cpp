// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "utils/Utils.h"

void
RoomlistModel::fetchPreviews(QString roomid_, const std::string &from)
{
    Q_UNUSED(from);

    const auto roomid = roomid_;
    bool fetched      = false;

    for (auto it = matrixJoinedRooms_.cbegin(); it != matrixJoinedRooms_.cend(); ++it) {
        const auto &id   = it.key();
        const auto &room = it.value();

        if (!room.parentSpaceRoomIds.contains(roomid))
            continue;

        if (invites.contains(id) ||
            (previewedRooms.contains(id) && previewedRooms.value(id).has_value()))
            continue;

        RoomInfo info{};
        info.name         = room.displayName.toStdString();
        info.topic        = room.topic.toStdString();
        info.avatar_url   = komai::matrix::normalizeMxcUri(room.avatarUrl).toStdString();
        info.is_space     = room.isSpace;
        info.member_count = static_cast<size_t>(room.memberCount);
        emit fetchedPreview(id, info);
        fetched = true;
    }

    if (!fetched)
        komai::logging::net()->debug(
          "Skipping hierarchy preview fetch for '{}'; no matrix room-summary preview data is "
          "available",
          roomid.toStdString());
}

void
RoomlistModel::joinPreview(const QString &roomid)
{
    if (previewedRooms.contains(roomid))
        ChatPage::instance()->joinRoom(roomid);
}

void
RoomlistModel::acceptInvite(QString roomid)
{
    if (invites.contains(roomid)) {
        ChatPage::instance()->joinRoom(roomid);
    } else if (matrixJoinedRooms_.contains(roomid) && matrixJoinedRooms_.value(roomid).isInvite) {
        recentlyAcceptedInviteRoomId_ = roomid;
        ChatPage::instance()->joinRoom(roomid);
    }
}

void
RoomlistModel::declineInvite(QString roomid)
{
    if (invites.contains(roomid)) {
        auto idx = roomidToIndex(roomid);

        if (idx != -1) {
            beginRemoveRows(QModelIndex(), idx, idx);
            roomids.erase(roomids.begin() + idx);
            invites.remove(roomid);
            endRemoveRows();
            ChatPage::instance()->leaveRoom(roomid, "");
        }
    } else if (matrixJoinedRooms_.contains(roomid) && matrixJoinedRooms_.value(roomid).isInvite) {
        ChatPage::instance()->leaveRoom(roomid, "");
        if (currentRoomPreview_ && currentRoomPreview_->roomid() == roomid)
            resetCurrentRoom();

        auto idx = roomidToIndex(roomid);
        if (idx != -1) {
            beginRemoveRows(QModelIndex(), idx, idx);
            roomids.erase(roomids.begin() + idx);
            endRemoveRows();
        }
        matrixJoinedRooms_.remove(roomid);
        removeRoomState(roomid);
    }
}

void
RoomlistModel::leave(QString roomid, QString reason)
{
    // We want to leave in any case, even if this is an invite or similar.
    ChatPage::instance()->leaveRoom(roomid, reason);
    if (currentRoomPreview_ && currentRoomPreview_->roomid() == roomid)
        resetCurrentRoom();

    auto idx = roomidToIndex(roomid);
    if (idx != -1) {
        beginRemoveRows(QModelIndex(), idx, idx);
        roomids.erase(roomids.begin() + idx);
        endRemoveRows();
    }

    removeRoomState(roomid);
}

RoomPreview
RoomlistModel::getRoomPreviewById(QString roomid) const
{
    RoomPreview preview{};

    if (matrixJoinedRooms_.contains(roomid)) {
        const auto room                = matrixJoinedRooms_.value(roomid);
        preview.roomid_                = roomid;
        preview.roomName_              = room.displayName.isEmpty() ? roomid : room.displayName;
        preview.roomTopic_             = utils::replaceEmoji(utils::linkifyMessage(
          room.topic.toHtmlEscaped().replace(QLatin1String("\n"), QLatin1String("<br>"))));
        preview.roomAvatarUrl_         = komai::matrix::normalizeMxcUri(room.avatarUrl);
        preview.directChatOtherUserId_ = room.directChatOtherUserId;
        preview.memberCount_           = static_cast<int>(room.memberCount);
        preview.isDirect_              = room.isDirect;
        preview.isEncrypted_           = room.isEncrypted;
        preview.isPublic_              = room.isPublic;
        preview.isSpace_               = room.isSpace;
        preview.isFetched_             = true;
        preview.isInvite_              = room.isInvite;
        preview.canJoin_               = false;
        preview.isMatrixSummary_       = true;
        if (room.isInvite) {
            preview.inviterUserId_      = room.inviterUserId;
            preview.inviterDisplayName_ = room.inviterDisplayName;
            preview.inviterAvatarUrl_   = room.inviterAvatarUrl;
            preview.reason_             = room.inviteReason;
        }
        return preview;
    }

    if (invites.contains(roomid) || previewedRooms.contains(roomid)) {
        std::optional<RoomInfo> i;
        if (invites.contains(roomid)) {
            i                 = invites.value(roomid);
            preview.isInvite_ = true;
            preview.canJoin_  = false;
        } else {
            i                 = previewedRooms.value(roomid);
            preview.isInvite_ = false;
            preview.canJoin_  = true;
        }

        preview.isFetched_ = i.has_value();

        if (i) {
            preview.roomid_    = roomid;
            preview.roomName_  = QString::fromStdString(i->name);
            preview.roomTopic_ = utils::replaceEmoji(
              utils::linkifyMessage(QString::fromStdString(i->topic).toHtmlEscaped().replace(
                QLatin1String("\n"), QLatin1String("<br>"))));
            preview.roomAvatarUrl_ =
              komai::matrix::normalizeMxcUri(QString::fromStdString(i->avatar_url));
            preview.memberCount_ = static_cast<int>(i->member_count);
        } else {
            preview.roomid_ = roomid;
        }
    }

    return preview;
}

QString
RoomPreview::inviterAvatarUrl() const
{
    return inviterAvatarUrl_;
}

QString
RoomPreview::inviterDisplayName() const
{
    return inviterDisplayName_;
}

QString
RoomPreview::inviterUserId() const
{
    return inviterUserId_;
}
