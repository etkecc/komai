// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "voip/VoiceRecorder.h"

#include <QAudioFormat>
#include <QAudioInput>
#include <QAudioSource>
#include <QDir>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QStandardPaths>
#include <QUuid>

#include <cmath>

#include "logging/Logging.h"

VoiceRecorder::VoiceRecorder(QObject *parent)
  : QObject(parent)
{
    durationTimer_.setInterval(100);
    connect(&durationTimer_, &QTimer::timeout, this, [this] {
        durationMs_ = static_cast<int>(recorder_->duration());
        emit durationChanged();
        // Accumulate current audio level as a waveform sample
        waveformSamples_.append(audioLevel_);
        emit waveformSamplesChanged();
    });
}

VoiceRecorder::~VoiceRecorder()
{
    if (recorder_ && recorder_->recorderState() != QMediaRecorder::StoppedState)
        recorder_->stop();
    cleanupTempFile();
}

void
VoiceRecorder::ensureInitialized()
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
            durationTimer_.stop();
            durationMs_ = static_cast<int>(recorder_->duration());
            emit durationChanged();
        }
        emit stateChanged();
    });

    connect(recorder_,
            &QMediaRecorder::errorOccurred,
            this,
            [this](QMediaRecorder::Error error, const QString &errorString) {
                nhlog::ui()->error(
                  "VoiceRecorder error {}: {}", static_cast<int>(error), errorString.toStdString());
                emit errorOccurred(errorString);
            });
}

bool
VoiceRecorder::recording() const
{
    return recorder_ && recorder_->recorderState() == QMediaRecorder::RecordingState;
}

bool
VoiceRecorder::paused() const
{
    return recorder_ && recorder_->recorderState() == QMediaRecorder::PausedState;
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

float
VoiceRecorder::audioLevel() const
{
    return audioLevel_;
}

QList<float>
VoiceRecorder::waveformSamples() const
{
    return waveformSamples_;
}

QList<float>
VoiceRecorder::normalizedWaveform(int targetSamples) const
{
    if (waveformSamples_.isEmpty() || targetSamples <= 0)
        return {};

    const int srcCount = waveformSamples_.size();
    if (srcCount <= targetSamples)
        return waveformSamples_;

    QList<float> result;
    result.reserve(targetSamples);

    const float bucketSize = static_cast<float>(srcCount) / static_cast<float>(targetSamples);
    for (int i = 0; i < targetSamples; ++i) {
        const int start = static_cast<int>(i * bucketSize);
        const int end   = std::min(static_cast<int>((i + 1) * bucketSize), srcCount);
        float sum       = 0.0f;
        for (int j = start; j < end; ++j)
            sum += waveformSamples_[j];
        result.append(sum / static_cast<float>(end - start));
    }
    return result;
}

void
VoiceRecorder::startRecording()
{
    ensureInitialized();

    if (recorder_->recorderState() != QMediaRecorder::StoppedState) {
        nhlog::ui()->warn("VoiceRecorder::startRecording called while already recording");
        return;
    }

    cleanupTempFile();
    stopped_    = false;
    durationMs_ = 0;
    waveformSamples_.clear();
    emit durationChanged();
    emit waveformSamplesChanged();

    const auto tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    filePath_          = tempDir + QStringLiteral("/komai-voice-") +
                QUuid::createUuid().toString(QUuid::Id128) + QStringLiteral(".ogg");

    recorder_->setOutputLocation(QUrl::fromLocalFile(filePath_));
    recorder_->record();
    durationTimer_.start();
    startLevelMonitor();
    emit stateChanged();
}

void
VoiceRecorder::pauseRecording()
{
    if (!recorder_ || recorder_->recorderState() != QMediaRecorder::RecordingState)
        return;

    recorder_->pause();
    durationTimer_.stop();
    stopLevelMonitor();
    durationMs_ = static_cast<int>(recorder_->duration());
    emit durationChanged();
    emit stateChanged();
}

void
VoiceRecorder::resumeRecording()
{
    if (!recorder_ || recorder_->recorderState() != QMediaRecorder::PausedState)
        return;

    recorder_->record();
    durationTimer_.start();
    startLevelMonitor();
    emit stateChanged();
}

void
VoiceRecorder::stopRecording()
{
    if (!recorder_ || recorder_->recorderState() == QMediaRecorder::StoppedState)
        return;

    stopped_ = true;
    stopLevelMonitor();
    recorder_->stop();
    // stateChanged emitted via the recorderStateChanged connection
}

void
VoiceRecorder::discardRecording()
{
    if (recorder_ && recorder_->recorderState() != QMediaRecorder::StoppedState)
        recorder_->stop();

    stopped_    = false;
    durationMs_ = 0;
    durationTimer_.stop();
    stopLevelMonitor();
    waveformSamples_.clear();
    cleanupTempFile();
    emit durationChanged();
    emit waveformSamplesChanged();
    emit stateChanged();
}

void
VoiceRecorder::releaseRecording()
{
    // Reset state without deleting the temp file (caller takes ownership).
    if (recorder_ && recorder_->recorderState() != QMediaRecorder::StoppedState)
        recorder_->stop();

    stopped_    = false;
    durationMs_ = 0;
    durationTimer_.stop();
    stopLevelMonitor();
    waveformSamples_.clear();
    filePath_.clear();
    emit durationChanged();
    emit waveformSamplesChanged();
    emit stateChanged();
}

void
VoiceRecorder::startLevelMonitor()
{
    if (levelSource_)
        return;

    QAudioFormat format;
    format.setSampleRate(8000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    const auto device = QMediaDevices::defaultAudioInput();
    if (!device.isFormatSupported(format)) {
        nhlog::ui()->warn(
          "VoiceRecorder: audio level format not supported, skipping level monitor");
        return;
    }

    levelSource_ = new QAudioSource(device, format, this);
    levelDevice_ = levelSource_->start();
    if (!levelDevice_) {
        nhlog::ui()->warn("VoiceRecorder: failed to start audio level monitor");
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
        // Convert to a perceptual loudness scale.
        // Speech RMS is typically 0.01-0.1, so we use a log-based curve
        // that maps this range to roughly 0.3-1.0 for visible waveform bars.
        float level = 0.0f;
        if (rms > 0.0001f) {
            // dB relative to full scale, clamped to -60..0 range
            const float db = std::clamp(20.0f * std::log10(rms), -60.0f, 0.0f);
            // Map -60..0 dB to 0..1 (with a power curve for better visual spread)
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
VoiceRecorder::stopLevelMonitor()
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
VoiceRecorder::cleanupTempFile()
{
    if (!filePath_.isEmpty()) {
        QFile::remove(filePath_);
        filePath_.clear();
    }
}

#include "moc_VoiceRecorder.cpp"
