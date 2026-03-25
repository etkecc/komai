// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MxcMediaProxy.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QMimeDatabase>
#include <QPointer>
#include <QUrl>

#include <mtxclient/crypto/utils.hpp>

#include <thread>

#include "chat/ChatPage.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "profile/Paths.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"

namespace {
uint64_t
currentMatrixRuntimeHandleId()
{
    auto *mainWindow = MainWindow::instance();
    return mainWindow ? mainWindow->matrixBackendHandleId() : 0;
}
}

MxcMediaProxy::MxcMediaProxy(QObject *parent)
  : QMediaPlayer(parent)
{
    pausedAudioOutputReleaseTimer_.setSingleShot(true);
    pausedAudioOutputReleaseTimer_.setInterval(1500);

    connect(this,
            &QMediaPlayer::errorOccurred,
            this,
            [this](QMediaPlayer::Error error, QString errorString) {
                nhlog::ui()->warn("Media player error {} and errorStr {}",
                                  static_cast<int>(error),
                                  errorString.toStdString());

                // When streaming via the proxy fails (e.g. upstream doesn't support Range,
                // proxy returns 416, QMediaPlayer/FFmpeg can't recover), fall back to
                // downloading the full file and playing from a local buffer.
                if (streaming_ && !streamingFallbackAttempted_) {
                    nhlog::ui()->info("Streaming failed, falling back to full download");
                    streamingFallbackAttempted_ = true;
                    streaming_                  = false;
                    setRecoveringFromStreamingFallback(true);
                    stop();
                    setSource(QUrl());
                    startDownload(false);
                }
            });
    connect(
      this, &MxcMediaProxy::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
          nhlog::ui()->info("Media player status {} and error {}",
                            static_cast<int>(status),
                            static_cast<int>(this->error()));
      });
    connect(
      this, &MxcMediaProxy::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState status) {
          if (status == QMediaPlayer::PlayingState) {
              pausedAudioOutputReleaseTimer_.stop();
              if (!skipAudioOutput_)
                  createAudioOutputIfNeeded();
          } else if (status == QMediaPlayer::PausedState) {
              pausedAudioOutputReleaseTimer_.start();
          } else {
              pausedAudioOutputReleaseTimer_.stop();
              releaseAudioOutput();
          }
      });
    connect(&pausedAudioOutputReleaseTimer_, &QTimer::timeout, this, [this] {
        if (playbackState() != QMediaPlayer::PausedState)
            return;

        nhlog::ui()->info("Releasing paused audio output after idle timeout");
        releaseAudioOutput();
    });
    connect(this, &MxcMediaProxy::metaDataChanged, [this]() { emit orientationChanged(); });

    connect(ChatPage::instance()->timelineManager()->rooms(),
            &RoomlistModel::currentRoomChanged,
            this,
            &MxcMediaProxy::pause);
}

int
MxcMediaProxy::orientation() const
{
    // nhlog::ui()->debug("metadata: {}",
    // availableMetaData().join(QStringLiteral(",")).toStdString());
    return metaData().value(QMediaMetaData::Orientation).toInt();
}

void
MxcMediaProxy::createAudioOutputIfNeeded()
{
    // We only set the output when playback is needed because otherwise the audio device lookup
    // takes about 500ms, which causes a lot of stutter.
    if (audioOutput())
        return;

    nhlog::ui()->info("Set audio output");
    auto newOut = new QAudioOutput(this);
    newOut->setMuted(muted_);
    newOut->setVolume(
      QAudio::convertVolume(volume_, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale));
    setAudioOutput(newOut);
}

void
MxcMediaProxy::releaseAudioOutput()
{
    auto *output = audioOutput();
    if (!output)
        return;

    output->setMuted(true);
    setAudioOutput(nullptr);
    output->deleteLater();
}

