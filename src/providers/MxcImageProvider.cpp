// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "providers/MxcImageProvider.h"

#include <optional>

#include <mtx/common.hpp>
#include <mtxclient/crypto/client.hpp>

#include <QByteArray>
#include <QCache>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QThreadPool>
#include <QTimer>

#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "profile/Paths.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

QHash<QString, mtx::crypto::EncryptedFile> infos;

static QImage
clipRadius(QImage img, double radius);

namespace {
QString
currentProfileId()
{
    auto settings = UserSettings::instance();
    return settings ? settings->profile() : QStringLiteral("default");
}

QString
providerIdToMxcUri(const QString &id)
{
    return komai::matrix::normalizeMxcUri(id);
}

bool
isMatrixTimelineProviderId(const QString &id)
{
    return id.startsWith(QStringLiteral("matrix-timeline:"));
}

QString
providerIdToMatrixTimelineItemId(const QString &id)
{
    return isMatrixTimelineProviderId(id) ? id.mid(QStringLiteral("matrix-timeline:").size())
                                          : QString();
}

void
purgeFilesInDir(const QString &dirPath)
{
    QDir dir(dirPath,
             "",
             QDir::SortFlags(QDir::Name | QDir::IgnoreCase),
             QDir::Filter::Writable | QDir::Filter::NoDotAndDotDot | QDir::Filter::Files);

    for (const auto &fileInfo : dir.entryInfoList()) {
        if (fileInfo.fileTime(QFile::FileTime::FileAccessTime)
              .daysTo(QDateTime::currentDateTime()) > app_paths::cache::mediaPurgeAgeDays) {
            if (QFile::remove(fileInfo.absoluteFilePath()))
                nhlog::net()->info("Deleted stale media '{}'",
                                   fileInfo.absoluteFilePath().toStdString());
            else
                nhlog::net()->warn("Failed to delete stale media '{}'",
                                   fileInfo.absoluteFilePath().toStdString());
        }
    }
}

std::optional<uint64_t>
activeMatrixBackendHandleId()
{
    const auto *window = MainWindow::instance();
    if (!window || window->matrixBackendHandleId() == 0)
        return std::nullopt;

    return window->matrixBackendHandleId();
}

QImage
prepareThumbnailImage(QByteArray data,
                      const QSize &requestedSize,
                      bool cropLocally,
                      double radius,
                      const QString &id)
{
    QImage image = utils::readImage(data);
    if (!image.isNull()) {
        if (requestedSize.width() <= 0) {
            image = image.scaledToHeight(requestedSize.height(), Qt::SmoothTransformation);
        } else {
            image = image.scaled(requestedSize,
                                 cropLocally ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
            if (cropLocally) {
                image = image.copy((image.width() - requestedSize.width()) / 2,
                                   (image.height() - requestedSize.height()) / 2,
                                   requestedSize.width(),
                                   requestedSize.height());
            }
        }

        if (radius != 0) {
            image = clipRadius(std::move(image), radius);
        }
    }

    if (!isMatrixTimelineProviderId(id))
        image.setText(QStringLiteral("mxc url"), providerIdToMxcUri(id));
    return image;
}
}

MxcImageProvider::MxcImageProvider()
  : QQuickAsyncImageProvider()
{
    auto timer = new QTimer(this);
    timer->setInterval(std::chrono::hours(1));
    connect(timer, &QTimer::timeout, this, [] {
        QThreadPool::globalInstance()->start([] {
            nhlog::net()->info("Running media purge");
            const auto profile = currentProfileId();

            auto purgeMediaDir = [](const QString &baseDir) {
                purgeFilesInDir(baseDir + QStringLiteral("/thumbnails"));
                purgeFilesInDir(baseDir + QStringLiteral("/full"));
            };

            // Purge shared media
            purgeMediaDir(app_paths::cache::sharedMediaDirectory(profile));

            // Purge per-room media and remove empty room directories
            const auto roomsRoot = app_paths::cache::mediaRoot(profile) + QStringLiteral("/rooms");
            QDir roomsDir(roomsRoot,
                          "",
                          QDir::SortFlags(QDir::Name | QDir::IgnoreCase),
                          QDir::Filter::NoDotAndDotDot | QDir::Filter::Dirs);
            for (const auto &roomDirInfo : roomsDir.entryInfoList()) {
                purgeMediaDir(roomDirInfo.absoluteFilePath());
                QDir roomDir(roomDirInfo.absoluteFilePath());
                if (roomDir.isEmpty())
                    roomDir.removeRecursively();
            }
        });
    });
    timer->start();
}

QQuickImageResponse *
MxcImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    auto id_      = id;
    bool crop     = true;
    double radius = 0;
    auto size     = requestedSize;
    QString roomId;

