// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineViewManager.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include "RoomlistModel.h"
#include "imagepacks/ImagePackListModel.h"
#include "logging/Logging.h"
#include "providers/MxcImageProvider.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

void
TimelineViewManager::openMediaOverlay(QObject *room,
                                      const QString &mxcUrl,
                                      const QString &eventId,
                                      double originalWidth,
                                      double proportionalHeight)
{
    if (mxcUrl.isEmpty()) {
        return;
    }

    emit showMediaOverlay(
      room, eventId, mxcUrl, originalWidth, proportionalHeight, -1, 0, QString{}, nullptr, nullptr);
}

void
TimelineViewManager::openMediaOverlayWithContext(QObject *room,
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

    emit showMediaOverlay(room,
                          eventId,
                          mxcUrl,
                          originalWidth,
                          proportionalHeight,
                          -1,
                          0,
                          QString{},
                          timeline,
                          timelineView);
}

void
TimelineViewManager::openMediaOverlay(QObject *room,
                                      const QString &mxcUrl,
                                      const QString &eventId,
                                      double originalWidth,
                                      double proportionalHeight,
                                      int mediaType,
                                      int duration,
                                      const QString &thumbnailUrl)
{
    if (mxcUrl.isEmpty())
        return;
    emit showMediaOverlay(room,
                          eventId,
                          mxcUrl,
                          originalWidth,
                          proportionalHeight,
                          mediaType,
                          duration,
                          thumbnailUrl,
                          nullptr,
                          nullptr);
}

void
TimelineViewManager::openMediaOverlayWithContext(QObject *room,
                                                 const QString &mxcUrl,
                                                 const QString &eventId,
                                                 double originalWidth,
                                                 double proportionalHeight,
                                                 int mediaType,
                                                 int duration,
                                                 const QString &thumbnailUrl,
                                                 QObject *timeline,
                                                 QObject *timelineView)
{
    if (mxcUrl.isEmpty())
        return;
    emit showMediaOverlay(room,
                          eventId,
                          mxcUrl,
                          originalWidth,
                          proportionalHeight,
                          mediaType,
                          duration,
                          thumbnailUrl,
                          timeline,
                          timelineView);
}

void
TimelineViewManager::openImagePackSettings(QString roomid)
{
    auto model = new ImagePackListModel(roomid.toStdString());
    QQmlEngine::setObjectOwnership(model, QQmlEngine::JavaScriptOwnership);
    // Keep room-pack creation conservatively disabled until matrix room-permission
    // fetch is migrated to a dedicated room-settings/image-pack surface.
    emit showImagePackSettings(model, false);
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

    if (!mxcUrl.startsWith(QLatin1String("mxc://"))) {
        nhlog::ui()->warn("Saving non-mxc media is not supported here: {}", mxcUrl.toStdString());
        return;
    }

    const auto id = QString(mxcUrl).remove(QStringLiteral("mxc://"));
    MxcImageProvider::download(
      id,
      QSize{},
      [filename, mxcUrl](QString, QSize, QImage, QString filePath) {
          if (filePath.isEmpty()) {
              nhlog::ui()->warn("Failed to resolve local file path for media '{}'",
                                mxcUrl.toStdString());
              return;
          }

          QFile::remove(filename);
          if (!QFile::copy(filePath, filename)) {
              nhlog::ui()->warn(
                "Failed to save media '{}' to '{}'", mxcUrl.toStdString(), filename.toStdString());
              return;
          }

          utils::markFileAsFromWeb(filename);
      },
      false,
      0);
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

    // Download to cache, then open the local file.
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
    if (!mxcUrl.startsWith(QLatin1String("mxc://"))) {
        nhlog::ui()->warn("Copying non-mxc media is not supported here: {}", mxcUrl.toStdString());
        return;
    }

    const auto id = QString(mxcUrl).remove(QStringLiteral("mxc://"));
    MxcImageProvider::download(
      id,
      QSize{},
      [mxcUrl](QString, QSize, QImage image, QString filePath) {
          try {
              if (image.isNull() && !filePath.isEmpty())
                  image = utils::readImageFromFile(filePath);

              if (image.isNull()) {
                  nhlog::ui()->warn("Failed to resolve image data for '{}'", mxcUrl.toStdString());
                  return;
              }

              QTimer::singleShot(0, qApp, [image = std::move(image)] {
                  QGuiApplication::clipboard()->setImage(image);
              });
          } catch (const std::exception &e) {
              nhlog::ui()->warn("Error while copying file to clipboard: {}", e.what());
          }
      },
      false,
      0);
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
