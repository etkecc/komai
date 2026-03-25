// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MxcAnimatedImage.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <QPointer>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QTimer>

#include <mtxclient/crypto/utils.hpp>

#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "profile/Paths.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/TimelineModel.h"
#include "ui/MainWindow.h"

#include <thread>

namespace {
uint64_t
currentMatrixRuntimeHandleId()
{
    const auto *mainWindow = MainWindow::instance();
    return mainWindow ? mainWindow->matrixBackendHandleId() : 0;
}

TimelineModel *
legacyRoomContext(QObject *roomContext)
{
    return qobject_cast<TimelineModel *>(roomContext);
}

QString
roomContextRoomId(QObject *roomContext)
{
    if (auto *room = legacyRoomContext(roomContext))
        return room->roomId();

    return roomContext ? roomContext->property("roomId").toString().trimmed() : QString{};
}

QString
matrixRuntimeMediaCacheKey(const QString &roomId, const QString &itemId)
{
    const auto digest =
      QCryptographicHash::hash((roomId + u':' + itemId).toUtf8(), QCryptographicHash::Sha256)
        .toHex();
    return QString::fromUtf8(digest);
}
}

void
MxcAnimatedImage::startDownload()
{
    // Always reset prior animation state first. The same item can be rebound to
    // a different event, and stale "loaded" state would wrongly hide static fallback.
    const bool wasLoaded = loaded_;
    loaded_              = false;
    if (wasLoaded)
        emit loadedChanged();

    movie.stop();
    movie.setDevice(nullptr);
    if (buffer.isOpen())
        buffer.close();
    buffer.setData({});

    if (eventId_.isEmpty())
        return;

    auto *legacyRoom  = legacyRoomContext(room_);
    const auto roomId = roomContextRoomId(room_);

    QString mxcUrl;
    QByteArray mimeType = mimeTypeHint_.trimmed().toUtf8();
    std::optional<mtx::crypto::EncryptedFile> encryptionInfo;

    if (legacyRoom) {
        auto event = legacyRoom->eventById(eventId_);
        if (!event) {
            nhlog::ui()->error("Failed to load media for event {}, event not found.",
                               eventId_.toStdString());
            return;
        }

        mxcUrl          = QString::fromStdString(mtx::accessors::url(*event));
        const auto mime = QString::fromStdString(mtx::accessors::mimetype(*event));
        if (!mime.isEmpty())
            mimeType = mime.toUtf8();
        encryptionInfo = mtx::accessors::file(*event);

        if (!mxcUrl.startsWith(QLatin1String("mxc://")))
            return;
    } else {
        if (roomId.isEmpty()) {
            nhlog::ui()->warn(
              "Cannot load matrix-runtime animated media '{}' without a room id hint",
              eventId_.toStdString());
            return;
        }

        if (mimeType.isEmpty())
            return;
    }

    static const auto formats = QMovie::supportedFormats();
    const bool animatable     = formats.contains(mimeType.split('/').back());
    if (animatable_ != animatable) {
        animatable_ = animatable;
        emit animatableChanged();
    }

    if (!animatable)
        return;

    QString suffix = QMimeDatabase().mimeTypeForName(mimeType).preferredSuffix();
    const auto url = mxcUrl.toStdString();
    QFileInfo filename(
      legacyRoom
        ? QFileInfo(
            app_paths::cache::mediaFileForMxc(UserSettings::instance()->profile(),
                                              QString(mxcUrl).remove(QStringLiteral("mxc://")),
                                              suffix,
                                              roomId))
        : QFileInfo(app_paths::cache::mediaFileForMxc(UserSettings::instance()->profile(),
                                                      matrixRuntimeMediaCacheKey(roomId, eventId_),
                                                      suffix,
                                                      roomId)));
    if (QDir::cleanPath(filename.filePath()) != filename.filePath()) {
        nhlog::net()->warn("Media cache path '{}' is not safe, not downloading file",
                           filename.filePath().toStdString());
        return;
    }

    QDir().mkpath(filename.path());

    QPointer<MxcAnimatedImage> self = this;

    auto processBuffer = [this, mimeType, encryptionInfo, self](QIODevice &device) {
        if (!self)
            return;

        try {
            if (buffer.isOpen()) {
                movie.stop();
                movie.setDevice(nullptr);
                buffer.close();
            }

            if (encryptionInfo) {
                QByteArray ba = device.readAll();
                std::string temp(ba.constData(), ba.size());
                temp =
                  mtx::crypto::to_string(mtx::crypto::decrypt_file(temp, encryptionInfo.value()));
                buffer.setData(temp.data(), static_cast<int>(temp.size()));
            } else {
                buffer.setData(device.readAll());
            }
            buffer.open(QIODevice::ReadOnly);
            buffer.reset();
        } catch (const std::exception &e) {
            nhlog::net()->error("Failed to setup animated image buffer: {}", e.what());
        }

        QTimer::singleShot(0, this, [this, mimeType, self] {
            if (!self)
                return;

            nhlog::ui()->info("Preparing animated media buffer with size: {}, {}",
                              buffer.bytesAvailable(),
                              buffer.isOpen());
            // Don't trust event MIME blindly: some events advertise one format while
            // bytes are a different valid image format (e.g. webp metadata with PNG bytes).
            const auto declaredFormat = mimeType.split('/').back();
            const auto detectedFormat = QImageReader::imageFormat(&buffer);
            buffer.reset();

            if (!detectedFormat.isEmpty() && declaredFormat != detectedFormat) {
                nhlog::ui()->warn(
                  "Media format mismatch for event '{}': declared='{}' detected='{}'",
                  eventId_.toStdString(),
                  declaredFormat.toStdString(),
                  detectedFormat.toStdString());

                // The static Image path is more robust for mismatched metadata/content,
                // so prefer it over attempting animated rendering with uncertain format.
                movie.stop();
                movie.setDevice(nullptr);
                const bool loadedStateChanged = loaded_;
                loaded_                       = false;
                if (loadedStateChanged)
                    emit loadedChanged();
                update();
                return;
            }

            if (!detectedFormat.isEmpty())
                movie.setFormat(detectedFormat);
            movie.setDevice(&buffer);

            if (height() != 0 && width() != 0)
                movie.setScaledSize(this->size().toSize());
            if (buffer.bytesAvailable() <
                4LL * 1024 * 1024 * 1024) // cache images smaller than 4MB in RAM
                movie.setCacheMode(QMovie::CacheAll);
            if (play_ && movie.frameCount() > 1)
                movie.start();
            else {
                movie.jumpToFrame(0);
                movie.setPaused(true);
            }

            // If animated decode fails (despite mime type claiming it's animatable),
            // report not-loaded so QML falls back to the regular static Image path.
            bool canRenderMovie = movie.isValid();
            if (canRenderMovie && movie.currentImage().isNull()) {
                movie.jumpToFrame(0);
                canRenderMovie = !movie.currentImage().isNull();
            }

            // Keep static media on the regular Image path to avoid unnecessary QMovie usage.
            if (canRenderMovie && movie.frameCount() == 1)
                canRenderMovie = false;

            if (!canRenderMovie) {
                nhlog::ui()->warn(
                  "Animated media decode failed for event '{}', falling back to static image",
                  eventId_.toStdString());
                movie.stop();
                movie.setDevice(nullptr);
            }

            const bool loadedStateChanged = loaded_ != canRenderMovie;
            loaded_                       = canRenderMovie;
            if (loadedStateChanged)
                emit loadedChanged();
            update();
        });
    };

    const auto filenamePath = filename.filePath();

    auto processData = [processBuffer, filenamePath, self](QByteArray data) mutable {
        if (!self)
            return;

        try {
            QFile file(filenamePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
            }

            QBuffer buf(&data);
            buf.open(QIODevice::ReadOnly);
            processBuffer(buf);
        } catch (const std::exception &e) {
            nhlog::ui()->warn("Error while saving animated media to cache: {}", e.what());
        }
    };

    if (filename.isReadable()) {
        QFile f(filename.filePath());
        if (f.open(QIODevice::ReadOnly)) {
            processBuffer(f);
            return;
        }
    }

    const auto handleId = currentMatrixRuntimeHandleId();
    if (handleId == 0) {
        nhlog::ui()->warn("Refusing legacy animated media fetch for event '{}' without an active "
                          "matrix-sdk runtime handle",
                          eventId_.toStdString());
        return;
    }

    std::thread([handleId,
                 mxcUrl,
                 eventId = eventId_,
                 legacyRoom,
                 self,
                 processData = std::move(processData)]() mutable {
        QString error;
        auto bytes = legacyRoom
                       ? komai::MatrixBackendRuntimeService::fetchMediaContent(
                           handleId, mxcUrl, 0, 0, false, &error)
                       : komai::MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContent(
                           handleId, eventId, 0, 0, false, &error);

        if (!self)
            return;

        if (!bytes.has_value()) {
            if (legacyRoom) {
                nhlog::net()->warn("Failed to retrieve animated media '{}': {}",
                                   mxcUrl.toStdString(),
                                   error.toStdString());
            } else {
                nhlog::net()->warn("Failed to retrieve active timeline animated media '{}': {}",
                                   eventId.toStdString(),
                                   error.toStdString());
            }
            return;
        }

        QTimer::singleShot(
          0, self, [self, processData = std::move(processData), data = *bytes]() mutable {
              if (!self)
                  return;

              processData(std::move(data));
          });
    }).detach();
}