    if (requestedSize.width() == 0 && requestedSize.height() == 0)
        size = QSize();

    auto queryStart = id.lastIndexOf('?');
    if (queryStart != -1) {
        id_            = id.left(queryStart);
        auto query     = QStringView(id).mid(queryStart + 1);
        auto queryBits = query.split('&');

        for (auto b : std::as_const(queryBits)) {
            if (b == QStringView(u"scale")) {
                crop = false;
            } else if (b.startsWith(QStringView(u"radius="))) {
                radius = b.mid(7).toDouble();
            } else if (b.startsWith(u"avatarSize=")) {
                // Logical avatar size from QML.  Apply QScreen DPR to get the
                // physical thumbnail size.  This avoids per-surface DPR variance
                // on Wayland fractional scaling (Qt may scale sourceSize by
                // different DPR values for the main window vs overlay dialogs).
                double dpr = 1.0;
                for (const auto *s : QGuiApplication::screens())
                    dpr = qMax(dpr, s->devicePixelRatio());
                int side = qMax(1, qRound(b.mid(11).toInt() * dpr));
                size     = QSize(side, side);
            } else if (b.startsWith(u"height=")) {
                size.setHeight(b.mid(7).toInt());
                size.setWidth(0);
            } else if (b.startsWith(QStringView(u"room="))) {
                roomId = b.mid(5).toString();
            }
        }
    }

    return new MxcImageResponse(id_, crop, radius, size, roomId);
}

void
MxcImageProvider::addEncryptionInfo(const mtx::crypto::EncryptedFile &info)
{
    infos.insert(QString::fromStdString(info.url), info);
}
void
MxcImageRunnable::run()
{
    MxcImageProvider::download(
      m_id,
      m_requestedSize,
      [this](QString id, QSize, QImage image, QString) {
          if (image.isNull()) {
              emit error(QStringLiteral("Failed to download image: %1").arg(id));
          } else {
              emit done(image);
          }
          this->deleteLater();
      },
      m_crop,
      m_radius,
      m_roomId);
}

static QImage
clipRadius(QImage img, double radius)
{
    QImage out(img.size(), QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);

    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath ppath;
    ppath.addRoundedRect(img.rect(), radius, radius, Qt::SizeMode::RelativeSize);

    painter.setClipPath(ppath);
    painter.drawImage(img.rect(), img);

    return out;
}

static void
possiblyUpdateAccessTime(const QFileInfo &fileInfo)
{
    if (fileInfo.fileTime(QFile::FileTime::FileAccessTime).daysTo(QDateTime::currentDateTime()) >
        7) {
        nhlog::net()->debug("Updating file time for '{}'",
                            fileInfo.absoluteFilePath().toStdString());

        QFile f(fileInfo.absoluteFilePath());

        if (!f.open(QIODevice::ReadWrite) ||
            !f.setFileTime(QDateTime::currentDateTime(), QFile::FileTime::FileAccessTime)) {
            nhlog::net()->warn("Failed to update filetime for '{}'",
                               fileInfo.absoluteFilePath().toStdString());
        }
    }
}

