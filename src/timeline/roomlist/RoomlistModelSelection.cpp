// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <QTimer>

#include "TimelineViewManager.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"

bool
RoomlistModel::isCurrentRoomSelection(const QString &roomid) const
{
    return currentRoomPreview_ && currentRoomPreview_->roomid() == roomid;
}

void
RoomlistModel::clearCurrentRoomSelection()
{
    allowDeferredStartupCurrentRoomRestore_ = false;
    deferredStartupCurrentRoomId_.clear();
    pendingCurrentRoomId_.clear();
    currentRoomPreview_.reset();
    UserSettings::instance()->setCurrentRoomId(QString());
    notifyCurrentRoomIdChanged();
    scheduleCurrentRoomVisualStateChanged();
}

bool
RoomlistModel::trySelectCurrentMatrixSummaryRoom(const QString &roomid)
{
    if (!matrixJoinedRooms_.contains(roomid))
        return false;

    deferredStartupCurrentRoomId_.clear();
    pendingCurrentRoomId_.clear();
    currentRoomPreview_ = getRoomPreviewById(roomid);
    UserSettings::instance()->setCurrentRoomId(roomid);

    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.matrix_summary_selected");
    komai::logging::ui()->debug("Switched to matrix room summary: {}", roomid.toStdString());

    // Suppress the automatic currentRoomIdChanged → scheduleCurrentMatrixTimelineSelectionUpdate
    // connection so that UI signals fire first (room list highlight, header, pool activation)
    // before the heavier timeline selection work runs.
    if (manager)
        manager->suppressAutoTimelineSelection(true);

    notifyCurrentRoomIdChanged();
    currentRoomVisualStateDeferred_ = false;
    currentRoomVisualStateDeferredRoomId_.clear();
    ++currentRoomVisualStateGeneration_;
    emitCurrentRoomVisualStateChanged();
    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.current_room_summary_changed_emitted");

    if (manager)
        manager->suppressAutoTimelineSelection(false);

    if (manager)
        manager->primeCurrentMatrixTimelineSelection();

    // Without this, opening a room while the room list is paused leaves its
    // unread badge visible until the pause ends — the snapshot that carries the
    // updated (zero) counts is deferred.  This looks like a stuck badge,
    // especially when clicking through several unread rooms in a row.
    //
    // Because dynamic sorting is already disabled during suppression, emitting
    // dataChanged here only updates the visual badge without reordering rooms.
    // The deferred snapshot replaces matrixJoinedRooms_ when the pause ends.
    //
    // Skip this optimistic mirror when read-receipt sending is disabled: with
    // the toggle off, the auto-mark-as-read path is gated upstream and no
    // receipt is sent, so the server still considers the room unread.  Zeroing
    // the badge here would make the UI lie until the next sync corrected it.
    if (interactionSuppressed_ && UserSettings::instance()->timelineReadReceiptsEnabled()) {
        auto &room = matrixJoinedRooms_[roomid];
        if (room.unreadMessages > 0 || room.notificationCount > 0 || room.highlightCount > 0) {
            room.unreadMessages    = 0;
            room.notificationCount = 0;
            room.highlightCount    = 0;
            const auto idx         = roomidToIndex(roomid);
            if (idx != -1) {
                emit dataChanged(
                  index(idx),
                  index(idx),
                  {Roles::HasUnreadMessages, Roles::UnreadCount, Roles::HasLoudNotification});
            }
            emitAttentionCount();
        }
    }

    return true;
}

bool
RoomlistModel::trySelectCurrentPreviewRoom(const QString &roomid)
{
    if (!(invites.contains(roomid) || previewedRooms.contains(roomid)))
        return false;

    deferredStartupCurrentRoomId_.clear();
    pendingCurrentRoomId_.clear();
    currentRoomPreview_ = getRoomPreviewById(roomid);

    if (currentRoomPreview_->isFetched()) {
        if (manager)
            manager->markRoomSwitchPhaseCpp(roomid, "cpp.preview_selected");
        komai::logging::ui()->debug("Switched to (preview): {}",
                                    currentRoomPreview_->roomid_.toStdString());
    } else {
        if (manager)
            manager->markRoomSwitchPhaseCpp(roomid, "cpp.preview_placeholder_selected");
        komai::logging::ui()->debug("Switched to (empty): {}",
                                    currentRoomPreview_->roomid_.toStdString());
    }

    notifyCurrentRoomIdChanged();
    scheduleCurrentRoomVisualStateChanged();
    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.current_room_preview_changed_emitted");

    return true;
}

