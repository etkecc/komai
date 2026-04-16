// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringView>
#include <functional>

#include <memory>

#include <optional>

namespace komai::logging {
class Logger;
}

namespace settings::storage {

class ReaderWriter;
using ReaderWriterPtr = std::shared_ptr<ReaderWriter>;
struct StorageLoggers
{
    std::shared_ptr<komai::logging::Logger> ui;
    std::shared_ptr<komai::logging::Logger> db;
};

enum class SecureBackendJobStatus
{
    Success,
    EntryNotFound,
    Error,
};

struct SecureBackendJobResult
{
    SecureBackendJobStatus status = SecureBackendJobStatus::Error;
    QString value;
    int errorCode = 0;
    QString errorString;

    [[nodiscard]] bool ok() const { return status == SecureBackendJobStatus::Success; }
    [[nodiscard]] bool missing() const { return status == SecureBackendJobStatus::EntryNotFound; }
    [[nodiscard]] bool failed() const { return status == SecureBackendJobStatus::Error; }
};

/**
 * Abstraction for settings persistence transport (filesystem or test-time memory).
 */
class ReaderWriter
{
public:
    virtual ~ReaderWriter() = default;

    virtual QString profileDirPath(const QString &profile) const               = 0;
    virtual QString configFilePathForProfile(const QString &profile) const     = 0;
    virtual QString stateFilePathForProfile(const QString &profile) const      = 0;
    virtual QString sessionFilePathForProfile(const QString &profile) const    = 0;
    virtual QString secretsFilePathForProfile(const QString &profile) const    = 0;
    virtual QString readTextFile(const QString &path, const char *label) const = 0;
    virtual bool
    writeTextFile(const QString &path, const QString &content, bool ownerReadWriteOnly) const = 0;
    virtual bool pathExists(const QString &path) const                                        = 0;
    virtual bool createDir(const QString &path) const                                         = 0;
    virtual bool removePath(const QString &path) const                                        = 0;
};

ReaderWriterPtr
inMemoryReaderWriter(QStringView baseDir = QStringLiteral("/tmp/komai-test-settings"));

class ReaderWriterOverride
{
public:
    explicit ReaderWriterOverride(ReaderWriterPtr newWriter);
    ~ReaderWriterOverride();

private:
    ReaderWriterPtr previousWriter_;
};

/**
 * File-system and secure-storage helpers for settings persistence.
 */

QString
profileDirPath(const QString &profile);
/**
 * Build profile-scoped settings file paths.
 */
QString
configFilePathForProfile(const QString &profile);
QString
stateFilePathForProfile(const QString &profile);
QString
sessionFilePathForProfile(const QString &profile);
QString
secretsFilePathForProfile(const QString &profile);

bool
pathExists(const QString &path);
bool
createDir(const QString &path);
bool
removePath(const QString &path);

/**
 * Load raw settings file contents.
 */
QString
readTextFile(const QString &path, const char *label);
bool
writeTextFile(const QString &path, const QString &content, bool ownerReadWriteOnly);

void
setReaderWriter(ReaderWriterPtr writer);
void
setLoggers(StorageLoggers loggers);
const StorageLoggers &
activeLoggers();

/**
 * Secure backend key helpers used by fallback and keyring-backed secret storage.
 */
QString
secureStoreKey(const QString &profile, const char *keyName);
SecureBackendJobResult
readSecureValueResult(const QString &key);
std::optional<QString>
readSecureValue(const QString &key);
void
readSecureValueAsync(const QString &key,
                     QObject *receiver,
                     std::function<void(const SecureBackendJobResult &)> callback);
void
writeSecureValue(const QString &key, const QString &value);
SecureBackendJobResult
writeSecureValueResultBlocking(const QString &key, const QString &value);
void
writeSecureValueAsync(const QString &key,
                      const QString &value,
                      QObject *receiver,
                      std::function<void(const SecureBackendJobResult &)> callback);
bool
writeSecureValueBlocking(const QString &key, const QString &value);
void
deleteSecureValue(const QString &key);
SecureBackendJobResult
deleteSecureValueResultBlocking(const QString &key);
void
deleteSecureValueAsync(const QString &key,
                       QObject *receiver,
                       std::function<void(const SecureBackendJobResult &)> callback);
bool
deleteSecureValueBlocking(const QString &key);
/**
 * Probe whether the secure backend is currently usable in this environment.
 *
 * This helper is used for startup-time provider selection before any session
 * exists. It prefers secure backend usage when available and avoids silent
 * fallback for established sessions.
 */
bool
isSecureBackendAvailable();

} // namespace settings::storage
