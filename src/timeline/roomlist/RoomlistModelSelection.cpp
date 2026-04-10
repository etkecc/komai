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
    nhlog::ui()->debug("Switched to matrix room summary: {}", roomid.toStdString());

    if (manager)
        manager->primeCurrentMatrixTimelineSelection();

    notifyCurrentRoomIdChanged();
    deferCurrentRoomVisualState(roomid);
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

    notifyCurrentRoomIdChanged();
    scheduleCurrentRoomVisualStateChanged();
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
        !allowDeferredStartupCurrentRoomRestore_ && !currentRoomPreview_) {
        nhlog::ui()->info("Ignoring premature saved-room restore before startup release: {}",
                          roomid.toStdString());
        return;
    }

    allowDeferredStartupCurrentRoomRestore_ = false;
    deferredStartupCurrentRoomId_.clear();

    nhlog::ui()->debug("Trying to switch to: {}", roomid.toStdString());

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
    if (deferredStartupCurrentRoomId_.isEmpty())
        return;

    const auto roomid = deferredStartupCurrentRoomId_;

    if (currentRoomPreview_)
        return;

    if (!matrixJoinedRooms_.contains(roomid))
        return;

    allowDeferredStartupCurrentRoomRestore_ = true;
    nhlog::ui()->info("Resuming saved-room restore after the first chat frame: {}",
                      roomid.toStdString());
    setCurrentRoom(roomid);
}
