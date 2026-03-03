// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineViewManager.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileDialog>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QStandardPaths>
#include <QUrl>

#include "RoomlistModel.h"
#include "TimelineModel.h"
#include "imagepacks/ImagePackListModel.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "providers/MxcImageProvider.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

void
TimelineViewManager::openImageOverlay(TimelineModel *room,
                                      const QString &mxcUrl,
                                      const QString &eventId,
                                      double originalWidth,
                                      double proportionalHeight)
{
    if (mxcUrl.isEmpty()) {
        return;
    }

    emit showImageOverlay(
      room, eventId, mxcUrl, originalWidth, proportionalHeight, nullptr, nullptr);
}

void
TimelineViewManager::openImageOverlayWithContext(TimelineModel *room,
                                                 const QString &mxcUrl,
                                                 const QString &eventId,
                                                 double originalWidth,
                                                 double proportionalHeight,
                                                 QObject *timeline,
                                                 QObject *timelineView)
{
    if (mxcUrl.isEmpty()) {
        return;
    }

    emit showImageOverlay(
      room, eventId, mxcUrl, originalWidth, proportionalHeight, timeline, timelineView);
}

void
TimelineViewManager::openImagePackSettings(QString roomid)
{
    auto room  = rooms_->getRoomById(roomid).get();
    auto model = new ImagePackListModel(roomid.toStdString());
    QQmlEngine::setObjectOwnership(model, QQmlEngine::JavaScriptOwnership);
    emit showImagePackSettings(room, model);
}

void
TimelineViewManager::saveMedia(QString mxcUrl)
{
    const QString downloadsFolder =
      QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QString openLocation = downloadsFolder + "/" + mxcUrl.split(u'/').constLast();

    const QString filename = QFileDialog::getSaveFileName(nullptr, {}, openLocation);

    if (filename.isEmpty())
        return;

    const auto url = mxcUrl.toStdString();

    http::client()->download(url,
                             [filename, url](const std::string &data,
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
                                     QFile file(filename);

                                     if (!file.open(QIODevice::WriteOnly))
                                         return;

                                     file.write(QByteArray(data.data(), (int)data.size()));
                                     file.close();
                                     utils::markFileAsFromWeb(filename);

                                     return;
                                 } catch (const std::exception &e) {
                                     nhlog::ui()->warn("Error while saving file to: {}", e.what());
                                 }
                             });
}

void
TimelineViewManager::openMedia(QString mxcUrl)
{
    if (mxcUrl.trimmed().isEmpty())
        return;

    if (!mxcUrl.startsWith(QLatin1String("mxc://"))) {
        QDesktopServices::openUrl(QUrl(mxcUrl));
        return;
    }

    const auto id = QString(mxcUrl).remove(QStringLiteral("mxc://"));
    MxcImageProvider::download(
      id,
      QSize{},
      [mxcUrl](QString, QSize, QImage, QString filePath) {
          if (filePath.isEmpty()) {
              nhlog::ui()->warn("Failed to resolve local file path for media '{}'",
                                mxcUrl.toStdString());
              return;
          }

          const auto opened = QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
          if (!opened) {
              nhlog::ui()->warn("Failed to open media '{}' in external app",
                                filePath.toStdString());
          }
      },
      false,
      0);
}

void
TimelineViewManager::copyImage(const QString &mxcUrl) const
{
    const auto url = mxcUrl.toStdString();
    QString mimeType;

    http::client()->download(
      url,
      [url, mimeType](const std::string &data,
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
              auto img = utils::readImage(QByteArray(data.data(), (qsizetype)data.size()));
              QGuiApplication::clipboard()->setImage(img);

              return;
          } catch (const std::exception &e) {
              nhlog::ui()->warn("Error while copying file to clipboard: {}", e.what());
          }
      });
}

//! WORKAROUND(Nico): for https://bugreports.qt.io/browse/QTBUG-93281
void
TimelineViewManager::fixImageRendering([[maybe_unused]] QQuickTextDocument *t,
                                       [[maybe_unused]] QQuickItem *i)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
    if (t) {
        QObject::connect(t->textDocument(), SIGNAL(imagesLoaded()), i, SLOT(updateWholeDocument()));
    }
#endif
}