void
MxcMediaProxy::startDownload(bool onlyCached)
{
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

    QString mxcUrl   = QString::fromStdString(mtx::accessors::url(*event));
    QString mimeType = QString::fromStdString(mtx::accessors::mimetype(*event));

    auto encryptionInfo = mtx::accessors::file(*event);

    bool isEnc = encryptionInfo.has_value();
    if (isEnc != encrypted_) {
        encrypted_ = isEnc;
        emit encryptedChanged();
    }

    // If the message is a link to a non mxcUrl, don't download it
    if (!mxcUrl.startsWith(QLatin1String("mxc://"))) {
        return;
    }

    QString suffix = QMimeDatabase().mimeTypeForName(mimeType).preferredSuffix();

    const auto name = QString(mxcUrl).remove(QStringLiteral("mxc://"));
    QFileInfo filename(app_paths::cache::mediaFileForMxc(
      UserSettings::instance()->profile(), name, suffix, room_->roomId()));
    if (QDir::cleanPath(filename.filePath()) != filename.filePath()) {
        nhlog::net()->warn("mxcUrl '{}' is not safe, not downloading file", mxcUrl.toStdString());
        return;
    }

    QDir().mkpath(filename.path());

    QPointer<MxcMediaProxy> self = this;

    auto processBuffer = [this, encryptionInfo, filename, self, suffix](QIODevice &device) {
        if (!self)
            return;

        if (encryptionInfo) {
            QByteArray ba = device.readAll();
            std::string temp(ba.constData(), ba.size());
            temp = mtx::crypto::to_string(mtx::crypto::decrypt_file(temp, encryptionInfo.value()));
            buffer.setData(temp.data(), static_cast<int>(temp.size()));
        } else {
            buffer.setData(device.readAll());
        }
        buffer.open(QIODevice::ReadOnly);
        buffer.reset();

        QTimer::singleShot(0, this, [this, filename] {
            nhlog::ui()->info(
              "Playing buffer with size: {}, {}", buffer.bytesAvailable(), buffer.isOpen());
            setRecoveringFromStreamingFallback(false);
            this->setSourceDevice(&buffer, QUrl(filename.fileName()));
            emit loadedChanged();
        });
    };

    if (filename.isReadable()) {
        QFile f(filename.filePath());
        if (f.open(QIODevice::ReadOnly)) {
            processBuffer(f);
            return;
        }
    }

    if (onlyCached)
        return;

    const auto handleId = currentMatrixRuntimeHandleId();
    if (handleId == 0) {
        nhlog::ui()->warn("Refusing legacy media fetch for event '{}' without an active "
                          "matrix-sdk runtime handle",
                          eventId_.toStdString());
        return;
    }

    std::thread([filename, mxcUrl, processBuffer, self, handleId]() mutable {
        QString error;
        auto bytes = komai::MatrixBackendRuntimeService::fetchMediaContent(
          handleId, mxcUrl, 0, 0, false, &error);

        QTimer::singleShot(0,
                           ChatPage::instance(),
                           [filename,
                            mxcUrl,
                            processBuffer = std::move(processBuffer),
                            self,
                            bytes = std::move(bytes),
                            error = std::move(error)]() mutable {
                               if (!bytes || bytes->isEmpty()) {
                                   if (self)
                                       self->setRecoveringFromStreamingFallback(false);
                                   nhlog::net()->warn(
                                     "failed to retrieve media {} via matrix-sdk runtime: {}",
                                     mxcUrl.toStdString(),
                                     error.toStdString());
                                   return;
                               }

                               try {
                                   QFile file(filename.filePath());
                                   if (!file.open(QIODevice::WriteOnly))
                                       return;

                                   file.write(*bytes);
                                   file.close();

                                   QBuffer buf(&*bytes);
                                   buf.open(QBuffer::ReadOnly);
                                   processBuffer(buf);
                               } catch (const std::exception &e) {
                                   nhlog::ui()->warn("Error while saving file to: {}", e.what());
                               }
                           });
    }).detach();
}

void
MxcMediaProxy::ensureAudioReady()
{
    pausedAudioOutputReleaseTimer_.stop();
    nhlog::ui()->info("Eagerly creating audio output");
    createAudioOutputIfNeeded();
}

#include "moc_MxcMediaProxy.cpp"
