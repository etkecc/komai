// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomSettings.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <QStandardPaths>

#include "logging/Logging.h"
#include "utils/QtWorkerTask.h"

namespace {
struct RoomAvatarMutationResult
{
    bool ok = false;
    QString actionError;
    std::optional<komai::MatrixRoomSettings> refreshedSettings;
    QString refreshError;
};
} // namespace

void
RoomSettings::stopLoading()
{
    isLoading_ = false;
    emit loadingChanged();
}

void
RoomSettings::avatarChanged()
{
    retrieveRoomInfo();
    emit avatarUrlChanged();
}

void
RoomSettings::updateAvatar()
{
    if (isLoading_)
        return;

    const QString picturesFolder =
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString fileName = QFileDialog::getOpenFileName(
      nullptr, tr("Select an avatar"), picturesFolder, tr("All Files (*)"));

    if (fileName.isEmpty())
        return;

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(fileName, QMimeDatabase::MatchContent);

    const auto format = mime.name().split(QStringLiteral("/"))[0];

    QFile file{fileName, this};
    if (format != QLatin1String("image")) {
        emit displayError(tr("The selected file is not an image"));
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        emit displayError(tr("Error while reading file: %1").arg(file.errorString()));
        return;
    }

    const auto dimensions = QImageReader(&file).size();

    isLoading_ = true;
    emit loadingChanged();

    const auto handleId = matrixBackendHandleId();
    komai::qt_worker_task::runQueued(
      this,
      [handleId,
       roomId = roomid_,
       fileName,
       mimeName = mime.name(),
       width    = dimensions.width(),
       height   = dimensions.height()]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          RoomAvatarMutationResult result;
          result.ok = komai::MatrixBackendRuntimeService::uploadRoomAvatar(
            context, handleId, roomId, fileName, mimeName, width, height, &result.actionError);
          if (!result.ok)
              return result;

          result.refreshedSettings = komai::MatrixBackendRuntimeService::fetchRoomSettings(
            context, handleId, roomId, &result.refreshError);
          return result;
      },
      [](RoomSettings *settings, const RoomAvatarMutationResult &result) {
          settings->isLoading_ = false;
          emit settings->loadingChanged();

          if (!result.ok) {
              emit settings->displayError(
                result.actionError.isEmpty() ? tr("Failed to upload image.") : result.actionError);
              return;
          }

          if (result.refreshedSettings.has_value()) {
              settings->applyMatrixRoomSettings(*result.refreshedSettings);
              return;
          }

          nhlog::ui()->warn("Failed to refresh room settings after avatar upload: {}",
                            result.refreshError.toStdString());
      });
}

void
RoomSettings::removeAvatar()
{
    if (isLoading_)
        return;

    isLoading_ = true;
    emit loadingChanged();

    const auto handleId = matrixBackendHandleId();
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId = roomid_]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          RoomAvatarMutationResult result;
          result.ok = komai::MatrixBackendRuntimeService::removeRoomAvatar(
            context, handleId, roomId, &result.actionError);
          if (!result.ok)
              return result;

          result.refreshedSettings = komai::MatrixBackendRuntimeService::fetchRoomSettings(
            context, handleId, roomId, &result.refreshError);
          return result;
      },
      [](RoomSettings *settings, const RoomAvatarMutationResult &result) {
          settings->isLoading_ = false;
          emit settings->loadingChanged();

          if (!result.ok) {
              emit settings->displayError(
                result.actionError.isEmpty() ? tr("Failed to remove avatar.") : result.actionError);
              return;
          }

          if (result.refreshedSettings.has_value()) {
              settings->applyMatrixRoomSettings(*result.refreshedSettings);
              return;
          }

          nhlog::ui()->warn("Failed to refresh room settings after avatar removal: {}",
                            result.refreshError.toStdString());
      });
}
