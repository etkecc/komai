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
#include <QFileInfo>
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
#include "ui/NotificationAction.h"
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
    if (!mxcUrl.startsWith(QLatin1String("mxc://"))) {
        komai::logging::ui()->warn("Saving non-mxc media is not supported here: {}",
                                   mxcUrl.toStdString());
        return;
    }

    const QString downloadsFolder =
      QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);

    // The dialog must stay non-blocking: a static getSaveFileName() spins a
    // nested event loop while the invoking QML signal handler is still on the
    // stack, and message dialogs delete themselves shortly after closing —
    // destroying an object mid-handler aborts the application.
    auto *dialog = new QFileDialog(nullptr, {}, downloadsFolder);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->selectFile(mxcUrl.split(u'/').constLast());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::ApplicationModal);
    connect(dialog, &QFileDialog::fileSelected, this, [mxcUrl](const QString &filename) {
        if (filename.isEmpty())
            return;

        saveMxcMediaToFile(mxcUrl, filename);
    });
    dialog->show();
}

void
TimelineViewManager::saveMxcMediaToFile(const QString &mxcUrl, const QString &filename)
{
    const auto id = QString(mxcUrl).remove(QStringLiteral("mxc://"));
    MxcImageProvider::download(
      id,
      QSize{},
      [filename, mxcUrl](QString, QSize, QImage, QString filePath) {
          auto *mainWindow = MainWindow::instance();
          if (filePath.isEmpty()) {
              komai::logging::ui()->warn("Failed to resolve local file path for media '{}'",
                                         mxcUrl.toStdString());
              if (mainWindow) {
                  mainWindow->showNotification(
                    TimelineViewManager::tr("Failed to resolve media for saving"));
              }
              return;
          }

          QFile::remove(filename);
          if (!QFile::copy(filePath, filename)) {
              komai::logging::ui()->warn(
                "Failed to save media '{}' to '{}'", mxcUrl.toStdString(), filename.toStdString());
              if (mainWindow) {
                  mainWindow->showNotification(
                    TimelineViewManager::tr("Failed to save media to '%1'").arg(filename));
              }
              return;
          }

          utils::markFileAsFromWeb(filename);

          if (mainWindow) {
              const QUrl fileUrl = QUrl::fromLocalFile(filename);
              const QList<komai::NotificationAction> actions{
                {komai::NotificationAction::OpenUrl,
                 TimelineViewManager::tr("Open"),
                 QStringLiteral("qrc:/icons/icons/ui/open-externally.svg"),
                 fileUrl},
                {komai::NotificationAction::RevealInFolder,
                 TimelineViewManager::tr("Show in folder"),
                 QStringLiteral("qrc:/icons/icons/ui/folder-open.svg"),
                 fileUrl},
              };
              emit mainWindow->showNotificationWithActions(
                TimelineViewManager::tr("Saved to '%1'").arg(QFileInfo(filename).fileName()),
                komai::toVariantList(actions));
          }
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
              komai::logging::ui()->warn("Failed to resolve local file path for media '{}'",
                                         mxcUrl.toStdString());
              return;
          }

          const auto opened = QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
          if (!opened) {
              komai::logging::ui()->warn("Failed to open media '{}' in external app",
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
        komai::logging::ui()->warn("Copying non-mxc media is not supported here: {}",
                                   mxcUrl.toStdString());
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
                  komai::logging::ui()->warn("Failed to resolve image data for '{}'",
                                             mxcUrl.toStdString());
                  return;
              }

              QTimer::singleShot(0, qApp, [image = std::move(image)] {
                  QGuiApplication::clipboard()->setImage(image);
              });
          } catch (const std::exception &e) {
              komai::logging::ui()->warn("Error while copying file to clipboard: {}", e.what());
          }
      },
      false,
      0);
}

//! Work around QTBUG-93281 on Qt < 6.7.
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
