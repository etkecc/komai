// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"
#include "SettingsStorageInternal.h"

#include "logging/Logging.h"

#include <string>
#include <string_view>

#include "profile/KeyringEnvironment.h"
#include "profile/ProfileId.h"

namespace settings::storage {

namespace {

StorageLoggers
defaultLoggers()
{
    return {
      .ui = std::make_shared<nhlog::Logger>("settings-ui"),
      .db = std::make_shared<nhlog::Logger>("settings-db"),
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
    return profile_id::normalized(profile);
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
    return keyring_environment::prefix() + normalizedProfileId(profile) +
           QStringLiteral(".settings.") + keyName.toString();
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