void
MxcAnimatedImage::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (newGeometry.size() != oldGeometry.size()) {
        if (height() != 0 && width() != 0) {
            QSizeF r = movie.scaledSize();
            r.scale(newGeometry.size(), Qt::KeepAspectRatio);
            movie.setScaledSize(r.toSize());
            imageDirty = true;
            update();
        }
    }
}

QSGNode *
MxcAnimatedImage::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    if (!imageDirty)
        return oldNode;

    // If the image is offscreen, just return the old node (if it exists) to save on animation CPU
    // use. Don't return null here, or you will never be called again.
    if (clipRect().isEmpty() && oldNode)
        return oldNode;

    imageDirty      = false;
    QSGImageNode *n = static_cast<QSGImageNode *>(oldNode);
    if (!n) {
        n = window()->createImageNode();
        n->setOwnsTexture(true);
        // n->setFlags(QSGNode::OwnedByParent | QSGNode::OwnsGeometry |
        // GSGNode::OwnsMaterial);
        n->setFlags(QSGNode::OwnedByParent);
    }

    auto img = movie.currentImage();
    n->setSourceRect(img.rect());
    if (!img.isNull())
        n->setTexture(window()->createTextureFromImage(std::move(img)));
    else {
        delete n;
        return nullptr;
    }

    QSizeF r = img.size();
    r.scale(size(), Qt::KeepAspectRatio);

    n->setRect((width() - r.width()) / 2, (height() - r.height()) / 2, r.width(), r.height());
    n->setFiltering(QSGTexture::Linear);
    n->setMipmapFiltering(QSGTexture::None);

    return n;
}

#include "moc_MxcAnimatedImage.cpp"
