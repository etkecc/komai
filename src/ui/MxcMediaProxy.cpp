// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MxcMediaProxy.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QMimeDatabase>
#include <QPointer>
#include <QUrl>

#include <algorithm>
#include <thread>

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "profile/Paths.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"

namespace {
uint64_t
currentMatrixRuntimeHandleId()
{
    auto *mainWindow = MainWindow::instance();
    return mainWindow ? mainWindow->matrixBackendHandleId() : 0;
}

QString
roomContextRoomId(QObject *roomContext)
{
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

MxcMediaProxy::MxcMediaProxy(QObject *parent)
  : QMediaPlayer(parent)
{
    pausedAudioOutputReleaseTimer_.setSingleShot(true);
    pausedAudioOutputReleaseTimer_.setInterval(1500);

    // If a proxy stream hasn't started playing within this window, treat it as
    // un-streamable (e.g. moov-at-end MP4, which FFmpeg loads but can't decode
    // forward) and fall back to a full download. Content that can stream —
    // audio, faststart/fragmented video — reaches PlayingState within a second
    // or two, so it is never aborted.
    streamingWatchdog_.setSingleShot(true);
    streamingWatchdog_.setInterval(8000);
    connect(&streamingWatchdog_, &QTimer::timeout, this, [this] {
        if (!streaming_ || streamingFallbackAttempted_)
            return;
        if (playbackState() == QMediaPlayer::PlayingState)
            return;
        komai::logging::ui()->info(
          "Streaming watchdog fired after {}ms without playback; falling back to full download",
          streamingWatchdog_.interval());
        fallBackToFullDownload();
    });

    connect(this,
            &QMediaPlayer::errorOccurred,
            this,
            [this](QMediaPlayer::Error error, QString errorString) {
                komai::logging::ui()->warn("Media player error {} and errorStr {}",
                                           static_cast<int>(error),
                                           errorString.toStdString());

                // When streaming via the proxy fails (e.g. upstream doesn't support Range,
                // proxy returns 416, QMediaPlayer/FFmpeg can't recover), fall back to
                // downloading the full file and playing from a local buffer. If there's
                // no fallback left, the error is terminal — stop the buffering spinner.
                if (!fallBackToFullDownload())
                    setBuffering(false);
            });
    connect(
      this, &MxcMediaProxy::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
          komai::logging::ui()->info("Media player status {} and error {}",
                                     static_cast<int>(status),
                                     static_cast<int>(this->error()));

          // The FFmpeg backend does not reliably emit errorOccurred when a streamed
          // source fails to open — it can silently transition LoadingMedia ->
          // NoMedia/InvalidMedia with error() still NoError. Treat that as a
          // streaming failure too so the full-download fallback still kicks in;
          // otherwise the video stays stuck on its thumbnail forever.
          if (status == QMediaPlayer::LoadingMedia) {
              if (streaming_)
                  streamingLoadStarted_ = true;
          } else if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) {
              // The stream opened successfully; a later NoMedia (end/stop/loop)
              // is not a load failure and must not trigger the download fallback.
              streamingLoadStarted_ = false;
          } else if (streamingLoadStarted_ &&
                     (status == QMediaPlayer::NoMedia || status == QMediaPlayer::InvalidMedia)) {
              if (!fallBackToFullDownload())
                  setBuffering(false);
          }
      });
    connect(
      this, &MxcMediaProxy::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState status) {
          if (status == QMediaPlayer::PlayingState) {
              // Frames are flowing now — the load is done, drop the spinner.
              // Streaming succeeded, so the un-streamable watchdog can stand down.
              streamingWatchdog_.stop();
              setBuffering(false);
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

        komai::logging::ui()->info("Releasing paused audio output after idle timeout");
        releaseAudioOutput();
    });
    connect(this, &MxcMediaProxy::metaDataChanged, [this]() { emit orientationChanged(); });

    // While the full-file download thread is running, poll the backend's
    // progress registry and feed the downloadProgress property.  Progress
    // stays -1 (indeterminate) until the download learns a total.
    downloadProgressPollTimer_.setInterval(150);
    connect(&downloadProgressPollTimer_, &QTimer::timeout, this, [this] {
        const auto handleId = currentMatrixRuntimeHandleId();
        if (handleId == 0 || eventId_.isEmpty())
            return;
        const auto [received, total] =
          komai::MatrixBackendRuntimeService::activeTimelineMediaDownloadProgress(handleId,
                                                                                  eventId_);
        if (total > 0)
            setDownloadProgress(std::min(static_cast<double>(received) / total, 1.0));
    });

    connect(ChatPage::instance()->timelineManager()->rooms(),
            &RoomlistModel::currentRoomIdChanged,
            this,
            &MxcMediaProxy::pause);
}

bool
MxcMediaProxy::fallBackToFullDownload()
{
    // No-op unless we were actively streaming and haven't already fallen back —
    // this is safe to call from both errorOccurred and mediaStatusChanged.
    if (!streaming_ || streamingFallbackAttempted_)
        return false;

    komai::logging::ui()->info("Streaming failed, falling back to full download");
    streamingWatchdog_.stop();
    streamingFallbackAttempted_ = true;
    streaming_                  = false;
    streamingLoadStarted_       = false;
    setRecoveringFromStreamingFallback(true);
    stop();
    setSource(QUrl());
    startDownload(false); // keeps buffering active through the download
    return true;
}

