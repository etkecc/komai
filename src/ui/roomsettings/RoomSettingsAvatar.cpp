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

#include <utility>

#include <mtx/responses/media.hpp>

#include "matrix/MatrixClient.h"

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

    isLoading_ = true;
    emit loadingChanged();

    // Events emitted from the http callbacks (different threads) will
    // be queued back into the UI thread through this proxy object.
    auto proxy = std::make_shared<ThreadProxy>();
    connect(proxy.get(), &ThreadProxy::error, this, &RoomSettings::displayError);
    connect(proxy.get(), &ThreadProxy::stopLoading, this, &RoomSettings::stopLoading);

    const auto bin        = file.peek(file.size());
    const auto payload    = std::string(bin.data(), bin.size());
    const auto dimensions = QImageReader(&file).size();

    // First we need to create a new mxc URI
    // (i.e upload media to the Matrix content repository) for the new avatar.
    http::client()->upload(
      payload,
      mime.name().toStdString(),
      QFileInfo(fileName).fileName().toStdString(),
      [proxy = std::move(proxy),
       dimensions,
       payload,
       mimetype = mime.name().toStdString(),
       size     = payload.size(),
       room_id  = roomid_.toStdString(),
       content = std::move(bin)](const mtx::responses::ContentURI &res, mtx::http::RequestErr err) {
          if (err) {
              emit proxy->stopLoading();
              emit proxy->error(tr("Failed to upload image: %s")
                                  .arg(QString::fromStdString(err->matrix_error.error)));
              return;
          }

          using namespace mtx::events;
          state::Avatar avatar_event;
          avatar_event.image_info.w        = dimensions.width();
          avatar_event.image_info.h        = dimensions.height();
          avatar_event.image_info.mimetype = mimetype;
          avatar_event.image_info.size     = size;
          avatar_event.url                 = res.content_uri;

          http::client()->send_state_event(
            room_id,
            avatar_event,
            [content = std::move(content),
             proxy = std::move(proxy)](const mtx::responses::EventId &, mtx::http::RequestErr err) {
                if (err) {
                    emit proxy->error(tr("Failed to upload image: %s")
                                        .arg(QString::fromStdString(err->matrix_error.error)));
                    return;
                }

                emit proxy->stopLoading();
            });
      });
}

void
RoomSettings::removeAvatar()
{
    isLoading_ = true;
    emit loadingChanged();

    auto proxy = std::make_shared<ThreadProxy>();
    connect(proxy.get(), &ThreadProxy::error, this, &RoomSettings::displayError);
    connect(proxy.get(), &ThreadProxy::stopLoading, this, &RoomSettings::stopLoading);

    using namespace mtx::events;
    state::Avatar avatar_event;
    avatar_event.url = "";

    http::client()->send_state_event(
      roomid_.toStdString(),
      avatar_event,
      [proxy](const mtx::responses::EventId &, mtx::http::RequestErr err) {
          if (err) {
              emit proxy->error(tr("Failed to remove avatar: %1")
                                  .arg(QString::fromStdString(err->matrix_error.error)));
              return;
          }

          emit proxy->stopLoading();
      });
}
