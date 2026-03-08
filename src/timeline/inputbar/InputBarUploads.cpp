// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "InputBar.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QMimeData>
#include <QMimeDatabase>
#include <QVideoFrame>
#include <QVideoSink>

#include <algorithm>
#include <iterator>

#include <mtx/responses/media.hpp>
#include <mtxclient/crypto/client.hpp>

#include "TimelineModel.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "utils/Utils.h"

#include "blurhash.hpp"

MediaUpload::MediaUpload(std::unique_ptr<QIODevice> source_,
                         const QString &mimetype,
                         const QString &originalFilename,
                         bool encrypt,
                         QObject *parent)
  : QObject(parent)
  , source(std::move(source_))
  , mimetype_(std::move(mimetype))
  , originalFilename_(QFileInfo(originalFilename).fileName())
  , encrypt_(encrypt)
{
    mimeClass_ = mimetype_.left(mimetype_.indexOf(u'/'));

    if (!source->isOpen())
        source->open(QIODevice::ReadOnly);

    data = source->readAll();
    source->reset();

    if (!data.size()) {
        nhlog::ui()->warn("Attempted to upload zero-byte file?! Mimetype {}, filename {}",
                          mimetype_.toStdString(),
                          originalFilename_.toStdString());
        emit uploadFailed(this);
        return;
    }

    nhlog::ui()->debug("Mime: {}", mimetype_.toStdString());
    if (mimeClass_ == u"image") {
        QImage img = utils::readImage(data);
        setThumbnail(img.scaled(
          std::min(800, img.width()), std::min(800, img.height()), Qt::KeepAspectRatioByExpanding));

        dimensions_ = img.size();
        if (img.height() > 200 && img.width() > 360)
            img = img.scaled(360, 200, Qt::KeepAspectRatioByExpanding);
        std::vector<unsigned char> data_;
        for (int y = 0; y < img.height(); y++) {
            for (int x = 0; x < img.width(); x++) {
                auto p = img.pixel(x, y);
                data_.push_back(static_cast<unsigned char>(qRed(p)));
                data_.push_back(static_cast<unsigned char>(qGreen(p)));
                data_.push_back(static_cast<unsigned char>(qBlue(p)));
            }
        }
        blurhash_ =
          QString::fromStdString(blurhash::encode(data_.data(), img.width(), img.height(), 4, 3));
    } else if (mimeClass_ == u"video" || mimeClass_ == u"audio") {
        auto mediaPlayer = new QMediaPlayer(this);
        mediaPlayer->setAudioOutput(nullptr);

        if (mimeClass_ == u"video") {
            auto newSurface = new QVideoSink(this);
            connect(newSurface,
                    &QVideoSink::videoFrameChanged,
                    this,
                    [this, mediaPlayer](const QVideoFrame &frame) {
                        QImage img = frame.toImage();
                        if (img.size().isEmpty())
                            return;

                        mediaPlayer->stop();

                        auto orientation =
                          mediaPlayer->metaData().value(QMediaMetaData::Orientation).toInt();
                        if (orientation == 90 || orientation == 270 || orientation == 180) {
                            img = img.transformed(QTransform().rotate(orientation),
                                                  Qt::SmoothTransformation);
                        }

                        nhlog::ui()->debug("Got image {}x{}", img.width(), img.height());

                        this->setThumbnail(img);

                        if (!dimensions_.isValid())
                            this->dimensions_ = img.size();

                        if (img.height() > 200 && img.width() > 360)
                            img = img.scaled(360, 200, Qt::KeepAspectRatioByExpanding);
                        std::vector<unsigned char> data_;
                        for (int y = 0; y < img.height(); y++) {
                            for (int x = 0; x < img.width(); x++) {
                                auto p = img.pixel(x, y);
                                data_.push_back(static_cast<unsigned char>(qRed(p)));
                                data_.push_back(static_cast<unsigned char>(qGreen(p)));
                                data_.push_back(static_cast<unsigned char>(qBlue(p)));
                            }
                        }
                        blurhash_ = QString::fromStdString(
                          blurhash::encode(data_.data(), img.width(), img.height(), 4, 3));
                    });
            mediaPlayer->setVideoOutput(newSurface);
        }

        connect(mediaPlayer,
                &QMediaPlayer::errorOccurred,
                this,
                [](QMediaPlayer::Error error, QString errorString) {
                    nhlog::ui()->debug("Media player error {} and errorStr {}",
                                       static_cast<int>(error),
                                       errorString.toStdString());
                });
        connect(mediaPlayer,
                &QMediaPlayer::mediaStatusChanged,
                [mediaPlayer](QMediaPlayer::MediaStatus status) {
                    nhlog::ui()->debug("Media player status {} and error {}",
                                       static_cast<int>(status),
                                       static_cast<int>(mediaPlayer->error()));
                });
        connect(mediaPlayer, &QMediaPlayer::metaDataChanged, this, [this, mediaPlayer]() {
            nhlog::ui()->debug("Got metadata {}");

            if (mediaPlayer->duration() > 0)
                this->duration_ = mediaPlayer->duration();

            auto dimensions = mediaPlayer->metaData().value(QMediaMetaData::Resolution).toSize();
            if (!dimensions.isEmpty()) {
                dimensions_ = dimensions;
                auto orientation =
                  mediaPlayer->metaData().value(QMediaMetaData::Orientation).toInt();
                if (orientation == 90 || orientation == 270) {
                    dimensions_.transpose();
                }
            }
        });
        connect(
          mediaPlayer, &QMediaPlayer::durationChanged, this, [this, mediaPlayer](qint64 duration) {
              if (duration > 0) {
                  this->duration_ = mediaPlayer->duration();
                  if (mimeClass_ == u"audio")
                      mediaPlayer->stop();
              }
              nhlog::ui()->debug("Duration changed {}", duration);
          });

        auto originalFile = qobject_cast<QFile *>(source.get());

        mediaPlayer->setSourceDevice(
          source.get(), QUrl(originalFile ? originalFile->fileName() : originalFilename_));

        mediaPlayer->play();
    }
}

