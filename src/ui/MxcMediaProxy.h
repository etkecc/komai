// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAudioOutput>
#include <QBuffer>
#include <QMediaPlayer>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVideoSink>

// I failed to get my own buffer into the MediaPlayer in qml, so just make our own. For that we just
// need the videoSurface property, so that part is really easy!
class MxcMediaProxy : public QMediaPlayer
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MxcMedia)

    Q_PROPERTY(QObject *roomm READ room WRITE setRoom NOTIFY roomChanged REQUIRED)
    Q_PROPERTY(QString eventId READ eventId WRITE setEventId NOTIFY eventIdChanged)
    Q_PROPERTY(
      QString mimeTypeHint READ mimeTypeHint WRITE setMimeTypeHint NOTIFY mimeTypeHintChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool encrypted READ isEncrypted NOTIFY encryptedChanged)
    Q_PROPERTY(bool recoveringFromStreamingFallback READ recoveringFromStreamingFallback NOTIFY
                 recoveringFromStreamingFallbackChanged)
    // True while a load/download is in flight but playback hasn't started yet,
    // so the UI can show a spinner instead of a dead-looking play button.
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
    // Fraction (0..1) of the full-file download completed, or -1 while no
    // download with a known total is running — so the UI can show a real
    // percentage instead of an indeterminate spinner during download-then-play.
    Q_PROPERTY(double downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(int orientation READ orientation NOTIFY orientationChanged)
    Q_PROPERTY(float volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    // When true, audio output is never created — suitable for muted GIF-like video playback.
    Q_PROPERTY(bool skipAudioOutput READ skipAudioOutput WRITE setSkipAudioOutput NOTIFY
                 skipAudioOutputChanged)

public:
    MxcMediaProxy(QObject *parent = nullptr);
    ~MxcMediaProxy()
    {
        killPlayback();
        this->setSourceDevice(nullptr);
    }

    // Immediately silence and stop playback — no lingering audio.
    Q_INVOKABLE void killPlayback()
    {
        pausedAudioOutputReleaseTimer_.stop();
        streamingWatchdog_.stop();
        stopDownloadProgressPolling();
        if (auto output = audioOutput())
            output->setMuted(true);
        stop();
        releaseAudioOutput();
        // Detach the old buffer so a subsequent startDownload() can
        // write new data without the "QBuffer::setData: Buffer is open" warning.
        setSourceDevice(nullptr);
        if (buffer.isOpen())
            buffer.close();
        streaming_                  = false;
        streamingFallbackAttempted_ = false;
        streamingLoadStarted_       = false;
        setRecoveringFromStreamingFallback(false);
        setBuffering(false);
    }

    bool loaded() const { return buffer.size() > 0 || streaming_; }
    bool buffering() const { return buffering_; }
    double downloadProgress() const { return downloadProgress_; }
    bool isEncrypted() const { return encrypted_; }
    bool recoveringFromStreamingFallback() const { return recoveringFromStreamingFallback_; }
    QString eventId() const { return eventId_; }
    QString mimeTypeHint() const { return mimeTypeHint_; }
    QObject *room() const { return room_; }
    void setEventId(QString newEventId)
    {
        eventId_ = newEventId;
        emit eventIdChanged();
    }
    void setMimeTypeHint(QString mimeTypeHint)
    {
        if (mimeTypeHint_ == mimeTypeHint)
            return;

        mimeTypeHint_ = std::move(mimeTypeHint);
        emit mimeTypeHintChanged();
    }
    void setRoom(QObject *room)
    {
        room_ = room;
        emit roomChanged();
    }
    int orientation() const;

    float volume() const { return volume_; }
    bool muted() const { return muted_; }
    void setVolume(float val)
    {
        volume_ = val;
        if (auto output = audioOutput()) {
            output->setVolume(QAudio::convertVolume(
              val, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale));
        }
        emit volumeChanged();
    }
    void setMuted(bool val)
    {
        muted_ = val;
        if (auto output = audioOutput()) {
            output->setMuted(val);
        }
        emit mutedChanged();
    }

    bool skipAudioOutput() const { return skipAudioOutput_; }
    void setSkipAudioOutput(bool val)
    {
        if (skipAudioOutput_ == val)
            return;
        skipAudioOutput_ = val;
        emit skipAudioOutputChanged();
    }

signals:
    void roomChanged();
    void eventIdChanged();
    void mimeTypeHintChanged();
    void loadedChanged();
    void newBuffer(QUrl, QIODevice *buf);

    void orientationChanged();
    void encryptedChanged();
    void recoveringFromStreamingFallbackChanged();
    void bufferingChanged();
    void downloadProgressChanged();

    void volumeChanged();
    void mutedChanged();
    void skipAudioOutputChanged();

public slots:
    void startDownload(bool onlyCached = false);
    void ensureAudioReady();

private:
    void createAudioOutputIfNeeded();
    void releaseAudioOutput();
    // Abandon a failed proxy stream and play from a full local download instead.
    // Returns true if a fallback download was started; false if there was nothing
    // to fall back to (not streaming, or already attempted).
    bool fallBackToFullDownload();
    void setRecoveringFromStreamingFallback(bool recovering)
    {
        if (recoveringFromStreamingFallback_ == recovering)
            return;
        recoveringFromStreamingFallback_ = recovering;
        emit recoveringFromStreamingFallbackChanged();
    }
    void setBuffering(bool buffering)
    {
        if (buffering_ == buffering)
            return;
        buffering_ = buffering;
        emit bufferingChanged();
    }
    void setDownloadProgress(double progress)
    {
        if (downloadProgress_ == progress)
            return;
        downloadProgress_ = progress;
        emit downloadProgressChanged();
    }
    void stopDownloadProgressPolling()
    {
        downloadProgressPollTimer_.stop();
        setDownloadProgress(-1);
    }

    QObject *room_ = nullptr;
    QString eventId_;
    QString mimeTypeHint_;
    QString filename_;
    QBuffer buffer;
    float volume_                         = 1.f;
    bool muted_                           = false;
    bool skipAudioOutput_                 = false;
    bool encrypted_                       = false;
    bool streaming_                       = false;
    bool streamingFallbackAttempted_      = false;
    bool streamingLoadStarted_            = false;
    bool recoveringFromStreamingFallback_ = false;
    bool buffering_                       = false;
    double downloadProgress_              = -1;
    QTimer pausedAudioOutputReleaseTimer_{this};
    // Polls the Rust download-progress registry while the full-file download
    // thread is running; feeds the downloadProgress property.
    QTimer downloadProgressPollTimer_{this};
    // Fires if a proxy stream never reaches PlayingState in time — the signal
    // that this media can't be streamed forward (e.g. moov-at-end MP4, which
    // FFmpeg loads but silently fails to decode). Triggers the download fallback.
    QTimer streamingWatchdog_{this};
};
