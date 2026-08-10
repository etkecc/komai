// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatExport.h"

#include <optional>

#include <QDateTime>
#include <QFileInfo>
#include <QPointer>
#include <QSaveFile>

#include "export/ChatExportFormatter.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "ui/NotificationAction.h"
#include "utils/QtWorkerTask.h"
#include "utils/Utils.h"

namespace {

constexpr uint32_t kExportBatchSize = 200;

QString
localPathFromUrl(const QUrl &file)
{
    if (file.isLocalFile())
        return file.toLocalFile();
    return file.toString();
}

uint64_t
currentHandleId()
{
    const auto *mainWindow = MainWindow::instance();
    return mainWindow ? mainWindow->matrixBackendHandleId() : 0;
}

QString
currentUserId()
{
    const auto settings = UserSettings::instance();
    return settings ? settings->sessionSnapshot().userId : QString{};
}

}

ChatExport::ChatExport(QObject *parent)
  : QObject(parent)
{
}

ChatExport *
ChatExport::instance()
{
    static auto *instance_ = new ChatExport();
    return instance_;
}

std::shared_ptr<ChatExport::ExportJob>
ChatExport::jobForRoom(const QString &roomId) const
{
    const std::lock_guard<std::mutex> lock(jobsMutex_);
    return jobs_.value(roomId);
}

bool
ChatExport::isExporting(const QString &roomId) const
{
    return jobForRoom(roomId) != nullptr;
}

int
ChatExport::fetchedCount(const QString &roomId) const
{
    const auto job = jobForRoom(roomId);
    return job ? job->fetched.load() : 0;
}

void
ChatExport::cancel(const QString &roomId)
{
    if (const auto job = jobForRoom(roomId))
        job->cancelRequested.store(true);
}

void
ChatExport::startExport(const QString &roomId,
                        const QString &roomName,
                        const QUrl &file,
                        Format format,
                        bool includeMetadata)
{
    const auto handleId = currentHandleId();
    if (handleId == 0 || roomId.isEmpty())
        return;

    std::shared_ptr<ExportJob> job;
    {
        const std::lock_guard<std::mutex> lock(jobsMutex_);
        // One job per room at a time; exports for other rooms run
        // concurrently and independently.
        if (jobs_.contains(roomId))
            return;
        job = std::make_shared<ExportJob>();
        jobs_.insert(roomId, job);
    }
    emit exportStarted(roomId);

    const auto path = localPathFromUrl(file);

    komai::chat_export::RenderInput renderInput;
    renderInput.roomName         = roomName;
    renderInput.roomId           = roomId;
    renderInput.exportingUserId  = currentUserId();
    renderInput.exportedAt       = QDateTime::currentDateTime();
    renderInput.includeMetadata  = includeMetadata;
    renderInput.htmlBodyPipeline = [](const QString &html) {
        return utils::linkifyMessage(utils::escapeBlacklistedHtml(html));
    };

    const auto renderFormat = [format]() {
        switch (format) {
        case Format::Html:
            return komai::chat_export::Format::Html;
        case Format::JsonLines:
            return komai::chat_export::Format::JsonLines;
        case Format::PlainText:
            break;
        }
        return komai::chat_export::Format::PlainText;
    }();

    struct TaskResult
    {
        std::optional<komai::chat_export::RenderResult> render;
        bool cancelled = false;
        QString error;
    };

    QPointer<ChatExport> self(this);

    komai::qt_worker_task::runQueued(
      this,
      [self, job, handleId, roomId, path, renderInput, renderFormat]() {
          TaskResult taskResult;
          const auto context = komai::matrix_backend::blockingCallContext();

          QList<komai::MatrixChatExportEvent> events;
          QString fromToken;
          while (true) {
              if (job->cancelRequested.load()) {
                  taskResult.cancelled = true;
                  return taskResult;
              }

              QString error;
              const auto batch = komai::MatrixBackendRuntimeService::fetchChatExportBatch(
                context, handleId, roomId, fromToken, kExportBatchSize, &error);
              if (!batch) {
                  taskResult.error = error;
                  return taskResult;
              }

              events.append(batch->events);
              const auto fetched = static_cast<int>(events.size());
              job->fetched.store(fetched);
              QMetaObject::invokeMethod(
                self,
                [self, roomId, fetched]() {
                    if (self)
                        emit self->progressChanged(roomId, fetched);
                },
                Qt::QueuedConnection);

              if (batch->reachedStart)
                  break;
              if (batch->nextToken.isEmpty() || batch->nextToken == fromToken) {
                  // Defensive: a server that neither signals the history
                  // start nor advances the token would loop forever.
                  komai::logging::ui()->warn(
                    "Chat export pagination stalled for room {}; treating as complete",
                    roomId.toStdString());
                  break;
              }
              fromToken = batch->nextToken;
          }

          if (job->cancelRequested.load()) {
              taskResult.cancelled = true;
              return taskResult;
          }

          const auto rendered = komai::chat_export::render(events, renderInput, renderFormat);

          QSaveFile out(path);
          if (!out.open(QIODevice::WriteOnly)) {
              taskResult.error = out.errorString();
              return taskResult;
          }
          out.write(rendered.document.toUtf8());
          if (!out.commit()) {
              taskResult.error = out.errorString();
              return taskResult;
          }

          taskResult.render = rendered;
          return taskResult;
      },
      [roomId, roomName, path](ChatExport *self, const TaskResult &taskResult) {
          {
              const std::lock_guard<std::mutex> lock(self->jobsMutex_);
              self->jobs_.remove(roomId);
          }
          if (taskResult.cancelled) {
              komai::logging::ui()->info("Chat export cancelled for room {}", roomId.toStdString());
              emit self->exportCancelled(roomId);
              return;
          }
          if (!taskResult.render) {
              komai::logging::ui()->error("Chat export failed for room {}: {}",
                                          roomId.toStdString(),
                                          taskResult.error.toStdString());
              emit self->exportFailed(roomId, taskResult.error);
              return;
          }
          komai::logging::ui()->info(
            "Chat export finished for room {}: {} messages, {} undecryptable",
            roomId.toStdString(),
            taskResult.render->messageCount,
            taskResult.render->utdCount);
          emit self->exportCompleted(
            roomId, taskResult.render->messageCount, taskResult.render->utdCount);

          // The same toast the timeline's "Save as" shows: it carries Open /
          // Show-in-folder actions, and reaches the user even when the Room
          // Info dialog was closed while the export kept running. With
          // concurrent exports finishing in any order, name the room.
          if (auto *mainWindow = MainWindow::instance()) {
              const QUrl fileUrl         = QUrl::fromLocalFile(path);
              const auto displayRoomName = roomName.isEmpty() ? roomId : roomName;
              const QList<komai::NotificationAction> actions{
                {komai::NotificationAction::OpenUrl,
                 tr("Open"),
                 QStringLiteral("qrc:/icons/icons/ui/open-externally.svg"),
                 fileUrl},
                {komai::NotificationAction::RevealInFolder,
                 tr("Show in folder"),
                 QStringLiteral("qrc:/icons/icons/ui/folder-open.svg"),
                 fileUrl},
              };
              emit mainWindow->showNotificationWithActions(
                tr("Chat export of '%1' saved to '%2'")
                  .arg(displayRoomName, QFileInfo(path).fileName()),
                komai::toVariantList(actions));
          }
      });
}

#include "moc_ChatExport.cpp"
