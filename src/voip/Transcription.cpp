// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "voip/Transcription.h"

#include <QColor>
#include <QFutureWatcher>
#include <QPointer>
#include <QQuickTextDocument>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {
QString
currentProfileId()
{
    auto settings = UserSettings::instance();
    return settings ? settings->profile() : QStringLiteral("default");
}

QString
errorCodeToString(::komai::rust::TranscriptionErrorCodeFfi code)
{
    switch (code) {
    case ::komai::rust::TranscriptionErrorCodeFfi::Ok:
        return QStringLiteral("ok");
    case ::komai::rust::TranscriptionErrorCodeFfi::NotConfigured:
        return QStringLiteral("not_configured");
    case ::komai::rust::TranscriptionErrorCodeFfi::Network:
        return QStringLiteral("network");
    case ::komai::rust::TranscriptionErrorCodeFfi::Unauthorized:
        return QStringLiteral("unauthorized");
    case ::komai::rust::TranscriptionErrorCodeFfi::ServerError:
        return QStringLiteral("server_error");
    case ::komai::rust::TranscriptionErrorCodeFfi::InvalidResponse:
        return QStringLiteral("invalid_response");
    case ::komai::rust::TranscriptionErrorCodeFfi::InvalidAudio:
        return QStringLiteral("invalid_audio");
    case ::komai::rust::TranscriptionErrorCodeFfi::Internal:
        return QStringLiteral("internal");
    }
    return QStringLiteral("unknown");
}
} // namespace

Transcription::Transcription(QObject *parent)
  : QObject(parent)
{
}

Transcription::~Transcription()
{
    if (realtimeJobId_ != 0) {
        ::komai::rust::transcription_realtime_cancel(realtimeJobId_);
        realtimeJobId_ = 0;
    }
}

QVariantMap
Transcription::resolveForRoom(const QString &roomId) const
{
    const auto profile = currentProfileId();
    const auto resolved =
      ::komai::rust::transcription_resolve_for_room(profile.toStdString(), roomId.toStdString());

    QVariantMap result;
    result.insert(QStringLiteral("provider"),
                  QString::fromStdString(std::string(resolved.provider)));
    result.insert(QStringLiteral("apiUrl"), QString::fromStdString(std::string(resolved.api_url)));
    result.insert(QStringLiteral("hasApiKey"), resolved.has_api_key);
    result.insert(QStringLiteral("needsApiKey"), resolved.needs_api_key);
    result.insert(QStringLiteral("model"), QString::fromStdString(std::string(resolved.model)));
    result.insert(QStringLiteral("language"),
                  QString::fromStdString(std::string(resolved.language)));
    result.insert(QStringLiteral("prompt"), QString::fromStdString(std::string(resolved.prompt)));
    result.insert(QStringLiteral("isReady"), resolved.is_ready);
    return result;
}

int
Transcription::runBatchAsync(const QString &roomId, const QString &audioPath)
{
    const int jobId    = nextJobId_++;
    const auto profile = currentProfileId();
    komai::logging::ui()->info("Transcription: batch jobId={} room={} path={}",
                               jobId,
                               roomId.toStdString(),
                               audioPath.toStdString());

    QPointer<Transcription> self(this);
    auto future = QtConcurrent::run(QThreadPool::globalInstance(), [profile, roomId, audioPath]() {
        return ::komai::rust::transcription_run_batch(
          profile.toStdString(), roomId.toStdString(), audioPath.toStdString());
    });

    auto *watcher = new QFutureWatcher<::komai::rust::TranscriptionBatchResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [self, watcher, jobId]() {
        watcher->deleteLater();
        if (!self)
            return;
        const auto result = watcher->result();
        if (result.success) {
            komai::logging::ui()->info(
              "Transcription: batch jobId={} finished, text_len={}", jobId, result.text.size());
            emit self->batchFinished(jobId, QString::fromStdString(std::string(result.text)));
        } else {
            komai::logging::ui()->warn("Transcription: batch jobId={} failed; code={} msg={}",
                                       jobId,
                                       static_cast<int>(result.error_code),
                                       std::string(result.error_message));
            emit self->batchFailed(jobId,
                                   errorCodeToString(result.error_code),
                                   QString::fromStdString(std::string(result.error_message)));
        }
    });
    watcher->setFuture(future);
    return jobId;
}

