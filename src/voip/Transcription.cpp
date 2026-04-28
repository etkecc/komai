// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "voip/Transcription.h"

#include <QFutureWatcher>
#include <QPointer>
#include <QThreadPool>
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

Transcription::~Transcription() = default;

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
            emit self->batchFinished(jobId, QString::fromStdString(std::string(result.text)));
        } else {
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

#include "moc_Transcription.cpp"
