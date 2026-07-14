// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/TimelineViewManager.h"

#include <QPointer>

#include "chat/ChatPage.h"
#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"
#include "timeline/RoomlistModel.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "timeline/view/TimelineViewManagerMatrixTimelineInternal.h"
#include "ui/MainWindow.h"
#include "ui/NotificationAction.h"
#include "utils/QtWorkerTask.h"

using namespace komai::timeline::view::internal;

void
TimelineViewManager::handleMatrixBackendRoomListSnapshotUpdated(std::uint64_t handleId)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    if (waitingForFirstSync_) {
        // First snapshot: process immediately so the room list appears without delay.
        komai::logging::ui()->info(
          "Clearing waitingForFirstSync from first matrix-sdk room-list snapshot "
          "for handle {}",
          handleId);
        waitingForFirstSync_ = false;
        emit waitingForFirstSyncChanged(false);
        rooms_->refreshMatrixBackendRooms();
        scheduleMatrixSidebarRefresh();
        return;
    }

    // Subsequent snapshots: coalesce rapid updates so the UI thread is not
    // blocked by repeated full model resets during the initial sync burst.
    matrixRoomListRefreshPending_ = true;
    if (!matrixRoomListRefreshQueued_) {
        matrixRoomListRefreshQueued_ = true;
        QTimer::singleShot(200, this, [this]() {
            matrixRoomListRefreshQueued_ = false;
            if (!matrixRoomListRefreshPending_)
                return;
            matrixRoomListRefreshPending_ = false;
            rooms_->refreshMatrixBackendRooms();
            scheduleMatrixSidebarRefresh();
        });
    }
}
void
TimelineViewManager::handleMatrixBackendNotificationReceived(std::uint64_t handleId,
                                                             const QString &roomId,
                                                             const QString &eventId)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId || !rooms_)
        return;

    rooms_->queueMatrixNotificationFetch(handleId, roomId, eventId);
}
void
TimelineViewManager::handleMatrixBackendRoomTimelineSnapshotUpdated(std::uint64_t handleId,
                                                                    const QString &roomId)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    // Accept snapshot updates for any room that has a per-room model, not
    // just the active room.  This keeps background room models up to date
    // with real-time events from their concurrent Rust timeline loops.
    if (!perRoomModels_.contains(roomId))
        return;

    // A pending event jump waiting on this snapshot is woken when the
    // snapshot is *applied* to the model (refreshCurrentMatrixTimeline),
    // not here: the apply is async, and re-running the jump resolver
    // against the not-yet-updated model burns its pagination attempts
    // without making progress.

    if (roomId == activeMatrixTimelineRoomId_)
        markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_snapshot_signal");

    if (waitingForFirstSync_) {
        komai::logging::ui()->info("Clearing waitingForFirstSync from first active matrix-sdk room "
                                   "timeline snapshot for handle {} room '{}'",
                                   handleId,
                                   roomId.toStdString());
        waitingForFirstSync_ = false;
        emit waitingForFirstSyncChanged(false);
    }

    matrixTimelineRefreshPendingRoomIds_.insert(roomId);

    scheduleCurrentMatrixTimelineRefresh();

    // If we have an active thread view for this room, signal the thread
    // timeline loop to rebuild.  TimelineFocus::Thread doesn't reliably
    // receive sync events in matrix-sdk 0.16, so we rebuild on each room
    // sync update to pick up new thread events.
    if (roomId == activeMatrixTimelineRoomId_ && !matrixTimelineThreadEventId_.isEmpty()) {
        try {
            ::komai::rust::matrix_refresh_thread_timeline(handleId);
        } catch (const std::exception &) {
        }
    }
}
void
TimelineViewManager::handleMatrixBackendRoomTimelinePaginationStateChanged(std::uint64_t handleId,
                                                                           const QString &roomId,
                                                                           bool inProgress)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    auto *model = perRoomModels_.value(roomId, nullptr);
    if (!model)
        return;

    model->setPaginationInProgress(inProgress);

    // A pagination that produces no new events (e.g. the timeline start was
    // already reached) yields no snapshot update, which would leave a
    // pending event jump waiting forever. Wake it on a delay: if a snapshot
    // did result from this pagination, its application clears the awaiting
    // flag first and this timer becomes a no-op. Waking immediately would
    // race the async snapshot apply and burn the jump's pagination attempts
    // against a stale model.
    if (!inProgress && matrixTimelinePendingJumpRoomId_ == roomId &&
        matrixTimelinePendingJumpAwaitingSnapshot_) {
        const auto pendingEventId = matrixTimelinePendingJumpEventId_;
        QTimer::singleShot(600, this, [this, roomId, pendingEventId]() {
            if (matrixTimelinePendingJumpRoomId_ != roomId ||
                matrixTimelinePendingJumpEventId_ != pendingEventId ||
                !matrixTimelinePendingJumpAwaitingSnapshot_) {
                return;
            }
            matrixTimelinePendingJumpAwaitingSnapshot_ = false;
            emit matrixTimelineStateChanged();
        });
    }
}
void
TimelineViewManager::handleMatrixBackendRoomPinnedEventsChanged(std::uint64_t handleId,
                                                                const QString &roomId,
                                                                const QStringList &eventIds)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    // The Rust reconciler only keeps forwarders alive for subscribed rooms,
    // but subscribe/unsubscribe is debounced so a late update for a room
    // we've already switched away from can still land here. Only apply for
    // the currently active room.
    if (roomId != activeMatrixTimelineRoomId_)
        return;

    if (matrixTimelinePinnedEventIds_ == eventIds)
        return;

    matrixTimelinePinnedEventIds_ = eventIds;
    emit matrixTimelineStateChanged();
}
void
TimelineViewManager::handleMatrixBackendSyncStopped(std::uint64_t handleId,
                                                    const QString &reason,
                                                    bool isAuthError)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    komai::logging::ui()->warn("Matrix-sdk sync stopped for handle {} (auth_error={}): {}",
                               handleId,
                               isAuthError,
                               reason.toStdString());

    if (isAuthError) {
        auto *chatPage = qobject_cast<ChatPage *>(parent());
        if (chatPage)
            emit chatPage->dropToLoginPageCb(
              tr("Your session has expired. Please sign in again.\n\n(%1)").arg(reason));
    }
}
void
TimelineViewManager::handleMatrixBackendSyncConnectionStateChanged(std::uint64_t handleId,
                                                                   bool isConnected)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;

    if (isConnected_ == isConnected)
        return;

    isConnected_ = isConnected;
    updateConnectedState();
}
bool
TimelineViewManager::paginateActiveMatrixTimelineBackwards(int pageSize)
{
    if (matrixTimelineModel_ &&
        matrixTimelineModel_->revealOlderItems(pageSize > 0 ? pageSize : 50)) {
        return true;
    }

    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to paginate matrix-sdk room timeline without an active "
          "runtime handle or selected matrix room");
        return false;
    }

    const auto clampedPageSize = static_cast<uint16_t>(std::clamp(pageSize, 0, 500));

    QString error;
    if (!komai::MatrixBackendRuntimeService::paginateActiveRoomTimelineBackwards(
          handleId, clampedPageSize, &error)) {
        komai::logging::ui()->warn(
          "Failed to paginate matrix-sdk room timeline backwards for '{}' on handle {}: {}",
          activeMatrixTimelineRoomId_.toStdString(),
          handleId,
          error.toStdString());
        return false;
    }

    return true;
}
void
TimelineViewManager::handleMatrixBackendTypingUsersUpdated(std::uint64_t handleId,
                                                           const QString &roomId,
                                                           const QStringList &displayNames)
{
    auto *mainWindow = MainWindow::instance();
    if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
        return;
    if (activeMatrixTimelineRoomId_ != roomId)
        return;

    if (matrixTimelineTypingUsers_ != displayNames) {
        matrixTimelineTypingUsers_ = displayNames;
        emit matrixTimelineTypingUsersChanged();
    }
}
void
TimelineViewManager::sendActiveMatrixTypingNotice(bool typing)
{
    if (activeMatrixTimelineRoomId_.isEmpty())
        return;

    auto *mainWindow = MainWindow::instance();
    if (!mainWindow)
        return;
    const auto handleId = mainWindow->matrixBackendHandleId();
    if (handleId == 0)
        return;

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, typing]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          komai::MatrixBackendRuntimeService::sendTypingNotice(context, handleId, roomId, typing);
      },
      [](TimelineViewManager *) {});
}
