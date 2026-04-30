// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantMap>

class QQuickTextDocument;
class QTimer;

/// Voice transcription service.
///
/// Provides a thin Qt wrapper over the Rust transcription FFI so the QML
/// composer can:
/// - Ask whether transcription is configured for a given room (drives the
///   long-press-Space behaviour and the configure-me hint).
/// - Submit a recorded audio file for batch transcription and receive the
///   resulting text via signal.
/// - Drive a realtime / WebSocket transcription session (start, push audio
///   chunks, commit on user release, cancel on Esc) and surface live
///   delta + completed events via signals.
/// - Read/write/clear api keys at the global or per-room level (drives the
///   Settings → Integrations → Voice transcription page).
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

    /// Start a realtime transcription session for `roomId`. Returns a
    /// jobId (> 0) on success, 0 on immediate failure (config not ready).
    /// On failure, `realtimeFailed` is also emitted synchronously with the
    /// same jobId of 0 — callers can simply branch on the return value.
    /// While the session is running, push audio with `pushRealtimeAudio`,
    /// then call `commitRealtime` (Space release / Stop click) or
    /// `cancelRealtime` (Esc / dismiss). Events arrive via
    /// `realtimeDelta` / `realtimeCompleted` / `realtimeFailed`.
    Q_INVOKABLE qint64 runRealtimeAsync(const QString &roomId);
    /// Push a PCM16 mono chunk into the active realtime session at
    /// `jobId`. The bytes must be little-endian PCM16 at the sample rate
    /// `TranscriptionAudioCapture.streamingSampleRate()`.
    Q_INVOKABLE void pushRealtimeAudio(qint64 jobId, const QByteArray &pcm16Bytes);
    /// Tell the server we're done sending audio. Server will respond with
    /// the polished `completed` event, after which `realtimeCompleted` is
    /// emitted and the session ends.
    Q_INVOKABLE void commitRealtime(qint64 jobId);
    /// Tear down the session immediately. After this, no further signals
    /// will be emitted for this jobId.
    Q_INVOKABLE void cancelRealtime(qint64 jobId);

    /// Read the global api key. Empty string means "not set". Designed for
    /// the Settings UI to populate a password field; the value is loaded
    /// from the secrets backend, never from `config.yml`.
    Q_INVOKABLE QString loadGlobalApiKey() const;
    Q_INVOKABLE void saveGlobalApiKey(const QString &value);
    Q_INVOKABLE void clearGlobalApiKey();

    Q_INVOKABLE QString loadRoomApiKey(const QString &roomId) const;
    Q_INVOKABLE void saveRoomApiKey(const QString &roomId, const QString &value);
    Q_INVOKABLE void clearRoomApiKey(const QString &roomId);

    /// Apply an italic + muted-foreground char format to the range
    /// `[start, start + length)` of `textDocument`. Used by the composer
    /// to render the realtime "tentative range" as visually distinct from
    /// final text. The format is stored on the underlying QTextDocument's
    /// per-character formats; clearing happens via `clearTextFormat`
    /// (or by simply removing the text, which removes the formatted
    /// range).
    Q_INVOKABLE void
    applyTentativeFormat(QQuickTextDocument *textDocument, int start, int length) const;
    /// Reset character formatting on the range back to defaults. Used
    /// when promoting tentative text in-place to final (rare; today the
    /// composer just removes-and-reinserts).
    Q_INVOKABLE void clearTextFormat(QQuickTextDocument *textDocument, int start, int length) const;

signals:
    void batchFinished(int jobId, const QString &text);
    void batchFailed(int jobId, const QString &errorCode, const QString &errorMessage);

    /// Incremental tentative text for the current utterance.
    void realtimeDelta(qint64 jobId, const QString &delta);
    /// Polished final transcript for ONE utterance. Server VAD can emit
    /// several of these per session (one per detected segment); the
    /// composer treats this as "consolidate the tentative range" and
    /// keeps the session running. Use `realtimeClosed` to detect the
    /// actual end of the session.
    void realtimeCompleted(qint64 jobId, const QString &transcript);
    /// Session ended cleanly. Always the last signal of a successful
    /// session; emitted after any final `realtimeCompleted`.
    void realtimeClosed(qint64 jobId);
    /// Session ended in failure. No further signals follow for this id.
    void realtimeFailed(qint64 jobId, const QString &errorCode, const QString &errorMessage);

private:
    void pollRealtimeEvents();

    int nextJobId_             = 1;
    qint64 realtimeJobId_      = 0;
    QTimer *realtimePollTimer_ = nullptr;
};