void
RoomlistModel::deferStartupCurrentRoomRestore(const QString &roomid)
{
    deferredStartupCurrentRoomId_ = roomid;
    pendingCurrentRoomId_.clear();

    if (firstChatFrameReleased_) {
        // The frame-swap latch already fired (single-shot connection in
        // MainWindow), so there will be no second resume callback to release
        // this restore.  Perform it inline instead of leaving the gate stuck.
        allowDeferredStartupCurrentRoomRestore_ = true;
        komai::logging::ui()->info(
          "Restoring saved room inline; first chat frame already released: {}",
          roomid.toStdString());
        setCurrentRoom(roomid);
        return;
    }

    allowDeferredStartupCurrentRoomRestore_ = false;
    komai::logging::ui()->info("Queued saved-room restore for after the first chat frame: {}",
                               roomid.toStdString());
}

void
RoomlistModel::deferCurrentRoomSelection(const QString &roomid)
{
    pendingCurrentRoomId_ = roomid;
    if (manager)
        manager->markRoomSwitchPhaseCpp(roomid, "cpp.switch_deferred_room_unavailable");
    komai::logging::ui()->debug("Deferring room switch until room is available: {}",
                                roomid.toStdString());
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
        !allowDeferredStartupCurrentRoomRestore_ && !currentRoomPreview_) {
        komai::logging::ui()->info(
          "Ignoring premature saved-room restore before startup release: {}", roomid.toStdString());
        return;
    }

    allowDeferredStartupCurrentRoomRestore_ = false;
    deferredStartupCurrentRoomId_.clear();

    komai::logging::ui()->debug("Trying to switch to: {}", roomid.toStdString());

    // Invited rooms are handled via a dialog, not by selecting them in the timeline.
    // Skip the dialog if this room was just accepted — the join callback triggers
    // setCurrentRoom before the room list has refreshed, so isInvite is still true.
    if (matrixJoinedRooms_.contains(roomid) && matrixJoinedRooms_.value(roomid).isInvite) {
        if (recentlyAcceptedInviteRoomId_ == roomid) {
            recentlyAcceptedInviteRoomId_.clear();
        } else {
            if (manager)
                emit manager->openInviteResponseDialog(roomid);
            return;
        }
    }

    if (manager)
        manager->markRoomSwitchRequested(roomid, "setCurrentRoom");

    // Rust-owned room summaries should win selection as soon as the room is present in the
    // matrix-sdk room list.
    if (trySelectCurrentMatrixSummaryRoom(roomid))
        return;

    if (trySelectCurrentPreviewRoom(roomid))
        return;

    deferCurrentRoomSelection(roomid);
}

void
RoomlistModel::resumeDeferredStartupCurrentRoomRestore()
{
    // Latch the frame-swap signal so a later deferStartupCurrentRoomRestore
    // call (e.g. when the matrix-sdk room-list snapshot lands after the first
    // frame swap) can restore inline rather than wait forever for a single-
    // shot connection that already fired.
    firstChatFrameReleased_ = true;

    if (deferredStartupCurrentRoomId_.isEmpty())
        return;

    const auto roomid = deferredStartupCurrentRoomId_;

    if (currentRoomPreview_)
        return;

    if (!matrixJoinedRooms_.contains(roomid))
        return;

    allowDeferredStartupCurrentRoomRestore_ = true;
    komai::logging::ui()->info("Resuming saved-room restore after the first chat frame: {}",
                               roomid.toStdString());
    setCurrentRoom(roomid);
}
