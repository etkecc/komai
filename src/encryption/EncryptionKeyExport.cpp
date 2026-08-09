// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EncryptionKeyExport.h"

#include <optional>

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"

namespace {

QString
localPathFromUrl(const QUrl &file)
{
    if (file.isLocalFile())
        return file.toLocalFile();
    // Qt.labs.platform's FileDialog always yields file:// URLs, but accept a
    // plain path defensively.
    return file.toString();
}

uint64_t
currentHandleId()
{
    const auto *mainWindow = MainWindow::instance();
    return mainWindow ? mainWindow->matrixBackendHandleId() : 0;
}

}

EncryptionKeyExport::EncryptionKeyExport(QObject *parent)
  : QObject(parent)
{
}

EncryptionKeyExport *
EncryptionKeyExport::instance()
{
    static auto *instance_ = new EncryptionKeyExport();
    return instance_;
}

void
EncryptionKeyExport::setBusy(bool busy)
{
    if (busy_ == busy)
        return;
    busy_ = busy;
    emit busyChanged();
}

void
EncryptionKeyExport::exportKeys(const QUrl &file, const QString &passphrase)
{
    const auto handleId = currentHandleId();
    if (busy_ || handleId == 0)
        return;

    setBusy(true);

    const auto path = localPathFromUrl(file);

    struct TaskResult
    {
        std::optional<uint64_t> count;
        QString error;
    };

    komai::qt_worker_task::runQueued(
      this,
      [handleId, path, passphrase]() {
          TaskResult taskResult;
          const auto context = komai::matrix_backend::blockingCallContext();
          taskResult.count   = komai::MatrixBackendRuntimeService::exportRoomKeys(
            context, handleId, path, passphrase, &taskResult.error);
          return taskResult;
      },
      [](EncryptionKeyExport *self, const TaskResult &taskResult) {
          self->setBusy(false);
          if (!taskResult.count) {
              komai::logging::crypto()->error("Room key export failed: {}",
                                              taskResult.error.toStdString());
              emit self->exportFailed(taskResult.error);
              return;
          }
          komai::logging::crypto()->info("Exported {} room keys", *taskResult.count);
          emit self->exportCompleted(static_cast<int>(*taskResult.count));
      });
}

void
EncryptionKeyExport::importKeys(const QUrl &file, const QString &passphrase)
{
    const auto handleId = currentHandleId();
    if (busy_ || handleId == 0)
        return;

    setBusy(true);

    const auto path = localPathFromUrl(file);

    struct TaskResult
    {
        std::optional<komai::MatrixRoomKeyImportCounts> counts;
        QString error;
    };

    komai::qt_worker_task::runQueued(
      this,
      [handleId, path, passphrase]() {
          TaskResult taskResult;
          const auto context = komai::matrix_backend::blockingCallContext();
          taskResult.counts  = komai::MatrixBackendRuntimeService::importRoomKeys(
            context, handleId, path, passphrase, &taskResult.error);
          return taskResult;
      },
      [](EncryptionKeyExport *self, const TaskResult &taskResult) {
          self->setBusy(false);
          if (!taskResult.counts) {
              komai::logging::crypto()->error("Room key import failed: {}",
                                              taskResult.error.toStdString());
              emit self->importFailed(taskResult.error);
              return;
          }
          komai::logging::crypto()->info("Imported {} of {} room keys",
                                         taskResult.counts->imported,
                                         taskResult.counts->total);
          emit self->importCompleted(static_cast<int>(taskResult.counts->imported),
                                     static_cast<int>(taskResult.counts->total));
      });
}

#include "moc_EncryptionKeyExport.cpp"
