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
/// Phase 1: writes OGG/Opus to a temp file, fires `recordingFinished` once
/// the file is flushed, exposes `audioLevel` so the composer banner can
/// render a level indicator. Phase 2 (realtime/streaming) will add a
/// PCM16 chunk pipeline.
class TranscriptionAudioCapture : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool recording READ recording NOTIFY stateChanged)
    Q_PROPERTY(bool hasRecording READ hasRecording NOTIFY stateChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY stateChanged)
    Q_PROPERTY(float audioLevel READ audioLevel NOTIFY audioLevelChanged)

public:
    explicit TranscriptionAudioCapture(QObject *parent = nullptr);
    ~TranscriptionAudioCapture() override;

    bool recording() const;
    bool hasRecording() const;
    QString filePath() const;
    float audioLevel() const;

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void discardRecording();

signals:
    void stateChanged();
    void audioLevelChanged();
    /// Fires once the recorder has flushed the final file to disk after a
    /// `stopRecording()` call. The path is also accessible via `filePath`.
    void recordingFinished(const QString &filePath);
    void errorOccurred(const QString &message);

private:
    void ensureInitialized();
    void cleanupTempFile();
    void startLevelMonitor();
    void stopLevelMonitor();

    QMediaCaptureSession *captureSession_ = nullptr;
    QAudioInput *audioInput_              = nullptr;
    QMediaRecorder *recorder_             = nullptr;
    QString filePath_;
    bool stopped_              = false;
    float audioLevel_          = 0.0f;
    QAudioSource *levelSource_ = nullptr;
    QIODevice *levelDevice_    = nullptr;
};
