// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include "ChatPage.h"
#include "Logging.h"
#include "MainWindow.h"
#include "TimelineModel.h"
#include "TimelineViewManager.h"
#include "cache/Cache.h"
#include "providers/MxcImageProvider.h"
#include "settings/ui/facade/UserSettingsPage.h"

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