int
MxcMediaProxy::orientation() const
{
    // komai::logging::ui()->debug("metadata: {}",
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

    komai::logging::ui()->info("Set audio output");
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
    if (eventId_.isEmpty())
        return;

    const auto roomId = roomContextRoomId(room_);

    QString mimeType = mimeTypeHint_.trimmed();
    if (mimeType.isEmpty())
        mimeType = QStringLiteral("application/octet-stream");

    const auto handleId = currentMatrixRuntimeHandleId();

    // Query actual encryption status from the runtime's media lookup.
    const bool isEnc =
      (handleId != 0)
        ? komai::MatrixBackendRuntimeService::isTimelineMediaEncrypted(handleId, eventId_)
        : false;
    if (isEnc != encrypted_) {
        encrypted_ = isEnc;
        emit encryptedChanged();
    }

    QString suffix = QMimeDatabase().mimeTypeForName(mimeType).preferredSuffix();
    QFileInfo filename(
      QFileInfo(app_paths::cache::mediaFileForMxc(UserSettings::instance()->profile(),
                                                  matrixRuntimeMediaCacheKey(roomId, eventId_),
                                                  suffix,
                                                  roomId)));
    if (QDir::cleanPath(filename.filePath()) != filename.filePath()) {
        komai::logging::net()->warn("Media cache path '{}' is not safe, not downloading file",
                                    filename.filePath().toStdString());
        return;
    }

    QDir().mkpath(filename.path());

    QPointer<MxcMediaProxy> self = this;

    auto processBuffer = [this, filename, self, suffix](QIODevice &device) {
        if (!self)
            return;

        buffer.setData(device.readAll());
        buffer.open(QIODevice::ReadOnly);
        buffer.reset();

        QTimer::singleShot(0, this, [this, filename] {
            komai::logging::ui()->info(
              "Playing buffer with size: {}, {}", buffer.bytesAvailable(), buffer.isOpen());
            setRecoveringFromStreamingFallback(false);
            this->setSourceDevice(&buffer, QUrl(filename.fileName()));
            emit loadedChanged();
        });
    };

    if (filename.isReadable()) {
        QFile f(filename.filePath());
        if (f.open(QIODevice::ReadOnly)) {
            komai::logging::ui()->info("Serving media for event '{}' from the local disk cache",
                                       eventId_.toStdString());
            processBuffer(f);
            return;
        }
    }

    if (onlyCached)
        return;

    // Try streaming via the local media proxy (unencrypted media only).
    // Skip if this is a fallback retry after a streaming failure.
    if (!isEnc && !streamingFallbackAttempted_ && handleId != 0) {
        auto proxyUrl = komai::MatrixBackendRuntimeService::registerTimelineMediaProxyUrl(
          handleId, eventId_, suffix);
        if (proxyUrl) {
            komai::logging::ui()->info("Streaming media via proxy for event '{}'",
                                       eventId_.toStdString());
            setBuffering(true);
            streaming_ = true;
            setSource(QUrl(*proxyUrl));
            // Arm the watchdog: if playback doesn't start in time, fall back.
            streamingWatchdog_.start();
            emit loadedChanged();
            return;
        }
    }
    if (handleId == 0) {
        komai::logging::ui()->warn(
          "Cannot fetch matrix-sdk timeline media for event '{}' without an "
          "active runtime handle",
          eventId_.toStdString());
        setBuffering(false);
        return;
    }

    // Fetching the full file over the network takes a moment; show a spinner
    // (or, once the download reports a total, a real percentage) until
    // playback actually starts.
    setBuffering(true);
    setDownloadProgress(-1);
    downloadProgressPollTimer_.start();
    std::thread([filename, eventId = eventId_, processBuffer, self, handleId]() mutable {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        auto bytes =
          komai::MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContentWithProgress(
            context, handleId, eventId, &error);

        QTimer::singleShot(
          0,
          ChatPage::instance(),
          [filename,
           eventId,
           processBuffer = std::move(processBuffer),
           self,
           bytes = std::move(bytes),
           error = std::move(error)]() mutable {
              if (self)
                  self->stopDownloadProgressPolling();
              if (!bytes || bytes->isEmpty()) {
                  if (self) {
                      self->setRecoveringFromStreamingFallback(false);
                      self->setBuffering(false);
                  }
                  komai::logging::net()->warn(
                    "failed to retrieve active timeline media {} via matrix-sdk runtime: {}",
                    eventId.toStdString(),
                    error.toStdString());
                  return;
              }

              try {
                  QFile file(filename.filePath());
                  if (!file.open(QIODevice::WriteOnly)) {
                      if (self)
                          self->setBuffering(false);
                      return;
                  }

                  file.write(*bytes);
                  file.close();

                  QBuffer buf(&*bytes);
                  buf.open(QBuffer::ReadOnly);
                  processBuffer(buf);
              } catch (const std::exception &e) {
                  komai::logging::ui()->warn("Error while saving file to: {}", e.what());
              }
          });
    }).detach();
}

void
MxcMediaProxy::ensureAudioReady()
{
    pausedAudioOutputReleaseTimer_.stop();
    komai::logging::ui()->info("Eagerly creating audio output");
    createAudioOutputIfNeeded();
}

#include "moc_MxcMediaProxy.cpp"