QString
Transcription::loadGlobalApiKey() const
{
    const auto profile = currentProfileId();
    const auto value   = ::komai::rust::transcription_load_global_api_key(profile.toStdString());
    if (!value.has_value)
        return {};
    return QString::fromStdString(std::string(value.value));
}

void
Transcription::saveGlobalApiKey(const QString &value)
{
    const auto profile = currentProfileId();
    ::komai::rust::transcription_save_global_api_key(profile.toStdString(), value.toStdString());
}

void
Transcription::clearGlobalApiKey()
{
    const auto profile = currentProfileId();
    ::komai::rust::transcription_clear_global_api_key(profile.toStdString());
}

QString
Transcription::loadRoomApiKey(const QString &roomId) const
{
    const auto profile = currentProfileId();
    const auto value =
      ::komai::rust::transcription_load_room_api_key(profile.toStdString(), roomId.toStdString());
    if (!value.has_value)
        return {};
    return QString::fromStdString(std::string(value.value));
}

void
Transcription::saveRoomApiKey(const QString &roomId, const QString &value)
{
    const auto profile = currentProfileId();
    ::komai::rust::transcription_save_room_api_key(
      profile.toStdString(), roomId.toStdString(), value.toStdString());
}

void
Transcription::clearRoomApiKey(const QString &roomId)
{
    const auto profile = currentProfileId();
    ::komai::rust::transcription_clear_room_api_key(profile.toStdString(), roomId.toStdString());
}

qint64
Transcription::runRealtimeAsync(const QString &roomId)
{
    if (realtimeJobId_ != 0) {
        // Defensive: composer should have cancelled before starting a new
        // session, but if it didn't, tear down the previous one cleanly.
        komai::logging::ui()->info(
          "Transcription: starting new realtime session while job {} still alive; cancelling old",
          realtimeJobId_);
        ::komai::rust::transcription_realtime_cancel(realtimeJobId_);
        realtimeJobId_ = 0;
        if (realtimePollTimer_)
            realtimePollTimer_->stop();
    }

    const auto profile = currentProfileId();
    const auto result =
      ::komai::rust::transcription_run_realtime(profile.toStdString(), roomId.toStdString());

    if (!result.accepted || result.job_id == 0) {
        // Surface the failure synchronously so the QML side gets a single,
        // consistent path: it always sees a `realtimeFailed` for a session
        // that didn't pan out.
        komai::logging::ui()->warn(
          "Transcription: realtime session refused at start; code={} message={}",
          static_cast<int>(result.error_code),
          std::string(result.error_message));
        emit realtimeFailed(0,
                            errorCodeToString(result.error_code),
                            QString::fromStdString(std::string(result.error_message)));
        return 0;
    }

    realtimeJobId_ = result.job_id;
    komai::logging::ui()->info("Transcription: realtime session started, jobId={} room={}",
                               realtimeJobId_,
                               roomId.toStdString());

    if (!realtimePollTimer_) {
        realtimePollTimer_ = new QTimer(this);
        realtimePollTimer_->setInterval(50);
        connect(realtimePollTimer_, &QTimer::timeout, this, &Transcription::pollRealtimeEvents);
    }
    realtimePollTimer_->start();

    return realtimeJobId_;
}

void
Transcription::pushRealtimeAudio(qint64 jobId, const QByteArray &pcm16Bytes)
{
    if (jobId == 0 || jobId != realtimeJobId_)
        return;
    if (pcm16Bytes.isEmpty())
        return;
    ::rust::Slice<const uint8_t> slice{
      reinterpret_cast<const uint8_t *>(pcm16Bytes.constData()),
      static_cast<std::size_t>(pcm16Bytes.size()),
    };
    ::komai::rust::transcription_realtime_push_audio(jobId, slice);
}

void
Transcription::commitRealtime(qint64 jobId)
{
    if (jobId == 0 || jobId != realtimeJobId_)
        return;
    komai::logging::ui()->info("Transcription: realtime commit for jobId={}", jobId);
    ::komai::rust::transcription_realtime_commit(jobId);
}

