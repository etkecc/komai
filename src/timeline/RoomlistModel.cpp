// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <algorithm>

#include "ChatPage.h"
#include "EventAccessors.h"
#include "Logging.h"
#include "MainWindow.h"
#include "MxcImageProvider.h"
#include "TimelineModel.h"
#include "TimelineViewManager.h"
#include "Utils.h"
#include "cache/Cache.h"
#include "settings/ui/facade/UserSettingsPage.h"

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

void
RoomlistModel::connectRoomModelSignals(const QString &room_id,
                                       const QSharedPointer<TimelineModel> &roomModel)
{
    connect(MainWindow::instance(),
            &MainWindow::activeChanged,
            roomModel.data(),
            &TimelineModel::lastReadIdOnWindowFocus);
    connect(roomModel.data(),
            &TimelineModel::newEncryptedImage,
            MainWindow::instance()->imageProvider(),
            &MxcImageProvider::addEncryptionInfo);
    connect(roomModel.data(),
            &TimelineModel::forwardToRoom,
            manager,
            &TimelineViewManager::forwardMessageToRoom);
    connect(roomModel.data(), &TimelineModel::lastMessageChanged, this, [room_id, this]() {
        auto idx = this->roomidToIndex(room_id);
        emit dataChanged(index(idx),
                         index(idx),
                         {
                           Roles::HasLoudNotification,
                           Roles::LastMessage,
                           Roles::Time,
                           Roles::Timestamp,
                           Roles::NotificationCount,
                           Qt::DisplayRole,
                         });
    });
    connect(roomModel.data(), &TimelineModel::roomAvatarUrlChanged, this, [room_id, this]() {
        auto idx = this->roomidToIndex(room_id);
        emit dataChanged(index(idx),
                         index(idx),
                         {
                           Roles::AvatarUrl,
                         });
    });
    connect(roomModel.data(), &TimelineModel::roomNameChanged, this, [room_id, this]() {
        auto idx = this->roomidToIndex(room_id);
        emit dataChanged(index(idx),
                         index(idx),
                         {
                           Roles::RoomName,
                         });
    });
    connect(roomModel.data(), &TimelineModel::notificationsChanged, this, [room_id, this]() {
        auto idx = this->roomidToIndex(room_id);
        emit dataChanged(index(idx),
                         index(idx),
                         {
                           Roles::HasLoudNotification,
                           Roles::NotificationCount,
                           Qt::DisplayRole,
                         });

        if (getRoomById(room_id)->isSpace())
            return; // no need to update space notifications

        int total_unread_msgs = 0;

        for (const auto &room : std::as_const(models)) {
            if (!room.isNull() && !room->isSpace())
                total_unread_msgs += room->notificationCount();
        }

        emit totalUnreadMessageCountUpdated(total_unread_msgs);
    });
}

void
RoomlistModel::restoreRoomDraft(const QString &room_id,
                                const QSharedPointer<TimelineModel> &roomModel)
{
    connect(roomModel->input(),
            &InputBar::draftTextChanged,
            this,
            [room_id, this](const QString &draftText) { persistDraftForRoom(room_id, draftText); });

    if (const auto settings = UserSettings::instance()) {
        const auto restoredDraft = settings->composerDraftForRoom(room_id);
        if (!restoredDraft.trimmed().isEmpty() && roomModel->input()->text().trimmed().isEmpty())
            roomModel->input()->setText(restoredDraft);
    }
}

QSharedPointer<TimelineModel>
RoomlistModel::createRoomModel(const QString &room_id)
{
    QSharedPointer<TimelineModel> roomModel(new TimelineModel(manager, room_id));
    refreshCachedRoomMetadata(room_id);
    auto style = UserSettings::instance()->sidebarsRoomListLastMessagePreview();
    roomModel->setDecryptDescription(style == UserSettings::LastMessagePreview::Always);

    restoreRoomDraft(room_id, roomModel);
    connectRoomModelSignals(room_id, roomModel);

    return roomModel;
}

