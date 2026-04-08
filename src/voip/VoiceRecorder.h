// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAudioInput>
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

public:
    explicit VoiceRecorder(QObject *parent = nullptr);
    ~VoiceRecorder() override;

    bool recording() const;
    bool paused() const;
    bool hasRecording() const;
    QString filePath() const;
    int durationMs() const;

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void pauseRecording();
    Q_INVOKABLE void resumeRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void discardRecording();
    Q_INVOKABLE void releaseRecording();

signals:
    void stateChanged();
    void durationChanged();
    void errorOccurred(const QString &message);

private:
    void cleanupTempFile();

    QMediaCaptureSession captureSession_;
    QAudioInput audioInput_;
    QMediaRecorder recorder_;
    QTimer durationTimer_;
    QString filePath_;
    int durationMs_ = 0;
    bool stopped_   = false;
};
