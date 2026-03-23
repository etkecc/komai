// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <algorithm>

#include "TimelineModel.h"
#include "TimelineViewManager.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

#ifdef KOMAI_DBUS_SYS
#include <QDBusConnection>
#endif

bool
RoomlistModel::isCachedEncryptedPreview(const QString &room_id, const DescInfo &description)
{
    if (room_id.isEmpty() || description.event_id.isEmpty())
        return false;

    const auto event = cache::getEvent(room_id.toStdString(), description.event_id.toStdString());
    return event.has_value() &&
           mtx::accessors::event_type(*event) == mtx::events::EventType::RoomEncrypted;
}

RoomlistModel::RoomlistModel(TimelineViewManager *parent)
  : QAbstractListModel(parent)
  , manager(parent)
{
    cache::onRoomReadStatusChanged(
      this, [this](const std::map<QString, bool> &status) { updateReadStatus(status); });

    connect(UserSettings::instance().get(),
            &UserSettings::sidebarsRoomListUnreadDetectionPolicyChanged,
            this,
            [](auto) { cache::calculateRoomReadStatus(); });

    connect(UserSettings::instance().get(),
            &UserSettings::sidebarsRoomListLastMessagePreviewChanged,
            this,
            [this]() {
                auto style   = UserSettings::instance()->sidebarsRoomListLastMessagePreview();
                bool decrypt = (style == UserSettings::LastMessagePreview::Always);

                cachedLastMessages_.clear();
                cachedLastMessagesComputed_.clear();
                QHash<QString, QSharedPointer<TimelineModel>>::iterator i;
                for (i = models.begin(); i != models.end(); ++i) {
                    auto ptr = i.value();

                    if (!ptr.isNull()) {
                        ptr->setDecryptDescription(decrypt);
                        ptr->updateLastMessage();
                    }
                }

                if (!roomids.empty()) {
                    emit dataChanged(index(0),
                                     index((int)roomids.size() - 1),
                                     {Roles::LastMessage, Roles::Time, Roles::Timestamp});
                }
            });

    connect(this,
            &RoomlistModel::totalUnreadMessageCountUpdated,
            ChatPage::instance(),
            &ChatPage::unreadMessages);

    connect(
      this,
      &RoomlistModel::fetchedPreview,
      this,
      [this](QString roomid, RoomInfo info) {
          if (this->previewedRooms.contains(roomid)) {
              this->previewedRooms.insert(roomid, std::move(info));
              auto idx = this->roomidToIndex(roomid);
              emit dataChanged(index(idx),
                               index(idx),
                               {
                                 Roles::RoomName,
                                 Roles::AvatarUrl,
                                 Roles::IsSpace,
                                 Roles::IsPreviewFetched,
                                 Qt::DisplayRole,
                               });
          }
      },
      Qt::QueuedConnection);

    initLruEviction();
}

QHash<int, QByteArray>
RoomlistModel::roleNames() const
{
    return {
      {AvatarUrl, "avatarUrl"},
      {RoomName, "roomName"},
      {RoomId, "roomId"},
      {LastMessage, "lastMessage"},
      {Time, "time"},
      {Timestamp, "timestamp"},
      {HasUnreadMessages, "hasUnreadMessages"},
      {HasLoudNotification, "hasLoudNotification"},
      {NotificationCount, "notificationCount"},
      {HasDraft, "hasDraft"},
      {DraftPreview, "draftPreview"},
      {IsInvite, "isInvite"},
      {IsSpace, "isSpace"},
      {Tags, "tags"},
      {ParentSpaces, "parentSpaces"},
      {IsDirect, "isDirect"},
      {DirectChatOtherUserId, "directChatOtherUserId"},
      {IsBotRoom, "isBotRoom"},
      {IsEncrypted, "isEncrypted"},
    };
}

QSharedPointer<TimelineModel>
RoomlistModel::getRoomById(QString id) const
{
    return getRoomByIdWithReason(std::move(id), "cpp.getRoomById");
}

QSharedPointer<TimelineModel>
RoomlistModel::getRoomByIdWithReason(QString id, const char *reason) const
{
    if (models.contains(id))
        return models.value(id);

    if (!cachedJoinedRooms_.contains(id))
        return {};

    return const_cast<RoomlistModel *>(this)->ensureRoomModel(id, true, reason);
}

QSharedPointer<TimelineModel>
RoomlistModel::getMaterializedRoomById(QString id) const
{
    if (models.contains(id))
        return models.value(id);

    return {};
}

QSharedPointer<TimelineModel>
RoomlistModel::ensureRoomModel(const QString &room_id,
                               bool suppressInsertNotification,
                               const char *reason)
{
    if (!models.contains(room_id) && cachedJoinedRooms_.contains(room_id))
        addRoom(room_id, suppressInsertNotification, reason);

    return models.value(room_id);
}

void
RoomlistModel::refreshCachedRoomMetadata(const QString &room_id)
{
    cachedJoinedRooms_.insert(room_id, cache::singleRoomInfo(room_id.toStdString()));
    cachedEncryptedRooms_.insert(room_id, cache::isRoomEncrypted(room_id.toStdString()));
    invalidateCachedLastMessage(room_id);
}

