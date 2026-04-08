// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "voip/VoiceRecorder.h"

#include <QDir>
#include <QMediaFormat>
#include <QStandardPaths>
#include <QUuid>

#include "logging/Logging.h"

VoiceRecorder::VoiceRecorder(QObject *parent)
  : QObject(parent)
{
    captureSession_.setAudioInput(&audioInput_);
    captureSession_.setRecorder(&recorder_);

    recorder_.setMediaFormat([] {
        QMediaFormat fmt;
        fmt.setFileFormat(QMediaFormat::Ogg);
        fmt.setAudioCodec(QMediaFormat::AudioCodec::Opus);
        return fmt;
    }());
    recorder_.setQuality(QMediaRecorder::NormalQuality);

    durationTimer_.setInterval(100);
    connect(&durationTimer_, &QTimer::timeout, this, [this] {
        durationMs_ = static_cast<int>(recorder_.duration());
        emit durationChanged();
    });

    connect(&recorder_, &QMediaRecorder::recorderStateChanged, this, [this](auto state) {
        if (state == QMediaRecorder::StoppedState && stopped_) {
            durationTimer_.stop();
            durationMs_ = static_cast<int>(recorder_.duration());
            emit durationChanged();
        }
        emit stateChanged();
    });

    connect(&recorder_,
            &QMediaRecorder::errorOccurred,
            this,
            [this](QMediaRecorder::Error error, const QString &errorString) {
                nhlog::ui()->error(
                  "VoiceRecorder error {}: {}", static_cast<int>(error), errorString.toStdString());
                emit errorOccurred(errorString);
            });
}

VoiceRecorder::~VoiceRecorder()
{
    if (recorder_.recorderState() != QMediaRecorder::StoppedState)
        recorder_.stop();
    cleanupTempFile();
}

bool
VoiceRecorder::recording() const
{
    return recorder_.recorderState() == QMediaRecorder::RecordingState;
}

bool
VoiceRecorder::paused() const
{
    return recorder_.recorderState() == QMediaRecorder::PausedState;
}

bool
VoiceRecorder::hasRecording() const
{
    return !filePath_.isEmpty() && stopped_;
}

QString
VoiceRecorder::filePath() const
{
    return filePath_;
}

int
VoiceRecorder::durationMs() const
{
    return durationMs_;
}

void
VoiceRecorder::startRecording()
{
    if (recorder_.recorderState() != QMediaRecorder::StoppedState) {
        nhlog::ui()->warn("VoiceRecorder::startRecording called while already recording");
        return;
    }

    cleanupTempFile();
    stopped_    = false;
    durationMs_ = 0;
    emit durationChanged();

    const auto tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    filePath_          = tempDir + QStringLiteral("/komai-voice-") +
                QUuid::createUuid().toString(QUuid::Id128) + QStringLiteral(".ogg");

    recorder_.setOutputLocation(QUrl::fromLocalFile(filePath_));
    recorder_.record();
    durationTimer_.start();
    emit stateChanged();
}

void
VoiceRecorder::pauseRecording()
{
    if (recorder_.recorderState() != QMediaRecorder::RecordingState)
        return;

    recorder_.pause();
    durationTimer_.stop();
    durationMs_ = static_cast<int>(recorder_.duration());
    emit durationChanged();
    emit stateChanged();
}

void
VoiceRecorder::resumeRecording()
{
    if (recorder_.recorderState() != QMediaRecorder::PausedState)
        return;

    recorder_.record();
    durationTimer_.start();
    emit stateChanged();
}

void
VoiceRecorder::stopRecording()
{
    if (recorder_.recorderState() == QMediaRecorder::StoppedState)
        return;

    stopped_ = true;
    recorder_.stop();
    // stateChanged emitted via the recorderStateChanged connection
}

void
VoiceRecorder::discardRecording()
{
    if (recorder_.recorderState() != QMediaRecorder::StoppedState)
        recorder_.stop();

    stopped_    = false;
    durationMs_ = 0;
    durationTimer_.stop();
    cleanupTempFile();
    emit durationChanged();
    emit stateChanged();
}

void
VoiceRecorder::releaseRecording()
{
    // Reset state without deleting the temp file (caller takes ownership).
    if (recorder_.recorderState() != QMediaRecorder::StoppedState)
        recorder_.stop();

    stopped_    = false;
    durationMs_ = 0;
    durationTimer_.stop();
    filePath_.clear();
    emit durationChanged();
    emit stateChanged();
}

void
VoiceRecorder::cleanupTempFile()
{
    if (!filePath_.isEmpty()) {
        QFile::remove(filePath_);
        filePath_.clear();
    }
}

#include "moc_VoiceRecorder.cpp"