void
MediaUpload::startUpload()
{
    if (!thumbnail_.isNull() && thumbnailUrl_.isEmpty()) {
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        thumbnail_.save(&buffer, "PNG", 0);
        if (type() == MediaType::Image && ba.size() >= (data.size() - data.size() / 10)) {
            nhlog::ui()->info(
              "Thumbnail is not a lot smaller than original image, not uploading it");
            nhlog::ui()->debug(
              "\n    Image size: {:9d}\nThumbnail size: {:9d}", data.size(), ba.size());
        } else {
            auto payload = std::string(ba.data(), ba.size());
            if (encrypt_) {
                mtx::crypto::BinaryBuf buf;
                std::tie(buf, thumbnailEncryptedFile) =
                  mtx::crypto::encrypt_file(std::move(payload));
                payload = mtx::crypto::to_string(buf);
            }
            thumbnailSize_ = payload.size();

            http::client()->upload(
              payload,
              encryptedFile ? "application/octet-stream" : "image/png",
              "",
              [this](const mtx::responses::ContentURI &res, mtx::http::RequestErr err) mutable {
                  if (err) {
                      emit ChatPage::instance()->showNotification(
                        tr("Failed to upload media. Please try again."));
                      nhlog::net()->warn("failed to upload media: {} {} ({})",
                                         err->matrix_error.error,
                                         to_string(err->matrix_error.errcode),
                                         static_cast<int>(err->status_code));
                      thumbnail_ = QImage();
                      startUpload();
                      return;
                  }

                  thumbnailUrl_ = QString::fromStdString(res.content_uri);
                  if (thumbnailEncryptedFile)
                      thumbnailEncryptedFile->url = res.content_uri;

                  startUpload();
              });
            return;
        }
    }

    auto payload = std::string(data.data(), data.size());
    if (encrypt_) {
        mtx::crypto::BinaryBuf buf;
        std::tie(buf, encryptedFile) = mtx::crypto::encrypt_file(std::move(payload));
        payload                      = mtx::crypto::to_string(buf);
    }
    size_ = payload.size();

    http::client()->upload(
      payload,
      encryptedFile ? "application/octet-stream" : mimetype_.toStdString(),
      encrypt_ ? "" : originalFilename_.toStdString(),
      [this](const mtx::responses::ContentURI &res, mtx::http::RequestErr err) mutable {
          if (err) {
              emit ChatPage::instance()->showNotification(
                tr("Failed to upload media. Please try again."));
              nhlog::net()->warn("failed to upload media: {} {} ({})",
                                 err->matrix_error.error,
                                 to_string(err->matrix_error.errcode),
                                 static_cast<int>(err->status_code));
              emit uploadFailed(this);
              return;
          }

          auto url = QString::fromStdString(res.content_uri);
          if (encryptedFile)
              encryptedFile->url = res.content_uri;

          emit uploadComplete(this, std::move(url));
      });
}