void
MxcImageProvider::download(const QString &id,
                           const QSize &requestedSize,
                           std::function<void(QString, QSize, QImage, QString)> then,
                           bool crop,
                           double radius,
                           const QString &roomId)
{
    if (id.isEmpty()) {
        nhlog::net()->warn("Attempted to download image with empty ID");
        then(id, QSize{}, QImage{}, QString{});
        return;
    }

    if (isMatrixTimelineProviderId(id)) {
        if (const auto handleId = activeMatrixBackendHandleId()) {
            const auto requestedWidth  = requestedSize.width() > 0 ? requestedSize.width() : 0;
            const auto requestedHeight = requestedSize.height() > 0 ? requestedSize.height() : 0;
            const auto itemId          = providerIdToMatrixTimelineItemId(id);

            QThreadPool::globalInstance()->start([requestedSize,
                                                  radius,
                                                  then,
                                                  id,
                                                  handleId,
                                                  requestedWidth,
                                                  requestedHeight,
                                                  crop,
                                                  itemId] {
                QString error;
                const auto data =
                  komai::MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContent(
                    *handleId, itemId, requestedWidth, requestedHeight, crop, &error);
                if (!data || data->isEmpty()) {
                    nhlog::net()->warn(
                      "Failed to fetch matrix-sdk active timeline media {} via backend handle {}: "
                      "{}",
                      itemId.toStdString(),
                      *handleId,
                      error.toStdString());
                    then(id, QSize(), {}, QLatin1String(""));
                    return;
                }

                auto image = prepareThumbnailImage(*data, requestedSize, false, radius, id);
                then(id, requestedSize, image, QLatin1String(""));
            });
            return;
        }

        nhlog::net()->warn("Refusing matrix-sdk active-timeline media fetch for '{}' without an "
                           "active runtime handle",
                           id.toStdString());
        then(id, QSize{}, QImage{}, QString{});
        return;
    }

    bool cropLocally = false;
    if (crop && requestedSize.width() > 96) {
        crop        = false;
        cropLocally = true;
    }

    std::optional<mtx::crypto::EncryptedFile> encryptionInfo;
    auto temp = infos.find("mxc://" + id);
    if (temp != infos.end())
        encryptionInfo = *temp;

    if (requestedSize.isValid() &&
        !encryptionInfo
        // Protect against synapse not following the spec:
        // https://github.com/matrix-org/synapse/issues/5302
        && requestedSize.height() <= 600 && requestedSize.width() <= 800) {
        QFileInfo fileInfo(app_paths::cache::mediaThumbnailFileForMxc(
          currentProfileId(), id, requestedSize, crop, radius, roomId));
        QDir().mkpath(fileInfo.absolutePath());

        if (fileInfo.exists()) {
            QImage image = utils::readImageFromFile(fileInfo.absoluteFilePath());
            if (!image.isNull()) {
                possiblyUpdateAccessTime(fileInfo);

                if (requestedSize.width() <= 0) {
                    image = image.scaledToHeight(requestedSize.height(), Qt::SmoothTransformation);
                } else {
                    image = image.scaled(requestedSize,
                                         cropLocally ? Qt::KeepAspectRatioByExpanding
                                                     : Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
                    if (cropLocally) {
                        image = image.copy((image.width() - requestedSize.width()) / 2,
                                           (image.height() - requestedSize.height()) / 2,
                                           requestedSize.width(),
                                           requestedSize.height());
                    }
                }

                if (radius != 0) {
                    image = clipRadius(std::move(image), radius);
                }

                if (!image.isNull()) {
                    then(id, requestedSize, image, fileInfo.absoluteFilePath());
                    return;
                }
            }
        }

        if (!encryptionInfo) {
            if (const auto handleId = activeMatrixBackendHandleId()) {
                const auto requestedWidth = requestedSize.width() > 0 ? requestedSize.width() : 0;
                const auto requestedHeight =
                  requestedSize.height() > 0 ? requestedSize.height() : 0;

                QThreadPool::globalInstance()->start([fileInfo,
                                                      requestedSize,
                                                      radius,
                                                      then,
                                                      id,
                                                      cropLocally,
                                                      handleId,
                                                      requestedWidth,
                                                      requestedHeight] {
                    QString error;
                    const auto data =
                      komai::MatrixBackendRuntimeService::fetchMediaContent(*handleId,
                                                                            providerIdToMxcUri(id),
                                                                            requestedWidth,
                                                                            requestedHeight,
                                                                            !cropLocally,
                                                                            &error);
                    if (!data || data->isEmpty()) {
                        nhlog::net()->warn(
                          "Failed to fetch matrix-sdk thumbnail {} via backend handle {}: {}",
                          id.toStdString(),
                          *handleId,
                          error.toStdString());
                        then(id, QSize(), {}, QLatin1String(""));
                        return;
                    }

                    auto image =
                      prepareThumbnailImage(*data, requestedSize, cropLocally, radius, id);
                    if (image.save(fileInfo.absoluteFilePath(), "png")) {
                        utils::markFileAsFromWeb(fileInfo.absoluteFilePath());
                        nhlog::ui()->debug("Wrote: {}", fileInfo.absoluteFilePath().toStdString());
                    } else {
                        nhlog::ui()->debug("Failed to write: {}",
                                           fileInfo.absoluteFilePath().toStdString());
                    }

                    then(id, requestedSize, image, fileInfo.absoluteFilePath());
                });
                return;
            }
        }

        nhlog::net()->warn("Refusing legacy thumbnail fetch for '{}' without an active matrix-sdk "
                           "runtime handle",
                           id.toStdString());
        then(id, QSize(), {}, QLatin1String(""));
    } else {
        try {
            QFileInfo fileInfo(
              app_paths::cache::mediaFullFileForMxc(currentProfileId(), id, roomId));
            QDir().mkpath(fileInfo.absolutePath());
            QFile f(fileInfo.absoluteFilePath());

            if (fileInfo.exists() && f.open(QIODevice::ReadOnly)) {
                if (encryptionInfo) {
                    QByteArray fileData = f.readAll();
                    auto tempData       = mtx::crypto::to_string(
                      mtx::crypto::decrypt_file(fileData.toStdString(), encryptionInfo.value()));
                    auto data    = QByteArray(tempData.data(), (int)tempData.size());
                    QImage image = utils::readImage(data);
                    image.setText(QStringLiteral("mxc url"), "mxc://" + id);
                    if (!image.isNull()) {
                        possiblyUpdateAccessTime(fileInfo);
                        if (radius != 0) {
                            image = clipRadius(std::move(image), radius);
                        }

                        then(id, requestedSize, image, fileInfo.absoluteFilePath());
                        return;
                    }
                } else {
                    QImage image = utils::readImageFromFile(fileInfo.absoluteFilePath());
                    if (!image.isNull()) {
                        possiblyUpdateAccessTime(fileInfo);
                        if (radius != 0) {
                            image = clipRadius(std::move(image), radius);
                        }

                        then(id, requestedSize, image, fileInfo.absoluteFilePath());
                        return;
                    }
                }
            }

            if (const auto handleId = activeMatrixBackendHandleId()) {
                QThreadPool::globalInstance()->start(
                  [fileInfo, requestedSize, then, id, radius, handleId, encryptionInfo] {
                      QString error;
                      const auto data = komai::MatrixBackendRuntimeService::fetchMediaContent(
                        *handleId, providerIdToMxcUri(id), 0, 0, false, &error);
                      if (!data || data->isEmpty()) {
                          nhlog::net()->warn(
                            "Failed to fetch matrix-sdk media {} via backend handle {}: {}",
                            id.toStdString(),
                            *handleId,
                            error.toStdString());
                          then(id, QSize(), {}, QLatin1String(""));
                          return;
                      }

                      QFile f(fileInfo.absoluteFilePath());
                      if (!f.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
                          nhlog::net()->error("Failed to write {}: {}",
                                              id.toStdString(),
                                              f.errorString().toStdString());
                          then(id, QSize(), {}, QLatin1String(""));
                          return;
                      }
                      f.write(*data);
                      f.close();
                      utils::markFileAsFromWeb(fileInfo.absoluteFilePath());

                      if (encryptionInfo) {
                          auto tempData = data->toStdString();
                          tempData      = mtx::crypto::to_string(
                            mtx::crypto::decrypt_file(tempData, encryptionInfo.value()));
                          auto decryptedData = QByteArray(tempData.data(), (int)tempData.size());
                          QImage image       = utils::readImage(decryptedData);
                          if (radius != 0)
                              image = clipRadius(std::move(image), radius);

                          image.setText(QStringLiteral("mxc url"), "mxc://" + id);
                          then(id, requestedSize, image, fileInfo.absoluteFilePath());
                          return;
                      }

                      QImage image = utils::readImageFromFile(fileInfo.absoluteFilePath());
                      if (radius != 0)
                          image = clipRadius(std::move(image), radius);
                      image.setText(QStringLiteral("mxc url"), "mxc://" + id);

                      then(id, requestedSize, image, fileInfo.absoluteFilePath());
                  });
                return;
            }

            nhlog::net()->warn(
              "Refusing legacy full-media fetch for '{}' without an active matrix-sdk runtime "
              "handle",
              id.toStdString());
            then(id, QSize(), {}, QLatin1String(""));
        } catch (std::exception &e) {
            nhlog::net()->error("Exception while downloading media: {}", e.what());
        }
    }
}

#include "moc_MxcImageProvider.cpp"