void
RoomlistModel::resetRoomCollections(bool clearAllDrafts)
{
    models.clear();
    cachedJoinedRooms_.clear();
    cachedEncryptedRooms_.clear();
    cachedLastMessages_.clear();
    cachedLastMessagesComputed_.clear();
    cachedLastMessageBackfillAttempted_.clear();
    cachedLastMessageBackfillQueued_.clear();
    cachedLastMessageBackfillInProgress_.clear();
    scheduledPrewarms_.clear();
    activePrewarms_.clear();
    prewarmLastAttemptMs_.clear();
    roomLruAccessMs_.clear();
    if (lruEvictionTimer_)
        lruEvictionTimer_->stop();
    startupMaterializationTrackingActive_ = false;
    startupMaterializationCount_          = 0;
    startupMaterializationWarningEmitted_ = false;
    previewedRooms.clear();
    invites.clear();
    roomids.clear();
    roomReadStatus.clear();
    pendingCurrentRoomId_.clear();
    currentRoom_ = nullptr;
    currentRoomPreview_.reset();

    if (clearAllDrafts) {
        if (const auto settings = UserSettings::instance())
            settings->clearAllComposerDrafts();
    }
}

void
RoomlistModel::removeRoomState(const QString &room_id, bool clearDraftForRoom)
{
    models.remove(room_id);
    invites.remove(room_id);
    previewedRooms.remove(room_id);
    cachedJoinedRooms_.remove(room_id);
    cachedEncryptedRooms_.remove(room_id);
    scheduledPrewarms_.remove(room_id);
    activePrewarms_.remove(room_id);
    prewarmLastAttemptMs_.remove(room_id);
    roomLruAccessMs_.remove(room_id);
    cachedLastMessageBackfillAttempted_.remove(room_id);
    cachedLastMessageBackfillQueued_.remove(room_id);
    cachedLastMessageBackfillInProgress_.remove(room_id);
    roomReadStatus.erase(room_id);
    invalidateCachedLastMessage(room_id);

    if (clearDraftForRoom) {
        if (const auto settings = UserSettings::instance())
            settings->clearComposerDraftForRoom(room_id);
    }

    if (pendingCurrentRoomId_ == room_id)
        pendingCurrentRoomId_.clear();
}

bool
RoomlistModel::hasDraft(const QString &room_id) const
{
    if (room_id.isEmpty())
        return false;

    if (invites.contains(room_id) || previewedRooms.contains(room_id))
        return false;

    const auto settings = UserSettings::instance();
    return settings && settings->hasComposerDraftForRoom(room_id);
}

QString
RoomlistModel::draftPreviewText(const QString &room_id) const
{
    if (!hasDraft(room_id))
        return {};

    const auto settings = UserSettings::instance();
    if (!settings)
        return {};

    auto draft = settings->composerDraftForRoom(room_id);
    draft.replace(QChar(u'\n'), QChar(u' '));
    draft.replace(QChar(u'\r'), QChar(u' '));
    return draft.simplified();
}

void
RoomlistModel::persistDraftForRoom(const QString &room_id, const QString &draftText)
{
    if (room_id.isEmpty())
        return;

    const auto settings = UserSettings::instance();
    if (!settings)
        return;

    if (draftText.trimmed().isEmpty())
        settings->clearComposerDraftForRoom(room_id);
    else
        settings->setComposerDraftForRoom(room_id, draftText);

    const auto idx = roomidToIndex(room_id);
    if (idx == -1)
        return;

    emit dataChanged(index(idx),
                     index(idx),
                     {Roles::HasDraft, Roles::DraftPreview, Roles::LastMessage, Qt::DisplayRole});
}

void
RoomlistModel::updateReadStatus(const std::map<QString, bool> &roomReadStatus_)
{
    std::vector<int> roomsToUpdate;
    roomsToUpdate.resize(roomReadStatus_.size());
    for (const auto &[roomid, roomUnread] : roomReadStatus_) {
        if (roomUnread != roomReadStatus[roomid]) {
            roomsToUpdate.push_back(this->roomidToIndex(roomid));
        }

        this->roomReadStatus[roomid] = roomUnread;
    }

    for (auto idx : roomsToUpdate) {
        emit dataChanged(index(idx),
                         index(idx),
                         {
                           Roles::HasUnreadMessages,
                         });
    }
}

#ifdef KOMAI_DBUS_SYS
void
RoomlistModel::setDbusInterfaceEnabled(bool enabled)
{
    if (enabled) {
        if (dbusHost_)
            return;

        dbusHost_ = new DbusHost{this};
        if (!QDBusConnection::sessionBus().registerObject(
              QStringLiteral("/"), dbusHost_, QDBusConnection::ExportAdaptors)) {
            nhlog::ui()->warn("Failed to register D-Bus interfaces");
            delete dbusHost_;
            dbusHost_ = nullptr;
            return;
        }
        return;
    }

    if (!dbusHost_)
        return;

    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/"));

    delete dbusHost_;
    dbusHost_ = nullptr;
}
#endif

void
RoomlistModel::clear()
{
    beginResetModel();
    resetRoomCollections(true);
    emit currentRoomChanged("");
    endResetModel();
}

#include "moc_RoomlistModel.cpp"
