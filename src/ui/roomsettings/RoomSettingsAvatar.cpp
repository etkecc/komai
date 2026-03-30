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

    const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
    QString error;
    if (!komai::MatrixBackendRuntimeService::uploadRoomAvatar(context,
                                                              matrixBackendHandleId(),
                                                              roomid_,
                                                              fileName,
                                                              mime.name(),
                                                              dimensions.width(),
                                                              dimensions.height(),
                                                              &error)) {
        isLoading_ = false;
        emit loadingChanged();
        emit displayError(error.isEmpty() ? tr("Failed to upload image.") : error);
        return;
    }

    if (loadMatrixRuntimeRoomSettings(&error)) {
        emit avatarUrlChanged();
    } else {
        nhlog::ui()->warn("Failed to refresh room settings after avatar upload: {}",
                          error.toStdString());
    }

    isLoading_ = false;
    emit loadingChanged();
}

void
RoomSettings::removeAvatar()
{
    isLoading_ = true;
    emit loadingChanged();

    const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
    QString error;
    if (!komai::MatrixBackendRuntimeService::removeRoomAvatar(
          context, matrixBackendHandleId(), roomid_, &error)) {
        isLoading_ = false;
        emit loadingChanged();
        emit displayError(error.isEmpty() ? tr("Failed to remove avatar.") : error);
        return;
    }

    if (loadMatrixRuntimeRoomSettings(&error)) {
        emit avatarUrlChanged();
    } else {
        nhlog::ui()->warn("Failed to refresh room settings after avatar removal: {}",
                          error.toStdString());
    }

    isLoading_ = false;
    emit loadingChanged();
}
