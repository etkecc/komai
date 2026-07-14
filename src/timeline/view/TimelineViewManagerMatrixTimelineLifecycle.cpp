// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/TimelineViewManager.h"

#include <cmath>

#include <QDateTime>
#include <QPointer>
#include <QTimer>

#include <thread>

#include "chat/ChatPage.h"
#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/CommunitiesModel.h"
#include "timeline/RoomlistModel.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "timeline/view/TimelineViewManagerMatrixTimelineInternal.h"
#include "ui/MainWindow.h"
#include "utils/MediaIcons.h"
#include "utils/QtWorkerTask.h"

using namespace komai::timeline::view::internal;

void
TimelineViewManager::primeCurrentMatrixTimelineSelection()
{
    // Run the room-to-timeline handoff immediately when the room summary is
    // selected, instead of waiting for currentRoomChanged fanout through QML
    // and other listeners before we even start the active Rust timeline.
    scheduleCurrentMatrixTimelineSelectionUpdate();
}
void
TimelineViewManager::scheduleCurrentMatrixTimelineSelectionUpdate()
{
    const auto preview = rooms_->currentRoomPreview();
    const auto roomId  = preview.isMatrixSummary() ? preview.roomid() : QString();

    if (!matrixTimelineSelectionUpdateQueued_ && roomId == activeMatrixTimelineRoomId_)
        return;

    if (!roomId.isEmpty())
        markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_selection_queued");

    if (matrixTimelineSelectionUpdateQueued_)
        return;

    matrixTimelineSelectionUpdateQueued_ = true;
    const auto currentPreview            = rooms_->currentRoomPreview();
    const auto currentRoomId =
      currentPreview.isMatrixSummary() ? currentPreview.roomid() : QString();
    if (!currentRoomId.isEmpty())
        markRoomSwitchPhaseCpp(currentRoomId, "cpp.matrix_timeline_selection_dequeued");

    // Selecting the active Rust timeline now just kicks off a background task,
    // so delaying it behind later event-loop work only adds avoidable room-open
    // latency. Run it immediately while the room-selection state is already hot.
    updateCurrentMatrixTimelineSelection();
    matrixTimelineSelectionUpdateQueued_ = false;
}
void
TimelineViewManager::updateCurrentMatrixTimelineSelection()
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;

    // Backend gone — clear everything without saving (state is meaningless
    // without a backend connection).
    if (handleId == 0) {
        clearCurrentMatrixTimeline(false);
        return;
    }

    const auto preview = rooms_->currentRoomPreview();
    const auto roomId  = preview.isMatrixSummary() ? preview.roomid() : QString();

    if (activeMatrixTimelineRoomId_ == roomId) {
        return;
    }

    // Save outgoing room's interaction state so it can be restored later.
    // This runs before any early returns so that navigating to "no room"
    // (closing last tab) or to an invite room also preserves state.
    if (!activeMatrixTimelineRoomId_.isEmpty()) {
        PerRoomInteractionState outgoing;
        outgoing.replyEventId           = matrixTimelineReplyEventId_;
        outgoing.replySenderDisplayName = matrixTimelineReplySenderDisplayName_;
        outgoing.replySenderId          = matrixTimelineReplySenderId_;
        outgoing.replyBody              = matrixTimelineReplyBody_;
        outgoing.threadEventId          = matrixTimelineThreadEventId_;
        outgoing.editEventId            = matrixTimelineEditEventId_;
        outgoing.editMessageKind        = matrixTimelineEditMessageKind_;

        // Sync user-edited body/filename from QML items into the pending structs.
        for (int i = 0; i < static_cast<int>(pendingMatrixAttachments_.size()) &&
                        i < matrixPendingAttachmentItems_.size();
             ++i) {
            if (auto *item = matrixPendingAttachmentItems_.at(i)) {
                pendingMatrixAttachments_[static_cast<size_t>(i)].filename =
                  item->filename().trimmed();
                pendingMatrixAttachments_[static_cast<size_t>(i)].body = item->body().trimmed();
            }
        }

        // Skip the in-flight entry (index 0) — it will complete on its own.
        const size_t skip =
          (matrixAttachmentUploadInFlight_ && !pendingMatrixAttachments_.empty()) ? 1 : 0;
        outgoing.attachments.assign(pendingMatrixAttachments_.begin() +
                                      static_cast<std::ptrdiff_t>(skip),
                                    pendingMatrixAttachments_.end());

        if (!outgoing.isEmpty())
            perRoomInteractionState_[activeMatrixTimelineRoomId_] = std::move(outgoing);
        else
            perRoomInteractionState_.remove(activeMatrixTimelineRoomId_);
    }

    if (roomId.isEmpty()) {
        clearCurrentMatrixTimeline(true);
        return;
    }

    clearActiveMatrixReplyState();
    clearActiveMatrixThreadState();
    clearActiveMatrixEditState();

    // Clear staged uploads.  Keep the in-flight entry so
    // finishPendingMatrixAttachment() can pop it.
    {
        const bool keepFront =
          matrixAttachmentUploadInFlight_ && !pendingMatrixAttachments_.empty();
        if (keepFront) {
            auto front = std::move(pendingMatrixAttachments_.front());
            pendingMatrixAttachments_.clear();
            pendingMatrixAttachments_.push_back(std::move(front));
        } else {
            pendingMatrixAttachments_.clear();
        }
        for (auto *attachment : matrixPendingAttachmentItems_) {
            if (attachment)
                attachment->deleteLater();
        }
        matrixPendingAttachmentItems_.clear();
    }

    if (!matrixTimelineTypingUsers_.isEmpty()) {
        matrixTimelineTypingUsers_.clear();
        emit matrixTimelineTypingUsersChanged();
    }

    setPreferredInitialMatrixTimelinePageSize(preferredInitialMatrixTimelinePageSize_ > 0
                                                ? preferredInitialMatrixTimelinePageSize_
                                                : fallbackInitialMatrixTimelinePageSize());

    const auto warmupGeneration      = ++matrixTimelineWarmupGuardGeneration_;
    matrixTimelineWarmupGuardActive_ = true;
    QTimer::singleShot(1500, this, [this, roomId, warmupGeneration]() {
        if (matrixTimelineWarmupGuardGeneration_ != warmupGeneration)
            return;
        if (activeMatrixTimelineRoomId_ != roomId)
            return;
        matrixTimelineWarmupGuardActive_ = false;
    });

    markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_select_begin");
    QString error;
    if (!komai::MatrixBackendRuntimeService::selectActiveRoomTimeline(handleId, roomId, &error)) {
        komai::logging::ui()->warn(
          "Failed to select active matrix-sdk room timeline for '{}' on handle {}: {}",
          roomId.toStdString(),
          handleId,
          error.toStdString());
        clearCurrentMatrixTimeline(false);
        return;
    }
    markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_select_done");

    // Release any stale subscription from the previous active room, then
    // subscribe to the new one.  Subscriptions populate the state store
    // with `m.room.pinned_events` and keep it live-updated, so pinned state
    // stays fresh without us polling `/state` on every room switch.
    if (!activeMatrixTimelineRoomId_.isEmpty() && activeMatrixTimelineRoomId_ != roomId) {
        komai::MatrixBackendRuntimeService::unsubscribeFromRoom(handleId,
                                                                activeMatrixTimelineRoomId_);
    }
    // The new room's pinned event list will arrive via
    // `handleMatrixBackendRoomPinnedEventsChanged` once the Rust reconciler
    // spawns the per-room forwarder. Clear the stale list from the previous
    // room so the header doesn't briefly show the wrong pins.
    if (!matrixTimelinePinnedEventIds_.isEmpty()) {
        matrixTimelinePinnedEventIds_.clear();
        emit matrixTimelineStateChanged();
    }
    komai::MatrixBackendRuntimeService::subscribeToRoom(handleId, roomId);

    activeMatrixTimelineRoomId_             = roomId;
    matrixTimelineInitialPrefetchAttempted_ = false;
    {
        const auto now = QDateTime::currentMSecsSinceEpoch();
        if (const auto it = matrixTimelineFrequentReactionsCache_.constFind(roomId);
            it != matrixTimelineFrequentReactionsCache_.constEnd() &&
            (now - it->timestampMs) <
              settings::core::definitions::kReactionFrequencyCacheDurationMs) {
            matrixTimelineFrequentReactions_ = it->reactions;
        } else {
            matrixTimelineFrequentReactions_.clear();
        }
    }
    refreshActiveMatrixTimelineRoomStateAsync();

    // Swap to the per-room model (creates one if needed).  Don't clear old
    // models — they stay populated so switching back is instant.
    auto *roomModel        = qobject_cast<komai::MatrixTimelineModel *>(ensureModelForRoom(roomId));
    matrixTimelineModel_   = roomModel;
    matrixTimelineLoading_ = roomModel ? (roomModel->count() == 0) : true;

    // Restore saved interaction state for the incoming room.
    if (auto it = perRoomInteractionState_.find(roomId); it != perRoomInteractionState_.end()) {
        auto saved = std::move(*it);
        perRoomInteractionState_.erase(it);

        setActiveMatrixReplyState(
          saved.replyEventId, saved.replySenderId, saved.replySenderDisplayName, saved.replyBody);
        setActiveMatrixThreadState(saved.threadEventId);
        setActiveMatrixEditState(saved.editEventId, saved.editMessageKind);

        // Re-attach the matrix-sdk runtime to the restored thread.
        // The runtime's "active thread" pointer still references whichever
        // thread was opened last (possibly in another tab/room). Without
        // this resubscribe, /relations refreshes route to the wrong thread
        // (issue #82). The thread-subscription cache makes a re-entry
        // near-free: the call resolves to a warm-path notify and the
        // snapshot is delivered to QML on the next event loop tick.
        if (!saved.threadEventId.isEmpty()) {
            markRoomSwitchPhaseCpp(roomId, "cpp.matrix_thread_restore_begin");
            // Ensure a cached model exists for the (room, thread) we're
            // restoring. If one already exists from a previous viewing in
            // this session, QML rebinds to it and shows its last snapshot
            // immediately; the upcoming re-subscribe refreshes it in the
            // background.
            auto *entry = ensureThreadTimelineEntry(roomId, saved.threadEventId);
            markRoomSwitchPhaseCpp(roomId, "cpp.matrix_thread_restore_entry_ensured");
            if (roomSwitchPerfEnabled()) {
                komai::logging::ui()->info(
                  "[room-switch-perf] phase=cpp.matrix_thread_restore_entry_ensured "
                  "room='{}' thread='{}' cached={} item_count={}",
                  roomId.toStdString(),
                  saved.threadEventId.toStdString(),
                  entry && entry->model && entry->model->count() > 0,
                  entry && entry->model ? entry->model->count() : 0);
            }

            auto *mainWindow    = MainWindow::instance();
            const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
            if (handleId != 0) {
                if (entry)
                    entry->loading = true;
                QElapsedTimer subscribeTimer;
                subscribeTimer.start();
                try {
                    ::komai::rust::matrix_subscribe_to_thread_timeline(
                      handleId, roomId.toStdString(), saved.threadEventId.toStdString());
                } catch (const std::exception &e) {
                    if (entry)
                        entry->loading = false;
                    komai::logging::ui()->warn(
                      "Failed to reattach matrix-sdk thread subscription on room switch: {}",
                      e.what());
                }
                const auto subscribeElapsedUs = subscribeTimer.nsecsElapsed() / 1000;
                markRoomSwitchPhaseCpp(roomId, "cpp.matrix_thread_subscribe_done");
                if (roomSwitchPerfEnabled()) {
                    komai::logging::ui()->info(
                      "[room-switch-perf] phase=cpp.matrix_thread_subscribe_done "
                      "room='{}' thread='{}' us={}",
                      roomId.toStdString(),
                      saved.threadEventId.toStdString(),
                      subscribeElapsedUs);
                }
            }
        }

        for (auto &att : saved.attachments) {
            pendingMatrixAttachments_.push_back(std::move(att));
            const auto &back = pendingMatrixAttachments_.back();
            matrixPendingAttachmentItems_.push_back(new MatrixPendingAttachmentUpload(
              back.filePath,
              back.filename,
              back.mimeType,
              utils::fileTypeIconSource(back.mimeType),
              matrixPendingAttachmentThumbnail(back.filePath, back.mimeType),
              this));
            if (!back.body.isEmpty())
                matrixPendingAttachmentItems_.back()->setBody(back.body);
        }
    }

    markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_loading_started");
    emit matrixTimelineStateChanged();
    // The active (room, thread) pair may have changed: the outgoing room's
    // thread was cleared at line ~357 and the incoming room may have a
    // restored thread. Either flip changes the model the active-entry
    // getter resolves to, so QML needs a re-binding signal here.
    emit matrixThreadTimelineChanged();
}
void
TimelineViewManager::scheduleCurrentMatrixTimelineRefresh()
{
    if (matrixTimelineRefreshPendingRoomIds_.isEmpty())
        return;

    if (matrixTimelineRefreshQueued_)
        return;

    // Prioritize the active room so the user sees updates immediately.
    const auto roomId = matrixTimelineRefreshPendingRoomIds_.contains(activeMatrixTimelineRoomId_)
                          ? activeMatrixTimelineRoomId_
                          : *matrixTimelineRefreshPendingRoomIds_.constBegin();

    matrixTimelineRefreshQueued_ = true;
    if (roomId == activeMatrixTimelineRoomId_)
        markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_refresh_queued");
    QTimer::singleShot(0, this, [this, roomId]() {
        matrixTimelineRefreshQueued_ = false;

        if (!matrixTimelineRefreshPendingRoomIds_.contains(roomId)) {
            // This room was removed while queued; schedule for whatever remains.
            if (!matrixTimelineRefreshPendingRoomIds_.isEmpty())
                scheduleCurrentMatrixTimelineRefresh();
            return;
        }

        matrixTimelineRefreshPendingRoomIds_.remove(roomId);
        if (roomId == activeMatrixTimelineRoomId_)
            markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_refresh_dequeued");
        refreshCurrentMatrixTimeline(roomId);
    });
}
void
TimelineViewManager::refreshActiveMatrixTimelineRoomStateAsync()
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    const auto roomId   = activeMatrixTimelineRoomId_;

    if (handleId == 0 || roomId.isEmpty()) {
        if (applyActiveMatrixTimelineRoomState({}, false, false))
            emit matrixTimelineStateChanged();
        return;
    }

    const auto now = QDateTime::currentMSecsSinceEpoch();
    QStringList cachedFrequentReactions;
    bool shouldFetchFrequentReactions = true;
    if (const auto it = matrixTimelineFrequentReactionsCache_.constFind(roomId);
        it != matrixTimelineFrequentReactionsCache_.constEnd() &&
        (now - it->timestampMs) < settings::core::definitions::kReactionFrequencyCacheDurationMs) {
        cachedFrequentReactions      = it->reactions;
        shouldFetchFrequentReactions = false;
    }
    const bool canCacheEmptyFrequentReactions =
      matrixTimelineModel_ && matrixTimelineModel_->count() > 0;

    matrixTimelineRoomStateRefreshPending_       = true;
    matrixTimelineRoomStateRefreshPendingRoomId_ = roomId;

    if (matrixTimelineRoomStateInFlightRequestId_ != 0 &&
        matrixTimelineRoomStateInFlightRoomId_ == roomId) {
        return;
    }

    matrixTimelineRoomStateRefreshPending_ = false;
    matrixTimelineRoomStateRefreshPendingRoomId_.clear();

    const auto requestId                      = ++matrixTimelineRoomStateRequestId_;
    matrixTimelineRoomStateInFlightRequestId_ = requestId;
    matrixTimelineRoomStateInFlightRoomId_    = roomId;

    QPointer<TimelineViewManager> guard(this);
    std::thread([guard,
                 handleId,
                 roomId,
                 requestId,
                 cachedFrequentReactions = std::move(cachedFrequentReactions),
                 shouldFetchFrequentReactions,
                 canCacheEmptyFrequentReactions]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        MatrixTimelineRoomStateSnapshot snapshot;
        QString frequentReactionsError;
        QString permissionsError;

        snapshot.frequentReactions              = cachedFrequentReactions;
        snapshot.canCacheEmptyFrequentReactions = canCacheEmptyFrequentReactions;

        if (shouldFetchFrequentReactions) {
            const auto frequentReactions =
              komai::MatrixBackendRuntimeService::fetchRoomFrequentReactions(
                context,
                handleId,
                roomId,
                settings::core::definitions::kReactionFrequencyLookbackDays,
                settings::core::definitions::kMaxQuickReactionSlots,
                settings::core::definitions::kMaxReactionScanEvents,
                &frequentReactionsError);
            if (frequentReactions) {
                snapshot.frequentReactions        = *frequentReactions;
                snapshot.fetchedFrequentReactions = true;
            }
        }

        const auto permissions = komai::MatrixBackendRuntimeService::fetchRoomRedactionPermissions(
          context, handleId, roomId, &permissionsError);
        if (permissions) {
            snapshot.canRedactOwn   = permissions->canRedactOwn;
            snapshot.canRedactOther = permissions->canRedactOther;
        }

        if (!guard)
            return;

        QMetaObject::invokeMethod(
          guard,
          [guard,
           handleId,
           roomId,
           requestId,
           snapshot               = std::move(snapshot),
           frequentReactionsError = std::move(frequentReactionsError),
           permissionsError       = std::move(permissionsError)]() mutable {
              if (!guard)
                  return;

              const bool isInFlightRequest =
                guard->matrixTimelineRoomStateInFlightRequestId_ == requestId &&
                guard->matrixTimelineRoomStateInFlightRoomId_ == roomId;
              if (isInFlightRequest) {
                  guard->matrixTimelineRoomStateInFlightRequestId_ = 0;
                  guard->matrixTimelineRoomStateInFlightRoomId_.clear();
              }

              auto *mainWindow = MainWindow::instance();
              if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
                  return;

              if (guard->activeMatrixTimelineRoomId_ != roomId) {
                  if (guard->matrixTimelineRoomStateRefreshPending_ &&
                      guard->matrixTimelineRoomStateRefreshPendingRoomId_ ==
                        guard->activeMatrixTimelineRoomId_) {
                      guard->refreshActiveMatrixTimelineRoomStateAsync();
                  }
                  return;
              }

              if (!frequentReactionsError.isEmpty()) {
                  komai::logging::ui()->warn(
                    "Failed to fetch matrix-sdk room frequent reactions for '{}' "
                    "on handle {}: {}",
                    roomId.toStdString(),
                    handleId,
                    frequentReactionsError.toStdString());
              }

              if (!permissionsError.isEmpty()) {
                  komai::logging::ui()->warn(
                    "Failed to fetch matrix-sdk room redaction permissions for "
                    "'{}' on handle {}: {}",
                    roomId.toStdString(),
                    handleId,
                    permissionsError.toStdString());
              }

              if (snapshot.fetchedFrequentReactions && (!snapshot.frequentReactions.isEmpty() ||
                                                        snapshot.canCacheEmptyFrequentReactions)) {
                  guard->matrixTimelineFrequentReactionsCache_.insert(
                    roomId,
                    TimelineViewManager::MatrixTimelineFrequentReactionsCacheEntry{
                      .reactions   = snapshot.frequentReactions,
                      .timestampMs = QDateTime::currentMSecsSinceEpoch(),
                    });
              }

              if (guard->applyActiveMatrixTimelineRoomState(std::move(snapshot.frequentReactions),
                                                            snapshot.canRedactOwn,
                                                            snapshot.canRedactOther)) {
                  emit guard->matrixTimelineStateChanged();
              }

              if (guard->matrixTimelineRoomStateRefreshPending_ &&
                  guard->matrixTimelineRoomStateRefreshPendingRoomId_ == roomId) {
                  guard->refreshActiveMatrixTimelineRoomStateAsync();
              }
          },
          Qt::QueuedConnection);
    }).detach();
}
bool
TimelineViewManager::applyActiveMatrixTimelineRoomState(QStringList frequentReactions,
                                                        bool canRedactOwn,
                                                        bool canRedactOther)
{
    if (matrixTimelineFrequentReactions_ == frequentReactions &&
        matrixTimelineCanRedactOwn_ == canRedactOwn &&
        matrixTimelineCanRedactOther_ == canRedactOther) {
        return false;
    }

    matrixTimelineFrequentReactions_ = std::move(frequentReactions);
    matrixTimelineCanRedactOwn_      = canRedactOwn;
    matrixTimelineCanRedactOther_    = canRedactOther;
    return true;
}
void
TimelineViewManager::invalidateMatrixTimelineFrequentReactionsCache(const QString &roomId)
{
    const auto trimmedRoomId = roomId.trimmed();
    if (trimmedRoomId.isEmpty())
        return;

    matrixTimelineFrequentReactionsCache_.remove(trimmedRoomId);
}
void
TimelineViewManager::refreshCurrentMatrixTimeline(const QString &roomId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;

    if (handleId == 0 || roomId.isEmpty()) {
        clearCurrentMatrixTimeline(false);
        return;
    }

    if (matrixTimelineRefreshInFlightRequestId_ != 0 &&
        matrixTimelineRefreshInFlightRoomId_ == roomId) {
        return;
    }

    const auto requestId                    = ++matrixTimelineRefreshRequestId_;
    matrixTimelineRefreshInFlightRequestId_ = requestId;
    matrixTimelineRefreshInFlightRoomId_    = roomId;

    if (roomId == activeMatrixTimelineRoomId_)
        markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_fetch_begin");

    QPointer<TimelineViewManager> guard(this);
    std::thread([guard, handleId, roomId, requestId]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        QElapsedTimer fetchTimer;
        fetchTimer.start();
        const auto items = komai::MatrixBackendRuntimeService::fetchRoomTimelineSnapshot(
          context, handleId, roomId, &error);
        const auto fetchElapsedUs = fetchTimer.nsecsElapsed() / 1000;

        if (!guard)
            return;

        QMetaObject::invokeMethod(
          guard,
          [guard, handleId, roomId, requestId, items, error, fetchElapsedUs]() mutable {
              if (!guard)
                  return;

              const bool isInFlightRequest =
                guard->matrixTimelineRefreshInFlightRequestId_ == requestId &&
                guard->matrixTimelineRefreshInFlightRoomId_ == roomId;
              if (isInFlightRequest) {
                  guard->matrixTimelineRefreshInFlightRequestId_ = 0;
                  guard->matrixTimelineRefreshInFlightRoomId_.clear();
              }

              auto *mainWindow = MainWindow::instance();
              if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
                  return;

              // Look up the per-room model for this refresh target.
              auto *targetModel = guard->perRoomModels_.value(roomId, nullptr);
              if (!targetModel) {
                  // Room was evicted from the model pool while the fetch was in flight.
                  if (!guard->matrixTimelineRefreshPendingRoomIds_.isEmpty()) {
                      guard->scheduleCurrentMatrixTimelineRefresh();
                  }
                  return;
              }

              const bool isActiveRoom = (roomId == guard->activeMatrixTimelineRoomId_);

              if (isActiveRoom)
                  guard->markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_fetch_done");

              if (guard->roomSwitchPerfEnabled()) {
                  komai::logging::ui()->info(
                    "[room-switch-perf] "
                    "phase=cpp.matrix_timeline_fetch_thread_done room='{}' us={}",
                    roomId.toStdString(),
                    fetchElapsedUs);
              }

              if (!items) {
                  komai::logging::ui()->warn(
                    "Failed to fetch matrix-sdk room timeline snapshot for '{}' on handle {}: {}",
                    roomId.toStdString(),
                    handleId,
                    error.toStdString());
                  if (isActiveRoom)
                      guard->clearCurrentMatrixTimeline(false);
                  return;
              }

              const auto itemCount         = items->size();
              const auto currentModelCount = targetModel->count();

              // For background (non-active) rooms, just update the model and
              // schedule any pending refreshes.  No warmup guard, loading
              // state, or initial prefetch logic needed.
              if (!isActiveRoom) {
                  targetModel->replaceItems(*items);
                  if (!guard->matrixTimelineRefreshPendingRoomIds_.isEmpty()) {
                      guard->scheduleCurrentMatrixTimelineRefresh();
                  }
                  return;
              }

              // Active room: full warmup guard + loading + prefetch logic.
              const auto preferredInitialPageSize =
                guard->preferredInitialMatrixTimelinePageSize_ > 0
                  ? guard->preferredInitialMatrixTimelinePageSize_
                  : fallbackInitialMatrixTimelinePageSize();
              const auto canDelayFirstPaint = guard->matrixTimelineLoading_ &&
                                              targetModel->count() == 0 &&
                                              !guard->matrixTimelineInitialPrefetchAttempted_;
              if (guard->matrixTimelineWarmupGuardActive_ &&
                  shouldIgnoreMatrixTimelineWarmupShrink(currentModelCount, itemCount)) {
                  if (guard->roomSwitchPerfEnabled()) {
                      komai::logging::ui()->info(
                        "[room-switch-perf] phase=cpp.matrix_timeline_warmup_shrink_ignored "
                        "room='{}' current_count={} next_count={}",
                        roomId.toStdString(),
                        currentModelCount,
                        itemCount);
                  }

                  if (guard->matrixTimelineRefreshPendingRoomIds_.contains(roomId)) {
                      guard->scheduleCurrentMatrixTimelineRefresh();
                  }
                  return;
              }

              if (canDelayFirstPaint && itemCount > 0 && itemCount < preferredInitialPageSize) {
                  const auto shortfall =
                    std::clamp(static_cast<int>(preferredInitialPageSize - itemCount), 1, 24);
                  QString paginateError;
                  if (komai::MatrixBackendRuntimeService::paginateActiveRoomTimelineBackwards(
                        handleId, static_cast<uint16_t>(shortfall), &paginateError)) {
                      guard->matrixTimelineInitialPrefetchAttempted_ = true;
                      if (guard->roomSwitchPerfEnabled()) {
                          komai::logging::ui()->info(
                            "[room-switch-perf] phase=cpp.matrix_timeline_initial_prefetch "
                            "room='{}' item_count={} target_count={} request_count={}",
                            roomId.toStdString(),
                            itemCount,
                            preferredInitialPageSize,
                            shortfall);
                      }

                      // Schedule a fallback refresh in case the pagination never
                      // produces a new snapshot (e.g. server-side auth failure).
                      // If a real snapshot arrives first, the pending flag will
                      // already be set and this becomes a no-op.
                      QTimer::singleShot(500, guard, [guard, roomId]() {
                          if (!guard)
                              return;
                          if (guard->activeMatrixTimelineRoomId_ != roomId)
                              return;
                          if (guard->matrixTimelineModel_ &&
                              guard->matrixTimelineModel_->count() > 0)
                              return;

                          komai::logging::ui()->warn(
                            "Initial prefetch fallback: forcing refresh for room '{}' "
                            "because the model is still empty",
                            roomId.toStdString());

                          guard->matrixTimelineRefreshPendingRoomIds_.insert(roomId);
                          guard->scheduleCurrentMatrixTimelineRefresh();
                      });
                      return;
                  }

                  komai::logging::ui()->warn(
                    "Failed to prefetch additional matrix-sdk room timeline items "
                    "for '{}' on handle {}: {}",
                    roomId.toStdString(),
                    handleId,
                    paginateError.toStdString());
                  guard->matrixTimelineInitialPrefetchAttempted_ = true;
              }

              targetModel->replaceItems(*items);
              guard->markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_model_replaced");

              auto stateChanged = false;
              guard->refreshActiveMatrixTimelineRoomStateAsync();

              if (guard->matrixTimelineLoading_) {
                  const bool hasContent = itemCount > 0 || targetModel->count() > 0;
                  if (hasContent) {
                      guard->matrixTimelineLoading_ = false;
                      guard->markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_loading_finished");
                      stateChanged = true;
                  } else {
                      // Empty snapshot while loading — backward pagination
                      // is still in progress.  Keep showing "Loading this
                      // room…" instead of "Nothing has loaded for this room
                      // yet."  A fallback clears the flag for genuinely empty
                      // rooms (no messages ever).
                      QTimer::singleShot(10000, guard, [guard, roomId]() {
                          if (!guard || guard->activeMatrixTimelineRoomId_ != roomId)
                              return;
                          if (!guard->matrixTimelineLoading_)
                              return;
                          guard->matrixTimelineLoading_ = false;
                          emit guard->matrixTimelineStateChanged();
                      });
                  }
              }

              // A pending event jump paginates and then waits for this
              // refresh; the QML resolver only re-runs on
              // matrixTimelineStateChanged, so a snapshot applied to an
              // already-loaded room must still notify it or the jump stalls
              // after its first pagination request. The awaiting flag is
              // cleared here — at apply time, when the model reflects the
              // paginated events — rather than when the snapshot
              // notification arrives, so the resolver never re-checks a
              // stale model.
              if (guard->matrixTimelinePendingJumpRoomId_ == roomId &&
                  !guard->matrixTimelinePendingJumpEventId_.isEmpty()) {
                  guard->matrixTimelinePendingJumpAwaitingSnapshot_ = false;
                  stateChanged                                      = true;
              }

              if (stateChanged)
                  emit guard->matrixTimelineStateChanged();

              guard->rooms_->flushDeferredCurrentRoomVisualState(roomId);
              guard->markRoomSwitchPhaseCpp(roomId, "cpp.matrix_timeline_snapshot_refreshed");

              if (!guard->matrixTimelineRefreshPendingRoomIds_.isEmpty()) {
                  guard->scheduleCurrentMatrixTimelineRefresh();
              }
          },
          Qt::QueuedConnection);
    }).detach();
}
void
TimelineViewManager::setPreferredInitialMatrixTimelinePageSize(int pageSize)
{
    const auto clampedPageSize = std::clamp(pageSize, 0, 50);
    const bool changed         = preferredInitialMatrixTimelinePageSize_ != clampedPageSize;

    preferredInitialMatrixTimelinePageSize_ = clampedPageSize;

    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId != 0 && clampedPageSize > 0) {
        QString error;
        if (!komai::MatrixBackendRuntimeService::setActiveRoomTimelineInitialPageSize(
              handleId, static_cast<uint16_t>(clampedPageSize), &error)) {
            komai::logging::ui()->warn(
              "Failed to update active matrix-sdk room timeline initial page size "
              "on handle {}: {}",
              handleId,
              error.toStdString());
        }
    }

    if (!changed)
        return;

    if (roomSwitchPerfEnabled_) {
        komai::logging::ui()->info(
          "[room-switch-perf] phase=cpp.matrix_timeline_initial_page_size_hint page_size={}",
          preferredInitialMatrixTimelinePageSize_);
    }
}
void
TimelineViewManager::clearCurrentMatrixTimeline(bool stopBackendTask)
{
    bool stateChanged = clearActiveMatrixReplyState();
    stateChanged |= clearActiveMatrixThreadState();
    stateChanged |= clearActiveMatrixEditState();
    if (!matrixTimelinePendingJumpRoomId_.isEmpty() ||
        !matrixTimelinePendingJumpEventId_.isEmpty() ||
        matrixTimelinePendingJumpPaginationAttempts_ != 0 ||
        matrixTimelinePendingJumpAwaitingSnapshot_ || matrixTimelinePendingJumpExhaustedLogged_) {
        matrixTimelinePendingJumpRoomId_.clear();
        matrixTimelinePendingJumpEventId_.clear();
        matrixTimelinePendingJumpPaginationAttempts_ = 0;
        matrixTimelinePendingJumpAwaitingSnapshot_   = false;
        matrixTimelinePendingJumpExhaustedLogged_    = false;
        stateChanged                                 = true;
    }

    if (!pendingMatrixAttachments_.empty() || !matrixPendingAttachmentItems_.empty()) {
        pendingMatrixAttachments_.clear();
        for (auto *attachment : matrixPendingAttachmentItems_) {
            if (attachment)
                attachment->deleteLater();
        }
        matrixPendingAttachmentItems_.clear();
        stateChanged = true;
    }

    // Drop every cached thread timeline model and unsubscribe from
    // whatever Rust still considers active. Re-entering any of these
    // threads later creates a fresh entry and triggers a re-subscribe.
    if (!matrixThreadTimelineEntries_.isEmpty()) {
        destroyAllThreadTimelineEntries();
        emit matrixThreadTimelineChanged();
    }
    {
        auto *mainWindow    = MainWindow::instance();
        const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
        if (handleId != 0) {
            try {
                ::komai::rust::matrix_unsubscribe_from_thread_timeline(handleId);
            } catch (const std::exception &) {
            }
        }
    }

    if (matrixTimelineLoading_) {
        matrixTimelineLoading_ = false;
        stateChanged           = true;
    }

    matrixTimelineWarmupGuardActive_ = false;
    ++matrixTimelineWarmupGuardGeneration_;

    if (!matrixTimelinePinnedEventIds_.isEmpty()) {
        matrixTimelinePinnedEventIds_.clear();
        stateChanged = true;
    }

    if (!matrixTimelineFrequentReactions_.isEmpty()) {
        matrixTimelineFrequentReactions_.clear();
        stateChanged = true;
    }

    if (matrixTimelineCanRedactOwn_ || matrixTimelineCanRedactOther_) {
        matrixTimelineCanRedactOwn_   = false;
        matrixTimelineCanRedactOther_ = false;
        stateChanged                  = true;
    }

    if (!matrixTimelineTypingUsers_.isEmpty()) {
        matrixTimelineTypingUsers_.clear();
        emit matrixTimelineTypingUsersChanged();
    }

    if (!activeMatrixTimelineRoomId_.isEmpty()) {
        if (stopBackendTask) {
            const auto *mainWindow = MainWindow::instance();
            const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
            if (handleId != 0) {
                komai::MatrixBackendRuntimeService::unsubscribeFromRoom(
                  handleId, activeMatrixTimelineRoomId_);
                QString error;
                if (!komai::MatrixBackendRuntimeService::selectActiveRoomTimeline(
                      handleId, QString(), &error)) {
                    komai::logging::ui()->warn(
                      "Failed to clear active matrix-sdk room timeline on handle {}: {}",
                      handleId,
                      error.toStdString());
                }
            }
        }

        activeMatrixTimelineRoomId_.clear();
        stateChanged = true;
    }

    matrixTimelineRefreshQueued_ = false;
    matrixTimelineRefreshPendingRoomIds_.clear();
    matrixTimelineRefreshInFlightRequestId_ = 0;
    matrixTimelineRefreshInFlightRoomId_.clear();
    matrixTimelineRoomStateRefreshPending_ = false;
    matrixTimelineRoomStateRefreshPendingRoomId_.clear();
    ++matrixTimelineRoomStateRequestId_;
    matrixTimelineRoomStateInFlightRequestId_ = 0;
    matrixTimelineRoomStateInFlightRoomId_.clear();
    clearMatrixReadMarkerQueue();
    matrixTimelineInitialPrefetchAttempted_ = false;

    // Don't clear the per-room model — it stays populated in perRoomModels_
    // so switching back to this room is instant.  Just null the active pointer.
    matrixTimelineModel_ = nullptr;

    if (stateChanged)
        emit matrixTimelineStateChanged();
}
