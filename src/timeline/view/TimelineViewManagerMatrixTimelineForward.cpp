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

bool
TimelineViewManager::forwardActiveMatrixTimelineEvent(const QString &eventId,
                                                      const QString &targetRoomId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to forward a matrix-sdk room event without an active runtime "
          "handle or selected matrix room");
        return false;
    }

    const auto trimmedEventId      = eventId.trimmed();
    const auto trimmedTargetRoomId = targetRoomId.trimmed();
    if (trimmedEventId.isEmpty() || trimmedTargetRoomId.isEmpty())
        return false;

    if (!matrixTimelineModel_) {
        komai::logging::ui()->warn(
          "Refusing to forward matrix-sdk room event '{}' without an active "
          "timeline model",
          trimmedEventId.toStdString());
        return false;
    }

    const auto item = matrixTimelineModel_->itemByEventId(trimmedEventId);
    if (!item) {
        komai::logging::ui()->warn("Refusing to forward unknown matrix-sdk room event '{}' in '{}'",
                                   trimmedEventId.toStdString(),
                                   activeMatrixTimelineRoomId_.toStdString());
        return false;
    }

    const auto itemKind = item->itemKind.trimmed().toLower();
    QString error;

    if (isForwardableActiveMatrixTimelineTextKind(itemKind)) {
        const auto sourceRoomId          = activeMatrixTimelineRoomId_;
        const auto useMarkdownFormatting = matrixMessageUsesMarkdownFormatting();
        const auto normalizedKind        = normalizedMatrixMessageKind(itemKind);
        komai::qt_worker_task::runQueued(
          this,
          [handleId,
           sourceRoomId,
           trimmedTargetRoomId,
           trimmedEventId,
           body = item->body,
           useMarkdownFormatting,
           normalizedKind]() {
              const auto context = komai::matrix_backend::blockingCallContext();
              QString error;
              const bool ok =
                komai::MatrixBackendRuntimeService::sendRoomMessage(context,
                                                                    handleId,
                                                                    trimmedTargetRoomId,
                                                                    body,
                                                                    useMarkdownFormatting,
                                                                    normalizedKind,
                                                                    komai::MatrixSendMode::Queued,
                                                                    QString(),
                                                                    false,
                                                                    &error)
                  .has_value();
              return MatrixTimelineEventActionResult{
                .handleId = handleId,
                .roomId   = sourceRoomId,
                .eventId  = trimmedEventId,
                .detail   = trimmedTargetRoomId,
                .error    = error,
                .ok       = ok,
              };
          },
          [](TimelineViewManager *, MatrixTimelineEventActionResult result) {
              auto *mainWindow = MainWindow::instance();
              if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
                  return;

              if (result.ok)
                  return;

              komai::logging::ui()->warn(
                "Failed to forward matrix-sdk room event '{}' from '{}' to '{}' "
                "on handle {}: {}",
                result.eventId.toStdString(),
                result.roomId.toStdString(),
                result.detail.toStdString(),
                result.handleId,
                result.error.toStdString());
              mainWindow->showNotification(
                TimelineViewManager::tr("Failed to forward message: %1").arg(result.error));
          });
        return true;
    }

    // For all other event types (media, stickers, etc.), forward by extracting the
    // raw event content JSON and resending it as-is.  This preserves all metadata
    // (dimensions, duration, thumbnail, blurhash, etc.) and avoids re-uploading
    // since the mxc:// URLs are already server-hosted.
    const auto sourceRoomId = activeMatrixTimelineRoomId_;
    komai::qt_worker_task::runQueued(
      this,
      [handleId, sourceRoomId, trimmedTargetRoomId, trimmedEventId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;

          auto content =
            komai::MatrixBackendRuntimeService::fetchActiveRoomEventContentForForwarding(
              context, handleId, sourceRoomId, trimmedEventId, &error);
          if (!content) {
              return MatrixTimelineEventActionResult{
                .handleId = handleId,
                .roomId   = sourceRoomId,
                .eventId  = trimmedEventId,
                .detail   = trimmedTargetRoomId,
                .error    = error,
                .ok       = false,
              };
          }

          const bool ok =
            komai::MatrixBackendRuntimeService::sendRoomMessageLikeEventJson(context,
                                                                             handleId,
                                                                             trimmedTargetRoomId,
                                                                             content->eventType,
                                                                             content->contentJson,
                                                                             &error);

          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = sourceRoomId,
            .eventId  = trimmedEventId,
            .detail   = trimmedTargetRoomId,
            .error    = error,
            .ok       = ok,
          };
      },
      [](TimelineViewManager *, MatrixTimelineEventActionResult result) {
          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          if (result.ok)
              return;

          komai::logging::ui()->warn(
            "Failed to forward matrix-sdk room event '{}' from '{}' to '{}' "
            "on handle {}: {}",
            result.eventId.toStdString(),
            result.roomId.toStdString(),
            result.detail.toStdString(),
            result.handleId,
            result.error.toStdString());
          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to forward message: %1").arg(result.error));
      });
    return true;
}
bool
TimelineViewManager::forwardActiveMatrixTimelineEvents(const QStringList &eventIds,
                                                       const QString &targetRoomId)
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || activeMatrixTimelineRoomId_.isEmpty()) {
        komai::logging::ui()->warn(
          "Refusing to forward matrix-sdk room events without an active runtime "
          "handle or selected matrix room");
        return false;
    }

    const auto trimmedTargetRoomId = targetRoomId.trimmed();
    if (trimmedTargetRoomId.isEmpty())
        return false;

    if (!matrixTimelineModel_) {
        komai::logging::ui()->warn("Refusing to forward matrix-sdk room events without an active "
                                   "timeline model");
        return false;
    }

    struct ForwardEntry
    {
        QString eventId;
        QString itemKind;
        QString body;
    };

    const auto sourceRoomId          = activeMatrixTimelineRoomId_;
    const auto useMarkdownFormatting = matrixMessageUsesMarkdownFormatting();
    QVector<ForwardEntry> entries;
    entries.reserve(eventIds.size());

    for (const auto &rawId : eventIds) {
        const auto eid = rawId.trimmed();
        if (eid.isEmpty())
            continue;

        const auto item = matrixTimelineModel_->itemByEventId(eid);
        if (!item) {
            komai::logging::ui()->warn(
              "Skipping unknown matrix-sdk room event '{}' during batch forward",
              eid.toStdString());
            continue;
        }

        entries.append(ForwardEntry{
          .eventId  = eid,
          .itemKind = item->itemKind.trimmed().toLower(),
          .body     = item->body,
        });
    }

    if (entries.isEmpty())
        return false;

    komai::qt_worker_task::runQueued(
      this,
      [handleId,
       sourceRoomId,
       trimmedTargetRoomId,
       useMarkdownFormatting,
       entries = std::move(entries)]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          int failCount      = 0;
          QString lastError;

          for (const auto &entry : entries) {
              QString error;
              bool ok = false;

              if (isForwardableActiveMatrixTimelineTextKind(entry.itemKind)) {
                  const auto normalizedKind =
                    entry.itemKind == QStringLiteral("notice")
                      ? QStringLiteral("notice")
                      : (entry.itemKind == QStringLiteral("emote") ? QStringLiteral("emote")
                                                                   : QStringLiteral("text"));
                  ok = komai::MatrixBackendRuntimeService::sendRoomMessage(
                         context,
                         handleId,
                         trimmedTargetRoomId,
                         entry.body,
                         useMarkdownFormatting,
                         normalizedKind,
                         komai::MatrixSendMode::Queued,
                         QString(),
                         false,
                         &error)
                         .has_value();
              } else {
                  auto content =
                    komai::MatrixBackendRuntimeService::fetchActiveRoomEventContentForForwarding(
                      context, handleId, sourceRoomId, entry.eventId, &error);
                  if (content) {
                      ok = komai::MatrixBackendRuntimeService::sendRoomMessageLikeEventJson(
                        context,
                        handleId,
                        trimmedTargetRoomId,
                        content->eventType,
                        content->contentJson,
                        &error);
                  }
              }

              if (!ok) {
                  ++failCount;
                  lastError = error;
                  komai::logging::ui()->warn(
                    "Failed to forward matrix-sdk room event '{}' from '{}' to '{}': {}",
                    entry.eventId.toStdString(),
                    sourceRoomId.toStdString(),
                    trimmedTargetRoomId.toStdString(),
                    error.toStdString());
              }
          }

          return MatrixTimelineEventActionResult{
            .handleId = handleId,
            .roomId   = sourceRoomId,
            .eventId  = QString::number(entries.size()),
            .detail   = trimmedTargetRoomId,
            .error    = lastError,
            .ok       = failCount == 0,
          };
      },
      [](TimelineViewManager *, MatrixTimelineEventActionResult result) {
          if (result.ok)
              return;

          auto *mainWindow = MainWindow::instance();
          if (!mainWindow || mainWindow->matrixBackendHandleId() != result.handleId)
              return;

          mainWindow->showNotification(
            TimelineViewManager::tr("Failed to forward some messages: %1").arg(result.error));
      });
    return true;
}
