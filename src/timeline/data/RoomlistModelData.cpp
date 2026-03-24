// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <algorithm>
#include <optional>

#include <QDateTime>
#include <QTimer>

#include "DirectChatResolver.h"
#include "TimelineModel.h"
#include "cache/Cache.h"
#include "events/EventAccessors.h"
#include "matrix/MatrixMediaUri.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/roomlist/RoomlistPreviewSelection.h"
#include "utils/Utils.h"

std::optional<QVariant>
RoomlistModel::commonRoomData(const QString &room_id, int role) const
{
    switch (role) {
    case Roles::ParentSpaces: {
        if (matrixJoinedRooms_.contains(room_id))
            return QVariant{QStringList{}};

        auto parents = cache::getParentRoomIds(room_id.toStdString());
        QStringList list;
        list.reserve(static_cast<int>(parents.size()));
        for (const auto &t : parents)
            list.push_back(QString::fromStdString(t));
        return QVariant{list};
    }
    case Roles::RoomId:
        return QVariant{room_id};
    case Roles::IsDirect:
        if (matrixJoinedRooms_.contains(room_id))
            return QVariant{matrixJoinedRooms_.value(room_id).isDirect};
        return QVariant{DirectChatResolver::instance().isDirectChat(room_id)};
    case Roles::DirectChatOtherUserId:
        if (matrixJoinedRooms_.contains(room_id))
            return QVariant{QString{}};
        return QVariant{DirectChatResolver::instance().directChatPartner(room_id)};
    case Roles::IsBotRoom:
        if (matrixJoinedRooms_.contains(room_id))
            return QVariant{false};
        return QVariant{DirectChatResolver::instance().isBotRoom(room_id)};
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
    case Roles::LastMessage:
        return QString{};
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
        return QStringList{};
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
    struct ResolvedPreviewFields
    {
        QString lastMessage;
        QString descriptiveTime;
        quint64 timestamp = 0;
    };

    bool previewResolved = false;
    ResolvedPreviewFields previewFields;
    auto resolvePreview =
      [this, &room_id, &room, &previewResolved, &previewFields]() -> const ResolvedPreviewFields & {
        if (previewResolved)
            return previewFields;

        previewResolved = true;

        const auto liveDescription = room->lastMessage();
        bool hasLiveMessagePreview = false;
        if (!liveDescription.body.isEmpty() && !liveDescription.event_id.isEmpty()) {
            if (const auto event =
                  cache::getEvent(room_id.toStdString(), liveDescription.event_id.toStdString());
                event.has_value() && mtx::accessors::is_message(*event)) {
                hasLiveMessagePreview = true;
            }
        }

        std::optional<DescInfo> cachedDescription;
        if (!hasLiveMessagePreview) {
            auto *self = const_cast<RoomlistModel *>(this);
            self->ensureCachedLastMessage(room_id);
            cachedDescription = self->cachedLastMessages_.value(room_id);
        }

        const auto roomInfo = cachedJoinedRooms_.value(room_id);
        const auto selected = timeline::roomlist::selectMaterializedPreviewFields(
          liveDescription,
          static_cast<quint64>(room->lastMessageTimestamp()),
          hasLiveMessagePreview,
          cachedDescription,
          static_cast<quint64>(roomInfo.approximate_last_modification_ts));
        previewFields.lastMessage     = selected.lastMessage;
        previewFields.descriptiveTime = selected.descriptiveTime;
        previewFields.timestamp       = selected.timestamp;

        if (previewFields.descriptiveTime.isEmpty() &&
            roomInfo.approximate_last_modification_ts > 0) {
            previewFields.descriptiveTime = utils::descriptiveTime(QDateTime::fromMSecsSinceEpoch(
              static_cast<qint64>(roomInfo.approximate_last_modification_ts)));
        }

        return previewFields;
    };

    switch (role) {
    case Roles::AvatarUrl: {
        const auto roomModelAvatar = room->roomAvatarUrl();
        if (!roomModelAvatar.isEmpty())
            return komai::matrix::normalizeMxcUri(roomModelAvatar);

        const auto avatarUrl = cache::roomAvatarUrl(room_id.toStdString());
        if (!avatarUrl.isEmpty())
            return komai::matrix::normalizeMxcUri(avatarUrl);

        return roomModelAvatar;
    }
    case Roles::RoomName:
        return room->plainRoomName();
    case Roles::LastMessage:
        return resolvePreview().lastMessage;
    case Roles::Time:
        return resolvePreview().descriptiveTime;
    case Roles::Timestamp:
        return QVariant{resolvePreview().timestamp};
    case Roles::HasUnreadMessages:
        return this->roomReadStatus.count(room_id) && this->roomReadStatus.at(room_id);
    case Roles::HasLoudNotification:
        return room->hasMentions();
    case Roles::NotificationCount: {
        const bool hasUnread =
          this->roomReadStatus.count(room_id) && this->roomReadStatus.at(room_id);
        const int notificationCount = room->notificationCount();
        return (hasUnread || room->hasMentions()) ? notificationCount : 0;
    }
    case Roles::IsInvite:
        return false;
    case Roles::IsSpace:
        return room->isSpace();
    case Roles::IsPreview:
        return false;
    case Roles::Tags: {
        auto info = cache::singleRoomInfo(room_id.toStdString());
        QStringList list;
        list.reserve(static_cast<int>(info.tags.size()));
        for (const auto &t : info.tags)
            list.push_back(QString::fromStdString(t));
        return list;
    }
    case Roles::IsEncrypted:
        return room->isEncrypted();
    default:
        return {};
    }
}

QVariant
RoomlistModel::dataForCachedRoom(const QString &room_id, const RoomInfo &room, int role) const
{
    switch (role) {
    case Roles::AvatarUrl: {
        if (!room.avatar_url.empty())
            return komai::matrix::normalizeMxcUri(QString::fromStdString(room.avatar_url));
        return komai::matrix::normalizeMxcUri(cache::roomAvatarUrl(room_id.toStdString()));
    }
    case Roles::RoomName: {
        // Use the DM-aware display name so the room list shows the partner's
        // name instead of computed fallbacks like "Someone and Bridge bot".
        auto dmName = DirectChatResolver::instance().dmRoomDisplayName(room_id);
        return dmName.isEmpty() ? QString::fromStdString(room.name) : dmName;
    }
    case Roles::LastMessage: {
        const auto style = UserSettings::instance()->sidebarsRoomListLastMessagePreview();
        const bool encrypted =
          cachedEncryptedRooms_.value(room_id, cache::isRoomEncrypted(room_id.toStdString()));
        const bool previewsEnabled =
          style == UserSettings::LastMessagePreview::Always ||
          (style == UserSettings::LastMessagePreview::OnlyUnencrypted && !encrypted);
        if (!previewsEnabled)
            return QString();

        auto *self = const_cast<RoomlistModel *>(this);
        self->ensureCachedLastMessage(room_id);
        const auto cachedDescription = cachedLastMessages_.value(room_id);
        if (cachedDescription.body.isEmpty())
            self->maybeBackfillCachedLastMessage(room_id);
        else if (style == UserSettings::LastMessagePreview::Always && encrypted &&
                 RoomlistModel::isCachedEncryptedPreview(room_id, cachedDescription)) {
            self->scheduleRoomPrewarm(room_id, QStringLiteral("auto_preview_decrypt"));
            QTimer::singleShot(0, self, [self, room_id]() {
                if (self->scheduledPrewarms_.contains(room_id))
                    self->prewarmRoom(room_id, QStringLiteral("auto_preview_decrypt"));
            });
        }
        return cachedDescription.body;
    }
    case Roles::Time: {
        auto *self = const_cast<RoomlistModel *>(this);
        self->ensureCachedLastMessage(room_id);
        const auto cachedDescription = cachedLastMessages_.value(room_id);
        if (!cachedDescription.descriptiveTime.isEmpty())
            return cachedDescription.descriptiveTime;

        if (room.approximate_last_modification_ts > 0) {
            return utils::descriptiveTime(QDateTime::fromMSecsSinceEpoch(
              static_cast<qint64>(room.approximate_last_modification_ts)));
        }
        return QString();
    }
    case Roles::Timestamp: {
        auto *self = const_cast<RoomlistModel *>(this);
        self->ensureCachedLastMessage(room_id);
        const auto cachedDescription = cachedLastMessages_.value(room_id);
        const auto ts = std::max(static_cast<quint64>(room.approximate_last_modification_ts),
                                 static_cast<quint64>(cachedDescription.timestamp));
        return QVariant{ts};
    }
    case Roles::HasUnreadMessages:
        return this->roomReadStatus.count(room_id) && this->roomReadStatus.at(room_id);
    case Roles::HasLoudNotification:
        return room.highlight_count > 0;
    case Roles::NotificationCount: {
        const bool hasUnread =
          this->roomReadStatus.count(room_id) && this->roomReadStatus.at(room_id);
        const int notificationCount = static_cast<int>(room.notification_count);
        return (hasUnread || room.highlight_count > 0) ? notificationCount : 0;
    }
    case Roles::IsInvite:
        return false;
    case Roles::IsSpace:
        return room.is_space;
    case Roles::IsPreview:
        return false;
    case Roles::Tags: {
        QStringList list;
        list.reserve(static_cast<int>(room.tags.size()));
        for (const auto &t : room.tags)
            list.push_back(QString::fromStdString(t));
        return list;
    }
    case Roles::IsEncrypted:
        return cachedEncryptedRooms_.value(room_id, cache::isRoomEncrypted(room_id.toStdString()));
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

    if (models.contains(room_id))
        return dataForMaterializedRoom(room_id, models.value(room_id), role);

    if (matrixJoinedRooms_.contains(room_id))
        return dataForMatrixRoom(room_id, matrixJoinedRooms_.value(room_id), role);

    if (cachedJoinedRooms_.contains(room_id))
        return dataForCachedRoom(room_id, cachedJoinedRooms_.value(room_id), role);

    if (invites.contains(room_id))
        return dataForInviteRoom(invites.value(room_id), role);

    if (previewedRooms.contains(room_id)) {
        if (const auto room = previewedRooms.value(room_id); room.has_value())
            return dataForPreviewRoom(room.value(), role);
    }

    return dataForUnavailablePreview(role);
}
