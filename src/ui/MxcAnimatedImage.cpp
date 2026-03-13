// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MxcAnimatedImage.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <QQuickWindow>
#include <QSGImageNode>

#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "profile/Paths.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/TimelineModel.h"

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

    if (!room_)
        return;
    if (eventId_.isEmpty())
        return;

    auto event = room_->eventById(eventId_);
    if (!event) {
        nhlog::ui()->error("Failed to load media for event {}, event not found.",
                           eventId_.toStdString());
        return;
    }

    QByteArray mimeType = QString::fromStdString(mtx::accessors::mimetype(*event)).toUtf8();

    static const auto formats = QMovie::supportedFormats();
    animatable_               = formats.contains(mimeType.split('/').back());
    animatableChanged();

    if (!animatable_)
        return;

    QString mxcUrl = QString::fromStdString(mtx::accessors::url(*event));

    auto encryptionInfo = mtx::accessors::file(*event);

    // If the message is a link to a non mxcUrl, don't download it
    if (!mxcUrl.startsWith(QLatin1String("mxc://"))) {
        return;
    }

    QString suffix = QMimeDatabase().mimeTypeForName(mimeType).preferredSuffix();

    const auto url  = mxcUrl.toStdString();
    const auto name = QString(mxcUrl).remove(QStringLiteral("mxc://"));
    QFileInfo filename(app_paths::cache::mediaFileForMxc(
      UserSettings::instance()->profile(), name, suffix, room_->roomId()));
    if (QDir::cleanPath(filename.filePath()) != filename.filePath()) {
        nhlog::net()->warn("mxcUrl '{}' is not safe, not downloading file", url);
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

    if (filename.isReadable()) {
        QFile f(filename.filePath());
        if (f.open(QIODevice::ReadOnly)) {
            processBuffer(f);
            return;
        }
    }

    http::client()->download(url,
                             [filename, url, processBuffer](const std::string &data,
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
                                     QFile file(filename.filePath());

                                     if (!file.open(QIODevice::WriteOnly))
                                         return;

                                     QByteArray ba(data.data(), (int)data.size());
                                     file.write(ba);
                                     file.close();

                                     QBuffer buf(&ba);
                                     buf.open(QBuffer::ReadOnly);
                                     processBuffer(buf);
                                 } catch (const std::exception &e) {
                                     nhlog::ui()->warn("Error while saving file to: {}", e.what());
                                 }
                             });
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
