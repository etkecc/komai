// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineMediaController.h"

#include <utility>

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QMimeDatabase>
#include <QObject>
#include <QStandardPaths>
#include <QTimer>

#include "chat/ChatPage.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "profile/Paths.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/TimelineEventTypes.h"
#include "ui/MediaProxyServer.h"
#include "utils/Utils.h"

timeline::media::TimelineMediaController::TimelineMediaController(QString roomId,
                                                                  EventStore &events,
                                                                  MediaCachedCallback mediaCached)
  : roomId_(std::move(roomId))
  , events_(events)
  , mediaCached_(std::move(mediaCached))
{
}

void
timeline::media::TimelineMediaController::openMedia(const QString &eventId) const
{
    const auto roomIdStd  = roomId_.toStdString();
    const auto eventIdStd = eventId.toStdString();
    nhlog::ui()->info("Open media requested (room='{}', event='{}')", roomIdStd, eventIdStd);

    // For unencrypted video/audio, open via the local media proxy so external
    // players can stream with auth handled transparently.
    auto event = events_.get(eventIdStd, "");
    if (event && !mtx::accessors::file(*event).has_value()) {
        QString mimeType = QString::fromStdString(mtx::accessors::mimetype(*event));
        if (mimeType.startsWith(QLatin1String("video/")) ||
            mimeType.startsWith(QLatin1String("audio/"))) {
            auto *proxy = MediaProxyServer::instance();
            if (proxy->port() > 0) {
                QString mxcUrl = QString::fromStdString(mtx::accessors::url(*event));
                if (mxcUrl.startsWith(QLatin1String("mxc://"))) {
                    nhlog::ui()->info(
                      "Opening media via proxy (room='{}', event='{}')", roomIdStd, eventIdStd);
                    if (proxy->openInExternalPlayer(mxcUrl, mimeType, roomId_))
                        return;
                    // Range not supported upstream — fall through to download-to-cache.
                    nhlog::ui()->info(
                      "Proxy streaming not available, downloading to cache (room='{}', event='{}')",
                      roomIdStd,
                      eventIdStd);
                }
            }
        }
    }

    // Fallback: download to cache, then open the local file.
    cacheMedia(eventId, [roomIdStd, eventIdStd](const QString &filename) {
        QTimer::singleShot(0, ChatPage::instance(), [roomIdStd, eventIdStd, filename] {
            const auto filePathStd = filename.toStdString();
            const auto opened      = QDesktopServices::openUrl(QUrl::fromLocalFile(filename));
            if (!opened) {
                nhlog::ui()->warn(
                  "Failed to open media externally (room='{}', event='{}', file='{}')",
                  roomIdStd,
                  eventIdStd,
                  filePathStd);
                return;
            }

            nhlog::ui()->info("Opened media externally (room='{}', event='{}', file='{}')",
                              roomIdStd,
                              eventIdStd,
                              filePathStd);
        });
    });
}

bool
timeline::media::TimelineMediaController::saveMedia(const QString &eventId) const
{
    auto event = events_.get(eventId.toStdString(), "");
    if (!event)
        return false;

    QString mxcUrl           = QString::fromStdString(mtx::accessors::url(*event));
    QString originalFilename = QString::fromStdString(mtx::accessors::filename(*event));
    QString mimeType         = QString::fromStdString(mtx::accessors::mimetype(*event));

    auto encryptionInfo = mtx::accessors::file(*event);

    qml_mtx_events::EventType eventType = qml_mtx_events::toRoomEventType(*event);

    QString dialogTitle;
    if (eventType == qml_mtx_events::EventType::ImageMessage) {
        dialogTitle = QObject::tr("Save image");
    } else if (eventType == qml_mtx_events::EventType::VideoMessage) {
        dialogTitle = QObject::tr("Save video");
    } else if (eventType == qml_mtx_events::EventType::AudioMessage) {
        dialogTitle = QObject::tr("Save audio");
    } else {
        dialogTitle = QObject::tr("Save file");
    }

    const QString filterString = QMimeDatabase().mimeTypeForName(mimeType).filterString();
    const QString downloadsFolder =
      QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QString openLocation = downloadsFolder + "/" + originalFilename;

    const QString filename =
      QFileDialog::getSaveFileName(nullptr, dialogTitle, openLocation, filterString);

    if (filename.isEmpty())
        return false;

    const auto url = mxcUrl.toStdString();

    http::client()->download(url,
                             [filename, url, encryptionInfo](const std::string &data,
                                                             const std::string &,
                                                             const std::string &,
                                                             mtx::http::RequestErr err) {
                                 if (err) {
                                     nhlog::net()->warn("failed to retrieve image {}: {} {}",
                                                        url,
                                                        err->matrix_error.error,
                                                        static_cast<int>(err->status_code));
                                     return;
                                 }

                                 try {
                                     auto temp = data;
                                     if (encryptionInfo)
                                         temp = mtx::crypto::to_string(
                                           mtx::crypto::decrypt_file(temp, encryptionInfo.value()));

                                     QFile file(filename);

                                     if (!file.open(QIODevice::WriteOnly))
                                         return;

                                     file.write(QByteArray(temp.data(), (int)temp.size()));
                                     file.close();
                                     utils::markFileAsFromWeb(filename);

                                     return;
                                 } catch (const std::exception &e) {
                                     nhlog::ui()->warn("Error while saving file to: {}", e.what());
                                 }
                             });
    return true;
}