void
Transcription::cancelRealtime(qint64 jobId)
{
    if (jobId == 0)
        return;
    komai::logging::ui()->info("Transcription: realtime cancel for jobId={}", jobId);
    ::komai::rust::transcription_realtime_cancel(jobId);
    if (jobId == realtimeJobId_) {
        realtimeJobId_ = 0;
        if (realtimePollTimer_)
            realtimePollTimer_->stop();
    }
}

void
Transcription::applyTentativeFormat(QQuickTextDocument *textDocument, int start, int length) const
{
    if (!textDocument || length <= 0)
        return;
    auto *doc = textDocument->textDocument();
    if (!doc)
        return;
    const int docLength = doc->characterCount() - 1; // -1 for trailing block sep
    if (start < 0 || start >= docLength)
        return;
    const int clampedLength = std::min(length, docLength - start);
    if (clampedLength <= 0)
        return;

    QTextCursor cursor(doc);
    cursor.setPosition(start);
    cursor.setPosition(start + clampedLength, QTextCursor::KeepAnchor);

    QTextCharFormat fmt;
    fmt.setFontItalic(true);
    // Muted foreground: blend palette `text` with `window`. Without
    // direct palette access we fall back to a neutral grey that reads as
    // "secondary" on both light and dark themes; the QML side could
    // re-call this with a theme-aware brush if needed.
    fmt.setForeground(QColor(128, 128, 128));
    cursor.mergeCharFormat(fmt);
}

void
Transcription::clearTextFormat(QQuickTextDocument *textDocument, int start, int length) const
{
    if (!textDocument || length <= 0)
        return;
    auto *doc = textDocument->textDocument();
    if (!doc)
        return;
    const int docLength = doc->characterCount() - 1;
    if (start < 0 || start >= docLength)
        return;
    const int clampedLength = std::min(length, docLength - start);
    if (clampedLength <= 0)
        return;

    QTextCursor cursor(doc);
    cursor.setPosition(start);
    cursor.setPosition(start + clampedLength, QTextCursor::KeepAnchor);
    cursor.setCharFormat(QTextCharFormat());
}

void
Transcription::pollRealtimeEvents()
{
    if (realtimeJobId_ == 0) {
        if (realtimePollTimer_)
            realtimePollTimer_->stop();
        return;
    }

    const auto events = ::komai::rust::transcription_realtime_drain_events(realtimeJobId_);
    if (events.empty())
        return;

    const qint64 jobId = realtimeJobId_;
    bool sessionEnded  = false;

    for (const auto &event : events) {
        switch (event.kind) {
        case ::komai::rust::TranscriptionRealtimeEventKindFfi::Delta:
            emit realtimeDelta(jobId, QString::fromStdString(std::string(event.text)));
            break;
        case ::komai::rust::TranscriptionRealtimeEventKindFfi::Completed:
            // Server VAD may emit several of these per session; treat
            // this as "consolidate the current tentative range" and let
            // the session keep running. The composer ends the session
            // on `Closed`, not here.
            emit realtimeCompleted(jobId, QString::fromStdString(std::string(event.text)));
            break;
        case ::komai::rust::TranscriptionRealtimeEventKindFfi::Closed:
            komai::logging::ui()->info("Transcription: realtime jobId={} closed cleanly", jobId);
            emit realtimeClosed(jobId);
            sessionEnded = true;
            break;
        case ::komai::rust::TranscriptionRealtimeEventKindFfi::Failed:
            komai::logging::ui()->warn("Transcription: realtime jobId={} failed; code={} msg={}",
                                       jobId,
                                       static_cast<int>(event.error_code),
                                       std::string(event.error_message));
            emit realtimeFailed(jobId,
                                errorCodeToString(event.error_code),
                                QString::fromStdString(std::string(event.error_message)));
            sessionEnded = true;
            break;
        }

        // If the consumer cancelled while we were dispatching events
        // synchronously (a slot called cancelRealtime), bail out — any
        // further events for this job are stale and the registry has been
        // cleared on the Rust side.
        if (realtimeJobId_ != jobId)
            return;

        if (sessionEnded)
            break;
    }

    if (sessionEnded && realtimeJobId_ == jobId) {
        realtimeJobId_ = 0;
        if (realtimePollTimer_)
            realtimePollTimer_->stop();
    }
}

#include "moc_Transcription.cpp"
