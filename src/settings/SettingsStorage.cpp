// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"
#include "SettingsStorageInternal.h"

#include <QCryptographicHash>
#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>

#include <string>
#include <string_view>

namespace settings::storage {

namespace {

std::shared_ptr<spdlog::logger>
nullLogger(std::string_view name)
{
    static auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    static auto settingsUiLogger =
      std::make_shared<spdlog::logger>(std::string("settings-ui"), sink);
    static auto settingsDbLogger =
      std::make_shared<spdlog::logger>(std::string("settings-db"), sink);

    if (name == "settings-ui")
        return settingsUiLogger;
    if (name == "settings-db")
        return settingsDbLogger;
    return settingsUiLogger;
}

StorageLoggers
defaultLoggers()
{
    return {
      .ui = nullLogger("settings-ui"),
      .db = nullLogger("settings-db"),
    };
}

StorageLoggers &
currentLoggers()
{
    static StorageLoggers loggers = defaultLoggers();
    return loggers;
}

QString
normalizedProfileId(QStringView profile)
{
    if (profile.isEmpty() || profile == u"default")
        return QStringLiteral("default");
    return profile.toString();
}

QString
profileHashHex(QStringView profile)
{
    return QString::fromLatin1(
      QCryptographicHash::hash(normalizedProfileId(profile).toUtf8(), QCryptographicHash::Sha256)
        .toHex());
}

ReaderWriterPtr &
currentReaderWriter()
{
    static ReaderWriterPtr activeWriter = detail::makeFilesystemReaderWriter();
    return activeWriter;
}

} // namespace

QString
detail::settingsSecretStoreKey(QStringView profile, QStringView keyName)
{
    return QStringLiteral("komai.") + profileHashHex(profile) + QStringLiteral(".settings.") +
           keyName.toString();
}

ReaderWriter &
detail::defaultReaderWriter()
{
    return *currentReaderWriter();
}

ReaderWriterPtr
inMemoryReaderWriter(QStringView baseDir)
{
    return detail::makeInMemoryReaderWriter(baseDir);
}

ReaderWriterOverride::ReaderWriterOverride(ReaderWriterPtr newWriter)
  : previousWriter_(currentReaderWriter())
{
    setReaderWriter(std::move(newWriter));
}

ReaderWriterOverride::~ReaderWriterOverride()
{
    setReaderWriter(previousWriter_);
}

void
setReaderWriter(ReaderWriterPtr writer)
{
    if (!writer)
        return;

    auto &global = currentReaderWriter();
    global       = std::move(writer);
}

void
setLoggers(StorageLoggers loggers)
{
    const auto &defaults = defaultLoggers();
    if (!loggers.ui)
        loggers.ui = defaults.ui;
    if (!loggers.db)
        loggers.db = defaults.db;
    currentLoggers() = std::move(loggers);
}

const StorageLoggers &
activeLoggers()
{
    return currentLoggers();
}

} // namespace settings::storage
