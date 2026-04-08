// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAudioInput>
#include <QAudioSource>
#include <QMediaCaptureSession>
#include <QMediaRecorder>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>

class VoiceRecorder : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool recording READ recording NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY stateChanged)
    Q_PROPERTY(bool hasRecording READ hasRecording NOTIFY stateChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY stateChanged)
    Q_PROPERTY(int durationMs READ durationMs NOTIFY durationChanged)
    Q_PROPERTY(float audioLevel READ audioLevel NOTIFY audioLevelChanged)
    Q_PROPERTY(QList<float> waveformSamples READ waveformSamples NOTIFY waveformSamplesChanged)

public:
    explicit VoiceRecorder(QObject *parent = nullptr);
    ~VoiceRecorder() override;

    bool recording() const;
    bool paused() const;
    bool hasRecording() const;
    QString filePath() const;
    int durationMs() const;
    float audioLevel() const;
    QList<float> waveformSamples() const;

    Q_INVOKABLE QList<float> normalizedWaveform(int targetSamples = 256) const;

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void pauseRecording();
    Q_INVOKABLE void resumeRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void discardRecording();
    Q_INVOKABLE void releaseRecording();

signals:
    void stateChanged();
    void durationChanged();
    void audioLevelChanged();
    void waveformSamplesChanged();
    void errorOccurred(const QString &message);

private:
    void cleanupTempFile();
    void startLevelMonitor();
    void stopLevelMonitor();

    QMediaCaptureSession captureSession_;
    QAudioInput audioInput_;
    QMediaRecorder recorder_;
    QTimer durationTimer_;
    QString filePath_;
    int durationMs_   = 0;
    bool stopped_     = false;
    float audioLevel_ = 0.0f;
    QList<float> waveformSamples_;
    QAudioSource *levelSource_ = nullptr;
    QIODevice *levelDevice_    = nullptr;
};
