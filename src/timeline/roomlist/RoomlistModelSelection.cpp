// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <QTimer>

#include "TimelineModel.h"
#include "TimelineViewManager.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {
void
scheduleLastReadUpdate(const QSharedPointer<TimelineModel> &roomModel, const QString &roomId)
{
    if (roomModel.isNull() || roomId.isEmpty())
        return;

    QTimer::singleShot(0, roomModel.data(), [roomModel, roomId]() {
        if (!roomModel.isNull())
            roomModel->updateLastReadId(roomId);
    });
}
}

bool
RoomlistModel::isCurrentRoomSelection(const QString &roomid) const
{
    return (currentRoom_ && currentRoom_->roomId() == roomid) ||
           (currentRoomPreview_ && currentRoomPreview_->roomid() == roomid);
}

void
RoomlistModel::clearCurrentRoomSelection()
{
    allowDeferredStartupCurrentRoomRestore_ = false;
    deferredStartupCurrentRoomId_.clear();
    pendingCurrentRoomId_.clear();
    currentRoom_ = nullptr;
    currentRoomPreview_.reset();
    UserSettings::instance()->setCurrentRoomId(QString());
    emit currentRoomChanged("");
    scheduleLruEviction();
}

void
RoomlistModel::activateMaterializedCurrentRoom(const QString &room_id, bool updateLastMessage)
{
    currentRoom_ = models.value(room_id);
    currentRoomPreview_.reset();
    if (updateLastMessage)
        currentRoom_->updateLastMessage();
    scheduleLastReadUpdate(currentRoom_, room_id);
    emit currentRoomChanged(room_id);
    scheduleCurrentRoomTimelineWarmup(room_id);
}

bool
RoomlistModel::trySelectCurrentMaterializedRoom(const QString &roomid)
{
    if (!models.contains(roomid))
        return false;

    deferredStartupCurrentRoomId_.clear();
    pendingCurrentRoomId_.clear();
    activateMaterializedCurrentRoom(roomid, true);
    UserSettings::instance()->setCurrentRoomId(roomid);
    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.current_room_changed_emitted");
    nhlog::ui()->debug("Switched to: {}", roomid.toStdString());

    if (currentRoom_->isSpace())
        emit spaceSelected(roomid);

    return true;
}

bool
RoomlistModel::trySelectCurrentMatrixSummaryRoom(const QString &roomid)
{
    if (!matrixJoinedRooms_.contains(roomid))
        return false;

    deferredStartupCurrentRoomId_.clear();
    pendingCurrentRoomId_.clear();
    currentRoom_        = nullptr;
    currentRoomPreview_ = getRoomPreviewById(roomid);
    UserSettings::instance()->setCurrentRoomId(roomid);

    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.matrix_summary_selected");
    nhlog::ui()->debug("Switched to matrix room summary: {}", roomid.toStdString());

    if (manager)
        manager->primeCurrentMatrixTimelineSelection();

    emit currentRoomChanged(roomid);
    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.current_room_summary_changed_emitted");

    return true;
}

bool
RoomlistModel::trySelectCurrentPreviewRoom(const QString &roomid)
{
    if (!(invites.contains(roomid) || previewedRooms.contains(roomid)))
        return false;

    deferredStartupCurrentRoomId_.clear();
    pendingCurrentRoomId_.clear();
    currentRoom_        = nullptr;
    currentRoomPreview_ = getRoomPreviewById(roomid);

    if (currentRoomPreview_->isFetched()) {
        if (manager)
            manager->markRoomSwitchPhaseCpp(roomid, "cpp.preview_selected");
        nhlog::ui()->debug("Switched to (preview): {}", currentRoomPreview_->roomid_.toStdString());
    } else {
        if (manager)
            manager->markRoomSwitchPhaseCpp(roomid, "cpp.preview_placeholder_selected");
        nhlog::ui()->debug("Switched to (empty): {}", currentRoomPreview_->roomid_.toStdString());
    }

    emit currentRoomChanged("");
    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.current_room_preview_changed_emitted");

    return true;
}

void
RoomlistModel::deferStartupCurrentRoomRestore(const QString &roomid)
{
    allowDeferredStartupCurrentRoomRestore_ = false;
    deferredStartupCurrentRoomId_           = roomid;
    pendingCurrentRoomId_.clear();
    nhlog::ui()->info("Queued saved-room restore for after the first chat frame: {}",
                      roomid.toStdString());
}

void
RoomlistModel::deferCurrentRoomSelection(const QString &roomid)
{
    pendingCurrentRoomId_ = roomid;
    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.switch_deferred_room_unavailable");
    nhlog::ui()->debug("Deferring room switch until room is available: {}", roomid.toStdString());
}

void
RoomlistModel::setCurrentRoom(const QString &roomid)
{
    if (isCurrentRoomSelection(roomid))
        return;

    if (roomid.isEmpty()) {
        clearCurrentRoomSelection();
        return;
    }

    if (!deferredStartupCurrentRoomId_.isEmpty() && roomid == deferredStartupCurrentRoomId_ &&
        !allowDeferredStartupCurrentRoomRestore_ && !currentRoom_ && !currentRoomPreview_) {
        nhlog::ui()->info("Ignoring premature saved-room restore before startup release: {}",
                          roomid.toStdString());
        return;
    }

    allowDeferredStartupCurrentRoomRestore_ = false;
    deferredStartupCurrentRoomId_.clear();

    // After the first explicit room selection, startup eager-materialization tracking
    // is no longer meaningful.
    startupMaterializationTrackingActive_ = false;

    nhlog::ui()->debug("Trying to switch to: {}", roomid.toStdString());
    if (manager)
        manager->markRoomSwitchRequested(roomid, "setCurrentRoom");

    touchRoomLru(roomid);
    scheduleLruEviction();

    // On the migration branch, Rust-owned room summaries should win selection as soon as the
    // room is present in the matrix-sdk room list, even if a legacy TimelineModel still exists.
    if (trySelectCurrentMatrixSummaryRoom(roomid))
        return;

    if (!models.contains(roomid) && cachedJoinedRooms_.contains(roomid))
        ensureRoomModel(roomid, false, "setCurrentRoom");

    if (trySelectCurrentMaterializedRoom(roomid))
        return;

    if (trySelectCurrentPreviewRoom(roomid))
        return;

    deferCurrentRoomSelection(roomid);
}

void
RoomlistModel::resumeDeferredStartupCurrentRoomRestore()
{
    if (deferredStartupCurrentRoomId_.isEmpty())
        return;

    const auto roomid = deferredStartupCurrentRoomId_;

    if (currentRoom_ || currentRoomPreview_)
        return;

    if (!matrixJoinedRooms_.contains(roomid) && !cachedJoinedRooms_.contains(roomid))
        return;

    allowDeferredStartupCurrentRoomRestore_ = true;
    nhlog::ui()->info("Resuming saved-room restore after the first chat frame: {}",
                      roomid.toStdString());
    setCurrentRoom(roomid);
}

void
RoomlistModel::refetchOnlineKeyBackupKeys()
{
    nhlog::crypto()->warn(
      "Ignoring legacy room-list key-backup refetch on the matrix-sdk migration branch");
}

void
RoomlistModel::clearDecryptionErrors()
{
    nhlog::crypto()->warn(
      "Ignoring legacy room-list clear-decryption-errors request on the matrix-sdk migration "
      "branch");
}
