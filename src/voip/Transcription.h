// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantMap>

/// Voice transcription service.
///
/// Provides a thin Qt wrapper over the Rust transcription FFI so the QML
/// composer can:
/// - Ask whether transcription is configured for a given room (drives the
///   long-press-Space behaviour and the configure-me hint).
/// - Submit a recorded audio file for batch transcription and receive the
///   resulting text via signal.
/// - Read/write/clear api keys at the global or per-room level (drives the
///   Settings → Integrations → Voice transcription page).
///
/// Realtime/streaming mode is a Phase 2 add-on and surfaces here under
/// separate API once it lands.
class Transcription : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Transcription(QObject *parent = nullptr);
    ~Transcription() override;

    /// Resolve the effective transcription configuration for `roomId` under
    /// the active profile. Returns a QVariantMap with the same fields
    /// declared in `TranscriptionResolvedConfig` on the Rust side:
    /// `provider`, `apiUrl`, `hasApiKey`, `needsApiKey`, `model`,
    /// `language`, `prompt`, `isReady`. The composer-side master toggle is
    /// surfaced separately via `Settings.composerInputTranscriptionEnabled`.
    Q_INVOKABLE QVariantMap resolveForRoom(const QString &roomId) const;

    /// Submit `audioPath` for batch transcription against the configuration
    /// effective for `roomId`. Returns a job id; the result lands via the
    /// `batchFinished` or `batchFailed` signal carrying the same id.
    Q_INVOKABLE int runBatchAsync(const QString &roomId, const QString &audioPath);

    /// Read the global api key. Empty string means "not set". Designed for
    /// the Settings UI to populate a password field; the value is loaded
    /// from the secrets backend, never from `config.yml`.
    Q_INVOKABLE QString loadGlobalApiKey() const;
    Q_INVOKABLE void saveGlobalApiKey(const QString &value);
    Q_INVOKABLE void clearGlobalApiKey();

    Q_INVOKABLE QString loadRoomApiKey(const QString &roomId) const;
    Q_INVOKABLE void saveRoomApiKey(const QString &roomId, const QString &value);
    Q_INVOKABLE void clearRoomApiKey(const QString &roomId);

signals:
    void batchFinished(int jobId, const QString &text);
    void batchFailed(int jobId, const QString &errorCode, const QString &errorMessage);

private:
    int nextJobId_ = 1;
};
