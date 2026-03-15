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

class TimelineModel;

// I failed to get my own buffer into the MediaPlayer in qml, so just make our own. For that we just
// need the videoSurface property, so that part is really easy!
class MxcMediaProxy : public QMediaPlayer
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MxcMedia)

    Q_PROPERTY(TimelineModel *roomm READ room WRITE setRoom NOTIFY roomChanged REQUIRED)
    Q_PROPERTY(QString eventId READ eventId WRITE setEventId NOTIFY eventIdChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool encrypted READ isEncrypted NOTIFY encryptedChanged)
    Q_PROPERTY(bool recoveringFromStreamingFallback READ recoveringFromStreamingFallback NOTIFY
                 recoveringFromStreamingFallbackChanged)
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
        setRecoveringFromStreamingFallback(false);
    }

    bool loaded() const { return buffer.size() > 0 || streaming_; }
    bool isEncrypted() const { return encrypted_; }
    bool recoveringFromStreamingFallback() const { return recoveringFromStreamingFallback_; }
    QString eventId() const { return eventId_; }
    TimelineModel *room() const { return room_; }
    void setEventId(QString newEventId)
    {
        eventId_ = newEventId;
        emit eventIdChanged();
    }
    void setRoom(TimelineModel *room)
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
    void loadedChanged();
    void newBuffer(QUrl, QIODevice *buf);

    void orientationChanged();
    void encryptedChanged();
    void recoveringFromStreamingFallbackChanged();

    void volumeChanged();
    void mutedChanged();
    void skipAudioOutputChanged();

public slots:
    void startDownload(bool onlyCached = false);
    void ensureAudioReady();

private:
    void createAudioOutputIfNeeded();
    void releaseAudioOutput();
    void setRecoveringFromStreamingFallback(bool recovering)
    {
        if (recoveringFromStreamingFallback_ == recovering)
            return;
        recoveringFromStreamingFallback_ = recovering;
        emit recoveringFromStreamingFallbackChanged();
    }

    TimelineModel *room_ = nullptr;
    QString eventId_;
    QString filename_;
    QBuffer buffer;
    float volume_                         = 1.f;
    bool muted_                           = false;
    bool skipAudioOutput_                 = false;
    bool encrypted_                       = false;
    bool streaming_                       = false;
    bool streamingFallbackAttempted_      = false;
    bool recoveringFromStreamingFallback_ = false;
    QTimer pausedAudioOutputReleaseTimer_{this};
};
