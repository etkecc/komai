// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "voip/TranscriptionAudioCapture.h"

#include <QAudioFormat>
#include <QAudioInput>
#include <QAudioSource>
#include <QFile>
#include <QIODevice>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <cmath>

#include "logging/Logging.h"

TranscriptionAudioCapture::TranscriptionAudioCapture(QObject *parent)
  : QObject(parent)
{
}

TranscriptionAudioCapture::~TranscriptionAudioCapture()
{
    if (recorder_ && recorder_->recorderState() != QMediaRecorder::StoppedState)
        recorder_->stop();
    cleanupTempFile();
}

void
TranscriptionAudioCapture::ensureInitialized()
{
    if (recorder_)
        return;

    audioInput_     = new QAudioInput(this);
    captureSession_ = new QMediaCaptureSession(this);
    recorder_       = new QMediaRecorder(this);

    captureSession_->setAudioInput(audioInput_);
    captureSession_->setRecorder(recorder_);

    recorder_->setMediaFormat([] {
        QMediaFormat fmt;
        fmt.setFileFormat(QMediaFormat::Ogg);
        fmt.setAudioCodec(QMediaFormat::AudioCodec::Opus);
        return fmt;
    }());
    recorder_->setQuality(QMediaRecorder::NormalQuality);

    connect(recorder_, &QMediaRecorder::recorderStateChanged, this, [this](auto state) {
        if (state == QMediaRecorder::StoppedState && stopped_) {
            emit stateChanged();
            emit recordingFinished(filePath_);
            return;
        }
        emit stateChanged();
    });

    connect(recorder_,
            &QMediaRecorder::errorOccurred,
            this,
            [this](QMediaRecorder::Error error, const QString &errorString) {
                komai::logging::ui()->error("TranscriptionAudioCapture error {}: {}",
                                            static_cast<int>(error),
                                            errorString.toStdString());
                emit errorOccurred(errorString);
            });
}

bool
TranscriptionAudioCapture::recording() const
{
    return recorder_ && recorder_->recorderState() == QMediaRecorder::RecordingState;
}

bool
TranscriptionAudioCapture::hasRecording() const
{
    return !filePath_.isEmpty() && stopped_;
}

QString
TranscriptionAudioCapture::filePath() const
{
    return filePath_;
}

float
TranscriptionAudioCapture::audioLevel() const
{
    return audioLevel_;
}

void
TranscriptionAudioCapture::startRecording()
{
    ensureInitialized();

    if (recorder_->recorderState() != QMediaRecorder::StoppedState) {
        komai::logging::ui()->warn(
          "TranscriptionAudioCapture::startRecording called while already recording");
        return;
    }

    cleanupTempFile();
    stopped_ = false;

    const auto tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    filePath_          = tempDir + QStringLiteral("/komai-transcription-") +
                QUuid::createUuid().toString(QUuid::Id128) + QStringLiteral(".ogg");

    recorder_->setOutputLocation(QUrl::fromLocalFile(filePath_));
    recorder_->record();
    startLevelMonitor();
    emit stateChanged();
}

void
TranscriptionAudioCapture::stopRecording()
{
    if (!recorder_ || recorder_->recorderState() == QMediaRecorder::StoppedState)
        return;

    stopped_ = true;
    stopLevelMonitor();
    recorder_->stop();
    // stateChanged + recordingFinished emitted via the recorderStateChanged connection.
}

void
TranscriptionAudioCapture::discardRecording()
{
    if (recorder_ && recorder_->recorderState() != QMediaRecorder::StoppedState)
        recorder_->stop();

    stopped_ = false;
    stopLevelMonitor();
    cleanupTempFile();
    emit stateChanged();
}

void
TranscriptionAudioCapture::startLevelMonitor()
{
    if (levelSource_)
        return;

    QAudioFormat format;
    format.setSampleRate(8000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    const auto device = QMediaDevices::defaultAudioInput();
    if (!device.isFormatSupported(format)) {
        komai::logging::ui()->warn(
          "TranscriptionAudioCapture: audio level format not supported, skipping level monitor");
        return;
    }

    levelSource_ = new QAudioSource(device, format, this);
    levelDevice_ = levelSource_->start();
    if (!levelDevice_) {
        komai::logging::ui()->warn(
          "TranscriptionAudioCapture: failed to start audio level monitor");
        delete levelSource_;
        levelSource_ = nullptr;
        return;
    }

    connect(levelDevice_, &QIODevice::readyRead, this, [this] {
        const auto data = levelDevice_->readAll();
        if (data.isEmpty())
            return;

        const auto *samples   = reinterpret_cast<const int16_t *>(data.constData());
        const int sampleCount = static_cast<int>(data.size() / sizeof(int16_t));
        double sumSquares     = 0.0;
        for (int i = 0; i < sampleCount; ++i) {
            const double s = static_cast<double>(samples[i]) / 32768.0;
            sumSquares += s * s;
        }
        const float rms = static_cast<float>(std::sqrt(sumSquares / std::max(1, sampleCount)));
        // Same perceptual mapping as VoiceRecorder: speech RMS is typically
        // 0.01-0.1, mapped via a log curve to roughly 0.3-1.0 for visible
        // bars. Kept in sync deliberately so the composer banner reads the
        // same loudness scale users already see for voice messages.
        float level = 0.0f;
        if (rms > 0.0001f) {
            const float db         = std::clamp(20.0f * std::log10(rms), -60.0f, 0.0f);
            const float normalized = (db + 60.0f) / 60.0f;
            level                  = std::clamp(normalized * normalized, 0.0f, 1.0f);
        }
        if (std::abs(audioLevel_ - level) > 0.005f) {
            audioLevel_ = level;
            emit audioLevelChanged();
        }
    });
}

void
TranscriptionAudioCapture::stopLevelMonitor()
{
    if (levelSource_) {
        levelSource_->stop();
        delete levelSource_;
        levelSource_ = nullptr;
        levelDevice_ = nullptr;
    }
    if (audioLevel_ != 0.0f) {
        audioLevel_ = 0.0f;
        emit audioLevelChanged();
    }
}

void
TranscriptionAudioCapture::cleanupTempFile()
{
    if (!filePath_.isEmpty()) {
        QFile::remove(filePath_);
        filePath_.clear();
    }
}

#include "moc_TranscriptionAudioCapture.cpp"
