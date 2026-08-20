// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/TimelineViewManager.h"

#include <QDateTime>
#include <QPointer>
#include <QTimer>

#include "chat/ChatPage.h"
#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"
#include "models/ReadReceiptsModel.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineEventTypes.h"
#include "timeline/formattedcode/RawJsonFormatter.h"
#include "timeline/rust/MatrixTimelineModel.h"
#include "timeline/view/TimelineViewManagerMatrixTimelineInternal.h"
#include "ui/MainWindow.h"
#include "ui/NotificationAction.h"
#include "ui/Theme.h"
#include "utils/QtWorkerTask.h"

using namespace komai::timeline::view::internal;

bool
TimelineViewManager::redactActiveMatrixTimelineEvent(const QString &eventId, const QString &reason)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to redact a matrix-sdk room event without an active runtime "
          "handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    const auto roomId        = activeMatrixTimelineRoomId_;
    const auto trimmedReason = reason.trimmed();
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId, trimmedReason]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::redactRoomEvent(
                            context, handleId, roomId, trimmedEventId, trimmedReason, &error)
                            .has_value();
          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedEventId,
            .detail   = {},
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *manager, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (!result.ok) {
              komai::logging::ui()->warn(
                "Failed to redact matrix-sdk room event '{}' in '{}' on handle {}: {}",
                result.eventId.toStdString(),
                result.roomId.toStdString(),
                result.handleId,
                result.error.toStdString());
              mainWindow->showNotification(
                TimelineViewManager::tr("Failed to delete message: %1").arg(result.error));
              return;
          }

          if (manager->activeMatrixTimelineRoomId_ != result.roomId ||
              !manager->matrixTimelineModel_)
              return;

          manager->matrixTimelineModel_->redactItemByEventId(result.eventId);
      });
    return true;
}
bool
TimelineViewManager::cancelActiveMatrixTimelineLocalEcho(const QString &transactionId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to cancel a matrix-sdk local echo without an active runtime "
          "handle or selected matrix room");
        return false;
    }

    const auto trimmedTransactionId = transactionId.trimmed();
    if (trimmedTransactionId.isEmpty())
        return false;

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedTransactionId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::cancelRoomLocalEcho(
            context, handleId, roomId, trimmedTransactionId, &error);
          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedTransactionId,
            .detail   = {},
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *manager, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (!result.ok) {
              komai::logging::ui()->warn(
                "Failed to cancel matrix-sdk local echo '{}' in '{}' on handle {}: {}",
                result.eventId.toStdString(),
                result.roomId.toStdString(),
                result.handleId,
                result.error.toStdString());
              mainWindow->showNotification(
                TimelineViewManager::tr("Failed to cancel unsent message: %1").arg(result.error));
              return;
          }

          if (manager->activeMatrixTimelineRoomId_ != result.roomId ||
              !manager->matrixTimelineModel_)
              return;

          manager->matrixTimelineModel_->removeItemByTransactionId(result.eventId);
      });
    return true;
}
bool
TimelineViewManager::retryActiveMatrixTimelineLocalEcho(const QString &transactionId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to retry a matrix-sdk local echo without an active runtime "
          "handle or selected matrix room");
        return false;
    }

    const auto trimmedTransactionId = transactionId.trimmed();
    if (trimmedTransactionId.isEmpty())
        return false;

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedTransactionId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::retryRoomLocalEcho(
            context, handleId, roomId, trimmedTransactionId, &error);
          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedTransactionId,
            .detail   = {},
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *manager, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (!result.ok) {
              komai::logging::ui()->warn(
                "Failed to retry matrix-sdk local echo '{}' in '{}' on handle {}: {}",
                result.eventId.toStdString(),
                result.roomId.toStdString(),
                result.handleId,
                result.error.toStdString());
              mainWindow->showNotification(
                TimelineViewManager::tr("Failed to retry unsent message: %1").arg(result.error));
              return;
          }

          // On success the send queue re-attempts the send; the timeline
          // subscription will push an updated snapshot (delivery_state flips
          // from "failed" back to "pending", and either "sent" or "failed"
          // once the attempt resolves). No optimistic UI update needed.
          Q_UNUSED(manager);
      });
    return true;
}
bool
TimelineViewManager::redactActiveMatrixTimelineEvents(const QStringList &eventIds,
                                                      const QString &reason)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to redact matrix-sdk room events without an active runtime "
          "handle or selected matrix room");
        return false;
    }

    const auto roomId        = activeMatrixTimelineRoomId_;
    const auto trimmedReason = reason.trimmed();

    QStringList trimmedIds;
    trimmedIds.reserve(eventIds.size());
    for (const auto &rawId : eventIds) {
        const auto eid = rawId.trimmed();
        if (!eid.isEmpty())
            trimmedIds.append(eid);
    }

    if (trimmedIds.isEmpty())
        return false;

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedReason, trimmedIds]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          int failCount      = 0;
          QString lastError;

          for (const auto &eventId : trimmedIds) {
              QString error;
              const bool ok = komai::MatrixBackendRuntimeService::redactRoomEvent(
                                context, handleId, roomId, eventId, trimmedReason, &error)
                                .has_value();

              if (!ok) {
                  ++failCount;
                  lastError = error;
                  komai::logging::ui()->warn(
                    "Failed to redact matrix-sdk room event '{}' in '{}' on handle {}: {}",
                    eventId.toStdString(),
                    roomId.toStdString(),
                    handleId,
                    error.toStdString());
              }
          }

          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedIds.join(QStringLiteral(",")),
            .detail   = {},
            .error    = lastError,
            .ok       = failCount == 0,
          };
      },
      [trimmedIds](TimelineViewManager *manager, MatrixTimelineEventActionResult result) {
          if (manager->activeMatrixTimelineRoomId_ == result.roomId &&
              manager->matrixTimelineModel_) {
              for (const auto &eventId : trimmedIds)
                  manager->matrixTimelineModel_->redactItemByEventId(eventId);
          }

          if (result.ok)
              return;

          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to delete some messages: %1").arg(result.error));
      });
    return true;
}
bool
TimelineViewManager::redactActiveMatrixTimelineEventsByUser(const QString &userId,
                                                            const QString &reason)
{
    if (!matrixTimelineModel_ || userId.isEmpty())
        return false;

    QStringList eventIds;
    const auto items = matrixTimelineModel_->visibleItemsSnapshot();
    for (const auto &item : items) {
        if (item.senderId == userId && !item.eventId.isEmpty())
            eventIds.append(item.eventId);
    }

    if (eventIds.isEmpty())
        return false;

    return redactActiveMatrixTimelineEvents(eventIds, reason);
}
bool
TimelineViewManager::markActiveMatrixTimelineEventAsRead(const QString &eventId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to mark a matrix-sdk room event as read without an active "
          "runtime handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    // Honour the per-room override before falling back to the global toggle,
    // so rooms with `Off` (or `On`) under Room Info → Preferences win over
    // the user's default.  When the toggle is off we still send a receipt,
    // but as `m.read.private` — the homeserver clears this user's unread
    // count without broadcasting the receipt over /sync or federating it
    // to other users.
    const bool publicReceipt =
      UserSettings::instance()->resolvedTimelineReadReceiptsEnabled(activeMatrixTimelineRoomId_);

    queueMatrixRoomReadMarker(handleId, activeMatrixTimelineRoomId_, trimmedEventId, publicReceipt);

    return true;
}
void
TimelineViewManager::queueMatrixRoomReadMarker(uint64_t handleId,
                                               const QString &roomId,
                                               const QString &eventId,
                                               bool publicReceipt)
{
    if (handleId == 0 || roomId.isEmpty() || eventId.isEmpty())
        return;

    const auto inflightIt = matrixReadMarkerInFlightEventIdsByRoom_.constFind(roomId);
    if (inflightIt != matrixReadMarkerInFlightEventIdsByRoom_.cend() &&
        inflightIt.value() == eventId) {
        return;
    }

    const auto pendingIt = matrixReadMarkerPendingEventIdsByRoom_.constFind(roomId);
    if (pendingIt != matrixReadMarkerPendingEventIdsByRoom_.cend() && pendingIt.value() == eventId)
        return;

    matrixReadMarkerPendingHandlesByRoom_.insert(roomId, handleId);
    matrixReadMarkerPendingEventIdsByRoom_.insert(roomId, eventId);
    matrixReadMarkerPendingPublicByRoom_.insert(roomId, publicReceipt);
    dispatchPendingMatrixReadMarker(roomId);
}
void
TimelineViewManager::dispatchPendingMatrixReadMarker(const QString &roomId)
{
    if (roomId.isEmpty() || matrixReadMarkerInFlightEventIdsByRoom_.contains(roomId) ||
        !matrixReadMarkerPendingEventIdsByRoom_.contains(roomId) ||
        !matrixReadMarkerPendingHandlesByRoom_.contains(roomId)) {
        return;
    }

    const auto handleId      = matrixReadMarkerPendingHandlesByRoom_.take(roomId);
    const auto eventId       = matrixReadMarkerPendingEventIdsByRoom_.take(roomId);
    const bool publicReceipt = matrixReadMarkerPendingPublicByRoom_.take(roomId);
    if (handleId == 0 || eventId.isEmpty())
        return;

    matrixReadMarkerInFlightEventIdsByRoom_.insert(roomId, eventId);

    QPointer<TimelineViewManager> guard(this);
    std::thread([guard, handleId, roomId, eventId, publicReceipt]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const bool ok = komai::MatrixBackendRuntimeService::markRoomEventAsRead(
          context, handleId, roomId, eventId, publicReceipt, &error);

        if (!guard)
            return;

        QMetaObject::invokeMethod(
          guard,
          [guard, handleId, roomId, eventId, ok, error]() {
              if (!guard)
                  return;

              const auto inflightIt =
                guard->matrixReadMarkerInFlightEventIdsByRoom_.constFind(roomId);
              const bool isCurrentInflight =
                inflightIt != guard->matrixReadMarkerInFlightEventIdsByRoom_.cend() &&
                inflightIt.value() == eventId;
              if (isCurrentInflight)
                  guard->matrixReadMarkerInFlightEventIdsByRoom_.remove(roomId);

              if (!ok) {
                  komai::logging::ui()->warn(
                    "Failed to mark matrix-sdk room event '{}' as read in '{}' on handle {}: {}",
                    eventId.toStdString(),
                    roomId.toStdString(),
                    handleId,
                    error.toStdString());

                  if (guard->activeMatrixTimelineRoomId_ == roomId) {
                      if (auto *mainWindow = MainWindow::instance()) {
                          mainWindow->showNotification(
                            TimelineViewManager::tr("Failed to mark message as read: %1")
                              .arg(error));
                      }
                  }
              }

              guard->dispatchPendingMatrixReadMarker(roomId);
          },
          Qt::QueuedConnection);
    }).detach();
}
void
TimelineViewManager::clearMatrixReadMarkerQueue()
{
    matrixReadMarkerPendingHandlesByRoom_.clear();
    matrixReadMarkerPendingEventIdsByRoom_.clear();
    matrixReadMarkerPendingPublicByRoom_.clear();
    matrixReadMarkerInFlightEventIdsByRoom_.clear();
}
bool
TimelineViewManager::reportActiveMatrixTimelineEvent(const QString &eventId, const QString &reason)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to report a matrix-sdk room event without an active runtime "
          "handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId, reason]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::reportRoomEvent(
            context, handleId, roomId, trimmedEventId, reason, &error);
          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedEventId,
            .detail   = {},
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (result.ok) {
              mainWindow->showNotification(TimelineViewManager::tr("Report sent"));
              return;
          }

          komai::logging::ui()->warn(
            "Failed to report matrix-sdk room event '{}' in '{}' on handle {}: {}",
            result.eventId.toStdString(),
            result.roomId.toStdString(),
            result.handleId,
            result.error.toStdString());
          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to report message: %1").arg(result.error));
      });
    return true;
}
bool
TimelineViewManager::pinActiveMatrixTimelineEvent(const QString &eventId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to pin a matrix-sdk room event without an active runtime "
          "handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::pinRoomEvent(
            context, handleId, roomId, trimmedEventId, &error);
          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedEventId,
            .detail   = {},
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *manager, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (!result.ok) {
              komai::logging::ui()->warn(
                "Failed to pin matrix-sdk room event '{}' in '{}' on handle {}: {}",
                result.eventId.toStdString(),
                result.roomId.toStdString(),
                result.handleId,
                result.error.toStdString());
              mainWindow->showNotification(
                TimelineViewManager::tr("Failed to pin message: %1").arg(result.error));
              return;
          }

          if (manager->activeMatrixTimelineRoomId_ != result.roomId)
              return;

          if (!manager->matrixTimelinePinnedEventIds_.contains(result.eventId)) {
              manager->matrixTimelinePinnedEventIds_.push_back(result.eventId);
              emit manager->matrixTimelineStateChanged();
          }
      });
    return true;
}
bool
TimelineViewManager::unpinActiveMatrixTimelineEvent(const QString &eventId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to unpin a matrix-sdk room event without an active runtime "
          "handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::unpinRoomEvent(
            context, handleId, roomId, trimmedEventId, &error);
          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedEventId,
            .detail   = {},
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *manager, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (!result.ok) {
              komai::logging::ui()->warn(
                "Failed to unpin matrix-sdk room event '{}' in '{}' on handle {}: {}",
                result.eventId.toStdString(),
                result.roomId.toStdString(),
                result.handleId,
                result.error.toStdString());
              mainWindow->showNotification(
                TimelineViewManager::tr("Failed to unpin message: %1").arg(result.error));
              return;
          }

          if (manager->activeMatrixTimelineRoomId_ != result.roomId)
              return;

          if (manager->matrixTimelinePinnedEventIds_.removeAll(result.eventId) > 0)
              emit manager->matrixTimelineStateChanged();
      });
    return true;
}
bool
TimelineViewManager::requestRawMessageDialogForActiveMatrixTimelineEvent(const QString &eventId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty())
        return false;

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    const auto roomId    = activeMatrixTimelineRoomId_;
    const auto themeSlug = UserSettings::instance()->uiThemeSlug();

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const auto dialogData =
            komai::MatrixBackendRuntimeService::fetchActiveRoomRawEventDialogData(
              context, handleId, roomId, trimmedEventId, &error);
          return MatrixTimelineRawMessageFetchResult{
            .handleId             = handleId,
            .roomId               = roomId,
            .eventId              = trimmedEventId,
            .cleartextJson        = dialogData ? dialogData->cleartextJson : QString(),
            .cleartextError       = dialogData ? dialogData->cleartextError : QString(),
            .wireJson             = dialogData ? dialogData->wireJson : QString(),
            .wireError            = dialogData ? dialogData->wireError : QString(),
            .wireMatchesCleartext = dialogData ? dialogData->wireMatchesCleartext : false,
            .body                 = dialogData ? dialogData->body : QString(),
            .formattedBody        = dialogData ? dialogData->formattedBody : QString(),
            .error                = error,
            .ok                   = dialogData.has_value(),
          };
      },
      [themeSlug](TimelineViewManager *manager, MatrixTimelineRawMessageFetchResult result) {
          QVariantMap dialogData;

          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (!result.ok) {
              komai::logging::ui()->warn(
                "Failed to fetch raw JSON for matrix-sdk room event '{}' in '{}' on handle {}: {}",
                result.eventId.toStdString(),
                result.roomId.toStdString(),
                result.handleId,
                result.error.toStdString());
              emit manager->activeMatrixTimelineRawMessageDialogReady(result.eventId, dialogData);
              return;
          }

          const auto timelinePalette = Theme::paletteFromTheme(themeSlug);
          // Pre-render syntax-highlighted HTML for both segments. The dialog
          // only reads from these two fields when it's ready to display; the
          // raw JSON strings are kept around for the "Copy" buttons.
          const auto renderedCleartext = result.cleartextJson.isEmpty()
                                           ? QString()
                                           : timeline::formattedcode::formatRawJsonForDialog(
                                               result.cleartextJson, timelinePalette);
          const auto renderedWire =
            result.wireJson.isEmpty()
              ? QString()
              : timeline::formattedcode::formatRawJsonForDialog(result.wireJson, timelinePalette);

          dialogData.insert(QStringLiteral("cleartextRendered"), renderedCleartext);
          dialogData.insert(QStringLiteral("cleartextJson"), result.cleartextJson);
          dialogData.insert(QStringLiteral("cleartextError"), result.cleartextError);
          dialogData.insert(QStringLiteral("wireRendered"), renderedWire);
          dialogData.insert(QStringLiteral("wireJson"), result.wireJson);
          dialogData.insert(QStringLiteral("wireError"), result.wireError);
          dialogData.insert(QStringLiteral("wireMatchesCleartext"), result.wireMatchesCleartext);
          dialogData.insert(QStringLiteral("rawMessageBody"), result.body);
          dialogData.insert(QStringLiteral("rawMessageFormattedBody"), result.formattedBody);

          emit manager->activeMatrixTimelineRawMessageDialogReady(result.eventId, dialogData);
      });

    return true;
}
bool
TimelineViewManager::requestReadReceiptsModelForActiveMatrixTimelineEvent(const QString &eventId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty())
        return false;

    const auto trimmedEventId = eventId.trimmed();
    if (trimmedEventId.isEmpty())
        return false;

    const auto roomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, trimmedEventId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const auto receipts = komai::MatrixBackendRuntimeService::fetchRoomReadReceipts(
            context, handleId, roomId, trimmedEventId, &error);
          return MatrixTimelineReadReceiptsFetchResult{
            .handleId = handleId,
            .roomId   = roomId,
            .eventId  = trimmedEventId,
            .receipts = receipts.value_or(QVector<komai::MatrixReadReceiptEntry>{}),
            .error    = error,
            .ok       = receipts.has_value(),
          };
      },
      [](TimelineViewManager *manager, MatrixTimelineReadReceiptsFetchResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          QObject *model = nullptr;
          if (!result.ok) {
              komai::logging::ui()->warn(
                "Failed to fetch matrix-sdk room read receipts for event '{}' in "
                "'{}' on handle {}: {}",
                result.eventId.toStdString(),
                result.roomId.toStdString(),
                result.handleId,
                result.error.toStdString());
              emit manager->activeMatrixTimelineReadReceiptsReady(result.eventId, model);
              return;
          }

          QVector<ReadReceiptEntry> entries;
          entries.reserve(result.receipts.size());
          for (const auto &entry : result.receipts) {
              entries.push_back(ReadReceiptEntry{
                .mxid         = entry.userId,
                .displayName  = entry.displayName,
                .avatarUrl    = entry.avatarUrl,
                .rawTimestamp = entry.timestamp == 0 ? QDateTime{}
                                                     : QDateTime::fromMSecsSinceEpoch(
                                                         static_cast<qint64>(entry.timestamp)),
              });
          }

          model = new ReadReceiptsProxy{std::move(entries), result.roomId};
          QQmlEngine::setObjectOwnership(model, QQmlEngine::JavaScriptOwnership);
          emit manager->activeMatrixTimelineReadReceiptsReady(result.eventId, model);
      });

    return true;
}