void
InputBar::finalizeUpload(MediaUpload *upload, const QString &url)
{
    auto mime          = upload->mimetype();
    auto filename      = upload->filename();
    auto mimeClass     = upload->mimeClass();
    auto size          = upload->size();
    auto encryptedFile = upload->encryptedFile_();
    auto caption       = caption_;
    if (mimeClass == u"image")
        image(filename,
              encryptedFile,
              url,
              mime,
              size,
              upload->dimensions(),
              upload->thumbnailEncryptedFile_(),
              upload->thumbnailUrl(),
              upload->thumbnailSize(),
              upload->thumbnailImg().size(),
              upload->blurhash(),
              caption);
    else if (mimeClass == u"audio")
        audio(filename, encryptedFile, url, mime, size, upload->duration(), caption);
    else if (mimeClass == u"video")
        video(filename,
              encryptedFile,
              url,
              mime,
              size,
              upload->duration(),
              upload->dimensions(),
              upload->thumbnailEncryptedFile_(),
              upload->thumbnailUrl(),
              upload->thumbnailSize(),
              upload->thumbnailImg().size(),
              upload->blurhash(),
              caption);
    else
        file(filename, encryptedFile, url, mime, size, caption);

    removeRunUpload(upload);
}

void
InputBar::removeRunUpload(MediaUpload *upload)
{
    auto it = std::find_if(runningUploads.begin(),
                           runningUploads.end(),
                           [upload](const UploadHandle &h) { return h.get() == upload; });
    if (it != runningUploads.end())
        runningUploads.erase(it);

    if (runningUploads.empty()) {
        setUploading(false);
        caption_.clear();
    } else {
        runningUploads.front()->startUpload();
    }
}

void
InputBar::startUploadFromPath(const QString &path)
{
    if (path.isEmpty())
        return;

    auto file = std::make_unique<QFile>(path);

    if (!file->open(QIODevice::ReadOnly)) {
        nhlog::ui()->warn(
          "Failed to open file ({}): {}", path.toStdString(), file->errorString().toStdString());
        return;
    }

    QMimeDatabase db;
    auto mime = db.mimeTypeForFileNameAndData(path, file.get());

    startUpload(std::move(file), path, mime.name());
}

void
InputBar::startUploadFromMimeData(const QMimeData &source, const QString &format)
{
    auto file = std::make_unique<QBuffer>();
    file->setData(source.data(format));

    if (!file->open(QIODevice::ReadOnly)) {
        nhlog::ui()->warn("Failed to open buffer: {}", file->errorString().toStdString());
        return;
    }

    QMimeDatabase db;
    auto mime        = db.mimeTypeForName(format);
    auto suffix      = mime.preferredSuffix();
    QString filename = QStringLiteral("clipboard");

    startUpload(std::move(file), suffix.isEmpty() ? filename : (filename + "." + suffix), format);
}

void
InputBar::startUpload(std::unique_ptr<QIODevice> dev, const QString &orgPath, const QString &format)
{
    auto upload =
      UploadHandle(new MediaUpload(std::move(dev), format, orgPath, room->isEncrypted(), this));
    connect(upload.get(), &MediaUpload::uploadComplete, this, &InputBar::finalizeUpload);
    // TODO(Nico): Show a retry option
    connect(upload.get(), &MediaUpload::uploadFailed, this, [this](MediaUpload *up) {
        ChatPage::instance()->showNotification(tr("Upload of '%1' failed").arg(up->filename()));
        removeRunUpload(up);
    });

    unconfirmedUploads.push_back(std::move(upload));

    nhlog::ui()->debug("Uploads {}", unconfirmedUploads.size());
    emit uploadsChanged();
}

void
InputBar::acceptUploads()
{
    if (unconfirmedUploads.empty())
        return;

    bool wasntRunning = runningUploads.empty();
    runningUploads.insert(runningUploads.end(),
                          std::make_move_iterator(unconfirmedUploads.begin()),
                          std::make_move_iterator(unconfirmedUploads.end()));
    unconfirmedUploads.clear();
    emit uploadsChanged();

    if (wasntRunning) {
        setUploading(true);
        runningUploads.front()->startUpload();
    }
}

void
InputBar::declineUploads()
{
    unconfirmedUploads.clear();
    emit uploadsChanged();
}

QVariantList
InputBar::uploads() const
{
    QVariantList l;
    l.reserve((int)unconfirmedUploads.size());

    for (auto &e : unconfirmedUploads)
        l.push_back(QVariant::fromValue(e.get()));
    return l;
}

bool
InputBar::allUploadsAreImages() const
{
    if (unconfirmedUploads.empty())
        return false;

    return std::all_of(unconfirmedUploads.begin(),
                       unconfirmedUploads.end(),
                       [](const UploadHandle &h) { return h->mimeClass_ == u"image"; });
}

void
InputBar::removeUpload(int index)
{
    if (index < 0 || index >= static_cast<int>(unconfirmedUploads.size()))
        return;

    unconfirmedUploads.erase(unconfirmedUploads.begin() + index);
    emit uploadsChanged();
}
