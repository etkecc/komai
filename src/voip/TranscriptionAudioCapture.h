// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class QAudioInput;
class QAudioSource;
class QIODevice;
class QMediaCaptureSession;
class QMediaRecorder;

/// Dedicated mic-only audio capture for the long-press-Space transcription
/// gesture. Intentionally separate from `VoiceRecorder` so the
/// voice-message attachment recorder and the Space-hold transcription
/// recorder don't contend for the same Qt singleton — see
/// `var/plans/composer-voice-transcription.md` § "Audio capture".
///
/// Two modes:
/// - **Batch** (`startRecording`): writes OGG/Opus to a temp file via
///   `QMediaRecorder` and fires `recordingFinished(filePath)` on stop.
///   Used by the OpenAI-compatible batch endpoint (multipart POST after
///   recording stops).
/// - **Streaming** (`startStreaming`): captures raw PCM16 mono at the
///   sample rate exposed by `streamingSampleRate()` via `QAudioSource`,
///   emitting fixed-size chunks via `pcmChunkReady(bytes)` at ~100 ms
///   cadence. No file is written. Used by the OpenAI Realtime
///   transcription protocol.
///
/// Both modes drive the `audioLevel` Q_PROPERTY so the composer banner's
/// level meter works the same regardless of mode.
class TranscriptionAudioCapture : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool recording READ recording NOTIFY stateChanged)
    Q_PROPERTY(bool hasRecording READ hasRecording NOTIFY stateChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY stateChanged)
    Q_PROPERTY(float audioLevel READ audioLevel NOTIFY audioLevelChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY stateChanged)

public:
    explicit TranscriptionAudioCapture(QObject *parent = nullptr);
    ~TranscriptionAudioCapture() override;

    bool recording() const;
    bool hasRecording() const;
    bool streaming() const;
    QString filePath() const;
    float audioLevel() const;

    /// Sample rate used by streaming mode. Pulled from the Rust realtime
    /// module so the C++ side never disagrees with what gets declared in
    /// the WebSocket `session.update`.
    Q_INVOKABLE static int streamingSampleRate();

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void discardRecording();

    /// Start capturing raw PCM16 mono at `streamingSampleRate()` Hz.
    /// Emits `pcmChunkReady(bytes)` with little-endian PCM16 chunks at
    /// roughly 100 ms cadence. No file is written. Switching between
    /// streaming and batch mode while recording is not supported — call
    /// `stopStreaming` (or `discardRecording`) first.
    Q_INVOKABLE void startStreaming();
    /// Stop streaming. Final partial chunk (if any) is flushed via one
    /// last `pcmChunkReady` before this returns.
    Q_INVOKABLE void stopStreaming();

signals:
    void stateChanged();
    void audioLevelChanged();
    /// Fires once the recorder has flushed the final file to disk after a
    /// `stopRecording()` call. The path is also accessible via `filePath`.
    /// Streaming mode does NOT fire this signal.
    void recordingFinished(const QString &filePath);
    /// Streaming-mode chunk. Bytes are little-endian PCM16 mono at
    /// `streamingSampleRate()` Hz. Roughly 100 ms of audio per emission.
    void pcmChunkReady(const QByteArray &bytes);
    void errorOccurred(const QString &message);

private:
    void ensureInitialized();
    void cleanupTempFile();
    void startLevelMonitor();
    void stopLevelMonitor();
    void emitLevelFromPcmInt16(const char *data, qsizetype byteCount);
    void teardownStreamingSource();

    QMediaCaptureSession *captureSession_ = nullptr;
    QAudioInput *audioInput_              = nullptr;
    QMediaRecorder *recorder_             = nullptr;
    QString filePath_;
    bool stopped_              = false;
    float audioLevel_          = 0.0f;
    QAudioSource *levelSource_ = nullptr;
    QIODevice *levelDevice_    = nullptr;

    // Streaming mode. `streamingSource_` runs at `streamingSampleRate()`
    // Hz and emits `pcmChunkReady` once `streamingChunkBuffer_` reaches
    // the target chunk size (~100 ms). No QMediaRecorder is involved.
    QAudioSource *streamingSource_ = nullptr;
    QIODevice *streamingDevice_    = nullptr;
    QByteArray streamingChunkBuffer_;
    bool streaming_ = false;
};