void
RoomlistModel::addRoom(const QString &room_id, bool suppressInsertNotification, const char *reason)
{
    if (!models.contains(room_id)) {
        if (startupMaterializationTrackingActive_ && !activePrewarms_.contains(room_id)) {
            ++startupMaterializationCount_;
            const bool verboseInfo = manager && manager->roomSwitchPerfEnabled();
            if (verboseInfo) {
                nhlog::ui()->info("[perf][room-list] startup_materialized room_id={} count={} "
                                  "reason={}",
                                  room_id.toStdString(),
                                  startupMaterializationCount_,
                                  reason ? reason : "unknown");
            } else {
                nhlog::ui()->debug("[perf][room-list] startup_materialized room_id={} count={} "
                                   "reason={}",
                                   room_id.toStdString(),
                                   startupMaterializationCount_,
                                   reason ? reason : "unknown");
            }

            constexpr int kStartupMaterializationWarnThreshold = 8;
            if (!startupMaterializationWarningEmitted_ &&
                startupMaterializationCount_ >= kStartupMaterializationWarnThreshold) {
                startupMaterializationWarningEmitted_ = true;
                nhlog::ui()->warn(
                  "[perf][room-list] unexpected eager materialization during startup "
                  "(count={} threshold={} last_room_id={} reason={})",
                  startupMaterializationCount_,
                  kStartupMaterializationWarnThreshold,
                  room_id.toStdString(),
                  reason ? reason : "unknown");
            }
        }

        auto newRoom = createRoomModel(room_id);

        // newRoom->updateLastMessage();

        std::vector<QString> previewsToAdd;
        if (newRoom->isSpace()) {
            auto childs = cache::getChildRoomIds(room_id.toStdString());
            for (const auto &c : childs) {
                auto id = QString::fromStdString(c);
                if (!(models.contains(id) || invites.contains(id) || previewedRooms.contains(id))) {
                    previewsToAdd.push_back(std::move(id));
                }
            }
        }

        bool wasInvite                = invites.contains(room_id);
        bool wasPreview               = previewedRooms.contains(room_id);
        const bool alreadyListed      = roomidToIndex(room_id) != -1;
        const bool insertPrimaryRow   = !alreadyListed && !wasInvite && !wasPreview;
        const int insertedRows        = (insertPrimaryRow ? 1 : 0) + (int)previewsToAdd.size();
        const bool shouldNotifyInsert = !suppressInsertNotification && insertedRows > 0;

        if (shouldNotifyInsert)
            beginInsertRows(
              QModelIndex(), (int)roomids.size(), (int)(roomids.size() + insertedRows - 1));

        models.insert(room_id, std::move(newRoom));
        if (wasInvite) {
            auto idx = roomidToIndex(room_id);
            invites.remove(room_id);
            emit dataChanged(index(idx), index(idx));
        } else if (wasPreview) {
            auto idx = roomidToIndex(room_id);
            previewedRooms.remove(room_id);
            emit dataChanged(index(idx), index(idx));
        } else if (alreadyListed) {
            auto idx = roomidToIndex(room_id);
            emit dataChanged(index(idx),
                             index(idx),
                             {Roles::AvatarUrl,
                              Roles::RoomName,
                              Roles::LastMessage,
                              Roles::Time,
                              Roles::Timestamp,
                              Roles::HasDraft,
                              Roles::DraftPreview,
                              Roles::IsInvite,
                              Roles::IsSpace,
                              Roles::Tags,
                              Roles::IsEncrypted,
                              Qt::DisplayRole});
        } else if (!alreadyListed) {
            roomids.push_back(room_id);
        }

        bool switchedToCurrentPreview = false;
        if ((wasInvite || wasPreview) && currentRoomPreview_ &&
            currentRoomPreview_->roomid() == room_id) {
            activateMaterializedCurrentRoom(room_id, false);
            if (manager)
                manager->markRoomSwitchPhaseCpp(room_id, "cpp.room_available_from_preview");
            switchedToCurrentPreview = true;
        }

        // Apply deferred focus requested earlier by setCurrentRoom() while this room
        // did not exist in `models` yet (common right after create-room/direct-chat).
        if (pendingCurrentRoomId_ == room_id) {
            pendingCurrentRoomId_.clear();
            if (!switchedToCurrentPreview) {
                activateMaterializedCurrentRoom(room_id, false);
                if (manager)
                    manager->markRoomSwitchPhaseCpp(room_id, "cpp.pending_room_available");
                nhlog::ui()->debug("Switched to deferred room: {}", room_id.toStdString());

                if (currentRoom_->isSpace())
                    emit spaceSelected(room_id);
            }
        }

        for (auto p : previewsToAdd) {
            previewedRooms.insert(p, std::nullopt);
            roomids.push_back(std::move(p));
        }

        if (shouldNotifyInsert)
            endInsertRows();

        emit ChatPage::instance()->newRoom(room_id);
    }
}

#ifdef KOMAI_DBUS_SYS
void
RoomlistModel::setDbusInterfaceEnabled(bool enabled)
{
    if (enabled) {
        if (dbusInterface_)
            return;

        dbusInterface_ = new DbusBackend{this};
        if (!QDBusConnection::sessionBus().registerObject(
              QStringLiteral("/"), dbusInterface_, QDBusConnection::ExportScriptableSlots)) {
            nhlog::ui()->warn("Failed to register rooms with D-Bus");
            delete dbusInterface_;
            dbusInterface_ = nullptr;
            return;
        }
        return;
    }

    if (!dbusInterface_)
        return;

    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/"));

    delete dbusInterface_;
    dbusInterface_ = nullptr;
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
