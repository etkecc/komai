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
#include "timeline/rust/MatrixTimelineModel.h"
#include "timeline/view/TimelineViewManagerMatrixTimelineInternal.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"

using namespace komai::timeline::view::internal;

void
TimelineViewManager::fetchActiveMatrixRoomThreadRoots(const QString &include,
                                                      const QString &from,
                                                      int limit)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty())
        return;

    const auto roomId = activeMatrixTimelineRoomId_;

    struct Result
    {
        uint64_t handleId;
        QString roomId;
        QVariantList items;
        QString nextBatchToken;
        QString error;
    };

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, include, from, limit]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const auto result = komai::MatrixBackendRuntimeService::fetchRoomThreadRoots(
            context, handleId, roomId, include, from, static_cast<uint32_t>(limit), &error);
          Result out;
          out.handleId = handleId;
          out.roomId   = roomId;
          if (result) {
              out.items          = result->items;
              out.nextBatchToken = result->nextBatchToken;
          } else {
              out.error = error;
          }
          return out;
      },
      [](TimelineViewManager *manager, Result result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;
          if (manager->activeMatrixTimelineRoomId_ != result.roomId)
              return;
          if (!result.error.isEmpty()) {
              komai::logging::ui()->warn("Failed to fetch thread roots: {}",
                                         result.error.toStdString());
              return;
          }
          emit manager->matrixRoomThreadRootsReady(result.items, result.nextBatchToken);
      });
}
bool
TimelineViewManager::queueActiveMatrixThread(const QString &threadEventId)
{
    if (activeMatrixTimelineRoomId_.isEmpty())
        return false;

    const auto trimmedThreadEventId = threadEventId.trimmed();
    if (trimmedThreadEventId.isEmpty())
        return false;

    // Re-entering the same thread (e.g. clicking "Reply in thread" on a
    // message while already viewing that thread) must not re-subscribe.
    // A re-subscription rebuilds the SDK TimelineFocus::Thread timeline,
    // which drops local echoes (sync events don't flow into thread-focused
    // timelines in matrix-sdk 0.16) and briefly replaces the snapshot with
    // server state lagging behind the user's just-sent reply.
    if (matrixTimelineThreadEventId_ == trimmedThreadEventId) {
        focusMessageInput();
        return true;
    }

    // Get-or-create the cached model for the (room, thread) we're
    // activating. A cached entry from a previous viewing in this session
    // shows its last snapshot immediately; a brand-new entry starts
    // empty and is filled when the subscribe-triggered snapshot lands.
    auto *entry = ensureThreadTimelineEntry(activeMatrixTimelineRoomId_, trimmedThreadEventId);

    if (setActiveMatrixThreadState(trimmedThreadEventId))
        emit matrixTimelineStateChanged();

    // Subscribe to the thread timeline.  This spawns a background Rust
    // task that builds a TimelineFocus::Thread timeline, subscribes to
    // its diff stream, and notifies C++ via snapshot updates: same
    // pipeline as the room timeline. No explicit refresh needed.
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId != 0) {
        if (entry)
            entry->loading = true;
        emit matrixThreadTimelineChanged();

        try {
            ::komai::rust::matrix_subscribe_to_thread_timeline(
              handleId,
              activeMatrixTimelineRoomId_.toStdString(),
              trimmedThreadEventId.toStdString());
        } catch (const std::exception &e) {
            komai::logging::ui()->warn("Failed to init thread timeline: {}", e.what());
            if (entry)
                entry->loading = false;
            emit matrixThreadTimelineChanged();
            focusMessageInput();
            return true;
        }
    } else {
        // No backend handle: still emit so QML rebinds to whatever the
        // active-entry getter resolves to under the new threadEventId.
        emit matrixThreadTimelineChanged();
    }

    focusMessageInput();
    return true;
}
void
TimelineViewManager::clearActiveMatrixThread()
{
    if (!clearActiveMatrixThreadState())
        return;

    // Unsubscribe from the thread timeline. Rust keeps the subscription
    // cached for warm re-entry, and we keep the C++ entry around too:
    // re-opening this thread later rebinds QML to the cached model.
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId != 0) {
        try {
            ::komai::rust::matrix_unsubscribe_from_thread_timeline(handleId);
        } catch (const std::exception &) {
        }
    }

    // matrixTimelineThreadEventId_ is now empty, so the active-entry
    // getter resolves to nullptr. Notify both signals so QML's
    // threadViewActive flips false and the ListView unbinds from the
    // previously-cached model.
    emit matrixTimelineStateChanged();
    emit matrixThreadTimelineChanged();
}
QAbstractItemModel *
TimelineViewManager::matrixThreadTimelineModel() const
{
    const auto *entry = activeThreadTimelineEntry();
    return entry ? entry->model : nullptr;
}
bool
TimelineViewManager::matrixThreadTimelineLoading() const
{
    const auto *entry = activeThreadTimelineEntry();
    return entry ? entry->loading : false;
}
void
TimelineViewManager::handleMatrixBackendThreadTimelineSnapshotUpdated(std::uint64_t handleId,
                                                                      const QString &roomId,
                                                                      const QString &threadRootId)
{
    if (activeMatrixTimelineRoomId_ != roomId)
        return;
    if (matrixTimelineThreadEventId_ != threadRootId)
        return;

    markRoomSwitchPhaseCpp(roomId, "cpp.matrix_thread_snapshot_queue");

    // Fetch the snapshot from Rust on a worker thread.
    struct Result
    {
        uint64_t handleId;
        QString roomId;
        QString threadRootId;
        QVector<komai::MatrixTimelineItem> items;
        QString error;
        qint64 fetchUs = 0;
    };

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, threadRootId]() {
          QElapsedTimer fetchTimer;
          fetchTimer.start();
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const auto result = komai::MatrixBackendRuntimeService::fetchThreadTimelineSnapshot(
            context, handleId, &error);
          Result out;
          out.handleId     = handleId;
          out.roomId       = roomId;
          out.threadRootId = threadRootId;
          if (result) {
              out.items = *result;
          } else {
              out.error = error;
          }
          out.fetchUs = fetchTimer.nsecsElapsed() / 1000;
          return out;
      },
      [](TimelineViewManager *manager, Result result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          // Re-check active state at callback time. Between the snapshot
          // request and the worker completing, the user may have switched
          // to a different thread; `fetchThreadTimelineSnapshot` returns
          // Rust's currently-active snapshot which would no longer match
          // the (roomId, threadRootId) the signal was originally for.
          if (manager->activeMatrixTimelineRoomId_ != result.roomId)
              return;
          if (manager->matrixTimelineThreadEventId_ != result.threadRootId)
              return;

          auto *entry = manager->ensureThreadTimelineEntry(result.roomId, result.threadRootId);
          if (entry)
              entry->loading = false;

          if (!result.error.isEmpty()) {
              komai::logging::ui()->warn("Failed to fetch thread timeline snapshot: {}",
                                         result.error.toStdString());
              emit manager->matrixThreadTimelineChanged();
              return;
          }

          const int itemCount = result.items.size();
          QElapsedTimer replaceTimer;
          replaceTimer.start();
          if (entry && entry->model)
              entry->model->replaceItems(std::move(result.items));
          const auto replaceUs = replaceTimer.nsecsElapsed() / 1000;
          manager->markRoomSwitchPhaseCpp(result.roomId, "cpp.matrix_thread_snapshot_applied");
          if (manager->roomSwitchPerfEnabled()) {
              komai::logging::ui()->info(
                "[room-switch-perf] phase=cpp.matrix_thread_snapshot_applied "
                "room='{}' thread='{}' items={} fetch_us={} replace_us={}",
                result.roomId.toStdString(),
                result.threadRootId.toStdString(),
                itemCount,
                result.fetchUs,
                replaceUs);
          }
          emit manager->matrixThreadTimelineChanged();
      });
}
void
TimelineViewManager::paginateActiveMatrixThreadTimelineBackwards(int limit,
                                                                 const QString &expectedRoomId)
{
    // Drop pagination requests that originate from a pool entry that's no
    // longer the foreground view. During a tab switch the outgoing
    // MatrixRoomView's ListView re-layouts as the pool deactivates it, which
    // can fire a final `atYEndChanged` whose slot races against the QML
    // binding propagation that would otherwise have disabled the Connection.
    // Without this guard the request reaches the runtime and paginates the
    // thread the user just left, slowing the switch.
    if (!expectedRoomId.isEmpty() && expectedRoomId != activeMatrixTimelineRoomId_)
        return;

    // Only paginate when a thread is genuinely active. clearActiveMatrixThreadState
    // empties this during room switches, which catches the race window where
    // C++ has cleared but the Rust runtime's `active_thread_key` hasn't yet been
    // reattached to the new room's thread.
    if (matrixTimelineThreadEventId_.isEmpty())
        return;

    auto *entry = activeThreadTimelineEntry();
    if (entry && entry->loading)
        return;

    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0)
        return;

    if (entry)
        entry->loading = true;
    emit matrixThreadTimelineChanged();

    // Capture the (room, thread) being paginated so the callback clears
    // loading on the correct entry even if the user has since activated
    // another thread.
    const auto paginatingRoomId        = activeMatrixTimelineRoomId_;
    const auto paginatingThreadEventId = matrixTimelineThreadEventId_;

    struct Result
    {
        QString roomId;
        QString threadEventId;
        QString error;
        bool hitStart = false;
    };

    komai::qt_worker_task::runQueued(
      this,
      [handleId, limit, paginatingRoomId, paginatingThreadEventId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const auto result = komai::MatrixBackendRuntimeService::paginateThreadTimelineBackwards(
            context, handleId, static_cast<uint16_t>(limit), &error);
          Result out;
          out.roomId        = paginatingRoomId;
          out.threadEventId = paginatingThreadEventId;
          if (result) {
              out.hitStart = *result;
          } else {
              out.error = error;
          }
          return out;
      },
      [](TimelineViewManager *manager, Result result) {
          // The pagination results flow through the subscription receiver,
          // which triggers handleMatrixBackendThreadTimelineSnapshotUpdated.
          // We just need to clear the loading state here on the entry the
          // request was issued for, regardless of what the user activated
          // in the meantime.
          const auto key = QPair<QString, QString>(result.roomId, result.threadEventId);
          auto it        = manager->matrixThreadTimelineEntries_.find(key);
          if (it != manager->matrixThreadTimelineEntries_.end())
              it->loading = false;
          if (!result.error.isEmpty()) {
              komai::logging::ui()->warn("Failed to paginate thread timeline: {}",
                                         result.error.toStdString());
          }
          emit manager->matrixThreadTimelineChanged();
      });
}
bool
TimelineViewManager::setActiveMatrixThreadState(const QString &threadEventId)
{
    const auto trimmed = threadEventId.trimmed();
    if (matrixTimelineThreadEventId_ == trimmed)
        return false;

    matrixTimelineThreadEventId_ = trimmed;
    return true;
}
bool
TimelineViewManager::clearActiveMatrixThreadState()
{
    if (matrixTimelineThreadEventId_.isEmpty())
        return false;

    matrixTimelineThreadEventId_.clear();
    return true;
}
TimelineViewManager::ThreadTimelineEntry *
TimelineViewManager::ensureThreadTimelineEntry(const QString &roomId, const QString &threadEventId)
{
    if (roomId.isEmpty() || threadEventId.isEmpty())
        return nullptr;
    const auto key = QPair<QString, QString>(roomId, threadEventId);
    auto it        = matrixThreadTimelineEntries_.find(key);
    if (it == matrixThreadTimelineEntries_.end()) {
        ThreadTimelineEntry entry;
        entry.model = new komai::MatrixTimelineModel(this);
        it          = matrixThreadTimelineEntries_.insert(key, entry);
    }
    return &it.value();
}
TimelineViewManager::ThreadTimelineEntry *
TimelineViewManager::activeThreadTimelineEntry()
{
    if (activeMatrixTimelineRoomId_.isEmpty() || matrixTimelineThreadEventId_.isEmpty())
        return nullptr;
    auto it = matrixThreadTimelineEntries_.find(
      {activeMatrixTimelineRoomId_, matrixTimelineThreadEventId_});
    return it != matrixThreadTimelineEntries_.end() ? &it.value() : nullptr;
}
const TimelineViewManager::ThreadTimelineEntry *
TimelineViewManager::activeThreadTimelineEntry() const
{
    if (activeMatrixTimelineRoomId_.isEmpty() || matrixTimelineThreadEventId_.isEmpty())
        return nullptr;
    auto it = matrixThreadTimelineEntries_.constFind(
      {activeMatrixTimelineRoomId_, matrixTimelineThreadEventId_});
    return it != matrixThreadTimelineEntries_.constEnd() ? &it.value() : nullptr;
}
void
TimelineViewManager::destroyAllThreadTimelineEntries()
{
    for (auto &entry : matrixThreadTimelineEntries_) {
        if (entry.model)
            entry.model->deleteLater();
    }
    matrixThreadTimelineEntries_.clear();
}
