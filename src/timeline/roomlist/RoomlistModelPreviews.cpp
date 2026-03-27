// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include "TimelineModel.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "utils/Utils.h"

void
RoomlistModel::fetchPreviews(QString roomid_, const std::string &from)
{
    Q_UNUSED(from);

    auto roomid   = roomid_.toStdString();
    auto children = cache::getChildRoomIds(roomid);
    bool fetched  = false;

    for (const auto &childRoomId : children) {
        const auto id = QString::fromStdString(childRoomId);
        if (invites.contains(id) || models.contains(id) ||
            (previewedRooms.contains(id) && previewedRooms.value(id).has_value()))
            continue;

        if (cachedJoinedRooms_.contains(id)) {
            emit fetchedPreview(id, cachedJoinedRooms_.value(id));
            fetched = true;
            continue;
        }

        if (matrixJoinedRooms_.contains(id)) {
            const auto room = matrixJoinedRooms_.value(id);
            RoomInfo info{};
            info.name       = room.displayName.toStdString();
            info.topic      = room.topic.toStdString();
            info.avatar_url = komai::matrix::normalizeMxcUri(room.avatarUrl).toStdString();
            info.is_space   = room.isSpace;
            emit fetchedPreview(id, info);
            fetched = true;
            continue;
        }

        const auto cachedInfo = cache::singleRoomInfo(childRoomId);
        if (!cachedInfo.name.empty() || !cachedInfo.topic.empty() ||
            !cachedInfo.avatar_url.empty() || cachedInfo.member_count != 0 || cachedInfo.is_space) {
            emit fetchedPreview(id, cachedInfo);
            fetched = true;
        }
    }

    if (!fetched)
        nhlog::net()->debug("Skipping legacy hierarchy preview fetch for '{}'; no local preview "
                            "data is available",
                            roomid);
}

void
RoomlistModel::joinPreview(const QString &roomid)
{
    if (previewedRooms.contains(roomid)) {
        ChatPage::instance()->joinRoomVia(
          roomid.toStdString(), utils::roomVias(roomid.toStdString()), false);
    }
}

void
RoomlistModel::acceptInvite(QString roomid)
{
    if (invites.contains(roomid)) {
        // Don't remove invite yet, so that we can switch to it
        auto members = cache::getMembersFromInvite(roomid.toStdString(), 0, -1);
        auto local   = utils::localUser();
        for (const auto &m : members) {
            if (m.user_id == local && m.is_direct) {
                nhlog::db()->info("marking {} as direct", roomid.toStdString());
                utils::markRoomAsDirect(roomid, members);
                break;
            }
        }

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
    }
}

void
RoomlistModel::leave(QString roomid, QString reason)
{
    // We want to leave in any case, even if this is an invite or similar.
    ChatPage::instance()->leaveRoom(roomid, reason);
    if ((currentRoom_ && currentRoom_->roomId() == roomid) ||
        (currentRoomPreview_ && currentRoomPreview_->roomid() == roomid))
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
        preview.roomTopic_             = room.topic;
        preview.roomAvatarUrl_         = komai::matrix::normalizeMxcUri(room.avatarUrl);
        preview.directChatOtherUserId_ = room.directChatOtherUserId;
        preview.memberCount_           = static_cast<int>(room.memberCount);
        preview.isDirect_              = room.isDirect;
        preview.isEncrypted_           = room.isEncrypted;
        preview.isPublic_              = room.isPublic;
        preview.isFetched_             = true;
        preview.isInvite_              = false;
        preview.canJoin_               = false;
        preview.isMatrixSummary_       = true;
        return preview;
    }

    if (invites.contains(roomid) || previewedRooms.contains(roomid)) {
        std::optional<RoomInfo> i;
        if (invites.contains(roomid)) {
            i                 = invites.value(roomid);
            preview.isInvite_ = true;
            preview.canJoin_  = false;

            auto member =
              cache::getInviteMember(roomid.toStdString(), utils::localUser().toStdString());

            if (member) {
                preview.reason_ = QString::fromStdString(member->reason);
            }
        } else {
            i                 = previewedRooms.value(roomid);
            preview.isInvite_ = false;
            preview.canJoin_  = true;
        }

        preview.isFetched_ = i.has_value();

        if (i) {
            preview.roomid_    = roomid;
            preview.roomName_  = QString::fromStdString(i->name);
            preview.roomTopic_ = QString::fromStdString(i->topic);
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
    if (isInvite_) {
        auto self = cache::getInviteMember(roomid_.toStdString(), utils::localUser().toStdString());
        if (self && !self->inviter.empty()) {
            auto other = cache::getInviteMember(roomid_.toStdString(), self->inviter);
            if (other && other->avatar_url.starts_with("mxc://")) {
                return QString::fromStdString(other->avatar_url);
            }
        }
    }

    return QString();
}

QString
RoomPreview::inviterDisplayName() const
{
    if (isInvite_) {
        auto self = cache::getInviteMember(roomid_.toStdString(), utils::localUser().toStdString());
        if (self && !self->inviter.empty()) {
            auto other = cache::getInviteMember(roomid_.toStdString(), self->inviter);
            if (other) {
                return QString::fromStdString(other->name).toHtmlEscaped();
            }
        }
    }

    return QString();
}

QString
RoomPreview::inviterUserId() const
{
    if (isInvite_) {
        auto self = cache::getInviteMember(roomid_.toStdString(), utils::localUser().toStdString());
        if (self && !self->inviter.empty()) {
            return QString::fromStdString(self->inviter);
        }
    }

    return QString();
}
