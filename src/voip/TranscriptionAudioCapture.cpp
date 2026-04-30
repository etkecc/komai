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

#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"

TranscriptionAudioCapture::TranscriptionAudioCapture(QObject *parent)
  : QObject(parent)
{
}

TranscriptionAudioCapture::~TranscriptionAudioCapture()
{
    if (recorder_ && recorder_->recorderState() != QMediaRecorder::StoppedState)
        recorder_->stop();
    teardownStreamingSource();
    cleanupTempFile();
}

int
TranscriptionAudioCapture::streamingSampleRate()
{
    return static_cast<int>(::komai::rust::transcription_realtime_sample_rate_hz());
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
    if (streaming_)
        return true;
    return recorder_ && recorder_->recorderState() == QMediaRecorder::RecordingState;
}

bool
TranscriptionAudioCapture::streaming() const
{
    return streaming_;
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
    if (streaming_) {
        komai::logging::ui()->warn(
          "TranscriptionAudioCapture::startRecording called while streaming; ignoring");
        return;
    }

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
    if (streaming_) {
        teardownStreamingSource();
        streaming_ = false;
        emit stateChanged();
        return;
    }

    if (recorder_ && recorder_->recorderState() != QMediaRecorder::StoppedState)
        recorder_->stop();

    stopped_ = false;
    stopLevelMonitor();
    cleanupTempFile();
    emit stateChanged();
}

void
TranscriptionAudioCapture::startStreaming()
{
    if (streaming_) {
        komai::logging::ui()->warn(
          "TranscriptionAudioCapture::startStreaming called while already streaming");
        return;
    }
    if (recorder_ && recorder_->recorderState() != QMediaRecorder::StoppedState) {
        komai::logging::ui()->warn(
          "TranscriptionAudioCapture::startStreaming called while batch recording; ignoring");
        return;
    }
    // Streaming mode owns its own QAudioSource (different sample rate &
    // pipeline shape than the level monitor used for batch). Make sure the
    // batch-mode level monitor is torn down to avoid contention on the mic.
    stopLevelMonitor();

    QAudioFormat format;
    format.setSampleRate(streamingSampleRate());
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    const auto device = QMediaDevices::defaultAudioInput();
    if (!device.isFormatSupported(format)) {
        komai::logging::ui()->warn(
          "TranscriptionAudioCapture: streaming PCM16 mono @{} Hz not supported by default input",
          streamingSampleRate());
        emit errorOccurred(
          tr("Microphone does not support PCM16 mono at the rate the realtime API expects."));
        return;
    }

    streamingSource_ = new QAudioSource(device, format, this);
    streamingDevice_ = streamingSource_->start();
    if (!streamingDevice_) {
        komai::logging::ui()->warn(
          "TranscriptionAudioCapture: failed to start streaming PCM16 source");
        delete streamingSource_;
        streamingSource_ = nullptr;
        emit errorOccurred(tr("Failed to start microphone capture for realtime transcription."));
        return;
    }

    streamingChunkBuffer_.clear();
    streaming_ = true;

    // Target: ~100 ms of audio per chunk → samples = rate * 0.1; bytes = samples * 2 (mono Int16).
    const int targetBytes = std::max(1024, streamingSampleRate() / 10 * 2);

    connect(streamingDevice_, &QIODevice::readyRead, this, [this, targetBytes] {
        if (!streamingDevice_)
            return;
        const auto data = streamingDevice_->readAll();
        if (data.isEmpty())
            return;

        emitLevelFromPcmInt16(data.constData(), data.size());
        streamingChunkBuffer_.append(data);

        while (streamingChunkBuffer_.size() >= targetBytes) {
            const QByteArray chunk = streamingChunkBuffer_.left(targetBytes);
            streamingChunkBuffer_.remove(0, targetBytes);
            emit pcmChunkReady(chunk);
        }
    });

    emit stateChanged();
}

void
TranscriptionAudioCapture::stopStreaming()
{
    if (!streaming_)
        return;

    // Flush any remaining buffered samples as a final partial chunk so the
    // server sees the tail end of the utterance before we send commit.
    if (streamingDevice_) {
        const auto tail = streamingDevice_->readAll();
        if (!tail.isEmpty()) {
            emitLevelFromPcmInt16(tail.constData(), tail.size());
            streamingChunkBuffer_.append(tail);
        }
    }
    if (!streamingChunkBuffer_.isEmpty()) {
        emit pcmChunkReady(streamingChunkBuffer_);
        streamingChunkBuffer_.clear();
    }

    teardownStreamingSource();
    streaming_ = false;
    if (audioLevel_ != 0.0f) {
        audioLevel_ = 0.0f;
        emit audioLevelChanged();
    }
    emit stateChanged();
}

void
TranscriptionAudioCapture::teardownStreamingSource()
{
    if (streamingSource_) {
        streamingSource_->stop();
        delete streamingSource_;
        streamingSource_ = nullptr;
    }
    streamingDevice_ = nullptr;
    streamingChunkBuffer_.clear();
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
        emitLevelFromPcmInt16(data.constData(), data.size());
    });
}

void
TranscriptionAudioCapture::emitLevelFromPcmInt16(const char *data, qsizetype byteCount)
{
    if (!data || byteCount <= 0)
        return;

    const auto *samples   = reinterpret_cast<const int16_t *>(data);
    const int sampleCount = static_cast<int>(byteCount / static_cast<qsizetype>(sizeof(int16_t)));
    if (sampleCount <= 0)
        return;
    double sumSquares = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        const double s = static_cast<double>(samples[i]) / 32768.0;
        sumSquares += s * s;
    }
    const float rms = static_cast<float>(std::sqrt(sumSquares / sampleCount));
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