bool
timeline::media::TimelineMediaController::copyMedia(const QString &eventId) const
{
    auto event = events_.get(eventId.toStdString(), "");
    if (!event)
        return false;

    QString mxcUrl                      = QString::fromStdString(mtx::accessors::url(*event));
    QString mimeType                    = QString::fromStdString(mtx::accessors::mimetype(*event));
    qml_mtx_events::EventType eventType = qml_mtx_events::toRoomEventType(*event);

    auto encryptionInfo = mtx::accessors::file(*event);

    const auto url = mxcUrl.toStdString();

    http::client()->download(
      url,
      [url, mimeType, eventType, encryptionInfo](const std::string &data,
                                                 const std::string &,
                                                 const std::string &,
                                                 mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->warn("failed to retrieve media {}: {} {}",
                                 url,
                                 err->matrix_error.error,
                                 static_cast<int>(err->status_code));
              return;
          }

          try {
              auto temp = data;
              if (encryptionInfo)
                  temp =
                    mtx::crypto::to_string(mtx::crypto::decrypt_file(temp, encryptionInfo.value()));

              auto by                 = QByteArray(temp.data(), (qsizetype)temp.size());
              QMimeData *clipContents = new QMimeData();
              clipContents->setData(mimeType, by);

              if (eventType == qml_mtx_events::EventType::ImageMessage) {
                  auto img = utils::readImage(QByteArray(data.data(), (qsizetype)data.size()));
                  clipContents->setImageData(img);
              }

              // Qt uses COM for clipboard management on windows and our HTTP threads do not
              // initialize it, so run in the event loop
              QTimer::singleShot(0, ChatPage::instance(), [clipContents] {
                  QGuiApplication::clipboard()->setMimeData(clipContents);
              });

              return;
          } catch (const std::exception &e) {
              nhlog::ui()->warn("Error while copying file to clipboard: {}", e.what());
          }
      });
    return true;
}

void
timeline::media::TimelineMediaController::cacheMedia(
  const QString &eventId,
  const std::function<void(const QString &)> &callback) const
{
    auto event = events_.get(eventId.toStdString(), "");
    if (!event) {
        nhlog::ui()->warn("cacheMedia failed: event not found (room='{}', event='{}')",
                          roomId_.toStdString(),
                          eventId.toStdString());
        return;
    }

    QString mxcUrl   = QString::fromStdString(mtx::accessors::url(*event));
    QString mimeType = QString::fromStdString(mtx::accessors::mimetype(*event));

    auto encryptionInfo = mtx::accessors::file(*event);

    // If the message is a link to a non mxcUrl, don't download it
    if (!mxcUrl.startsWith(QLatin1String("mxc://"))) {
        if (mediaCached_)
            mediaCached_(mxcUrl, mxcUrl);
        return;
    }

    QString suffix = QMimeDatabase().mimeTypeForName(mimeType).preferredSuffix();

    const auto url       = mxcUrl.toStdString();
    const auto roomIdStd = roomId_.toStdString();
    const auto name      = QString(mxcUrl).remove(QStringLiteral("mxc://"));
    QFileInfo filename(app_paths::cache::mediaFileForMxc(
      UserSettings::instance()->profile(), name, suffix, roomId_));
    if (QDir::cleanPath(filename.filePath()) != filename.filePath()) {
        nhlog::net()->warn("mxcUrl '{}' is not safe, not downloading file", url);
        return;
    }

    QDir().mkpath(filename.path());

    if (filename.isReadable()) {
        nhlog::ui()->info("cacheMedia hit (room='{}', event='{}', file='{}')",
                          roomIdStd,
                          eventId.toStdString(),
                          filename.filePath().toStdString());
        if (mediaCached_) {
#if defined(Q_OS_WIN)
            mediaCached_(mxcUrl, filename.filePath());
#else
            mediaCached_(mxcUrl, "file://" + filename.filePath());
#endif
        }
        if (callback) {
            callback(filename.filePath());
        }
        return;
    }

    nhlog::ui()->info("cacheMedia miss, downloading (room='{}', event='{}', mxc='{}')",
                      roomIdStd,
                      eventId.toStdString(),
                      mxcUrl.toStdString());

    const auto mediaCached = mediaCached_;
    http::client()->download(
      url,
      [callback, mediaCached, mxcUrl, filename, url, encryptionInfo](const std::string &data,
                                                                     const std::string &,
                                                                     const std::string &,
                                                                     mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->warn("failed to retrieve image {}: {} {}",
                                 url,
                                 err->matrix_error.error,
                                 static_cast<int>(err->status_code));
              return;
          }

          try {
              auto temp = data;
              if (encryptionInfo)
                  temp =
                    mtx::crypto::to_string(mtx::crypto::decrypt_file(temp, encryptionInfo.value()));

              QFile file(filename.filePath());

              if (!file.open(QIODevice::WriteOnly))
                  return;

              file.write(QByteArray(temp.data(), (int)temp.size()));
              file.close();

              if (callback) {
                  nhlog::ui()->info("cacheMedia downloaded (mxc='{}', file='{}')",
                                    mxcUrl.toStdString(),
                                    filename.filePath().toStdString());
                  callback(filename.filePath());
              }
          } catch (const std::exception &e) {
              nhlog::ui()->warn("Error while saving file to: {}", e.what());
          }

          if (mediaCached) {
#if defined(Q_OS_WIN)
              mediaCached(mxcUrl, filename.filePath());
#else
              mediaCached(mxcUrl, "file://" + filename.filePath());
#endif
          }
      });
}
