// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QString>
#include <QStringView>

#include <memory>

#include <optional>

#include <yaml-cpp/yaml.h>

namespace spdlog {
class logger;
}

namespace settings::storage {

class ReaderWriter;
using ReaderWriterPtr = std::shared_ptr<ReaderWriter>;
struct StorageLoggers
{
    std::shared_ptr<spdlog::logger> ui;
    std::shared_ptr<spdlog::logger> db;
};

/**
 * Abstraction for settings persistence transport (filesystem or test-time memory).
 */
class ReaderWriter
{
public:
    virtual ~ReaderWriter() = default;

    virtual QString profileDirPath(const QString &profile) const                  = 0;
    virtual QString configFilePathForProfile(const QString &profile) const        = 0;
    virtual QString stateFilePathForProfile(const QString &profile) const         = 0;
    virtual QString sessionFilePathForProfile(const QString &profile) const       = 0;
    virtual QString secretsFilePathForProfile(const QString &profile) const       = 0;
    virtual YAML::Node loadYamlFile(const QString &path, const char *label) const = 0;
    virtual bool
    writeYamlFile(const QString &path, const YAML::Node &root, bool ownerReadWriteOnly) const = 0;
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
 * Load/serialize YAML settings files.
 */
YAML::Node
loadYamlFile(const QString &path, const char *label);
bool
writeYamlFile(const QString &path, const YAML::Node &root, bool ownerReadWriteOnly);

void
setReaderWriter(ReaderWriterPtr writer);
void
setLoggers(StorageLoggers loggers);
StorageLoggers
activeLoggers();

/**
 * Secure backend key helpers used by fallback and keyring-backed secret storage.
 */
QString
secureStoreKey(const QString &profile, const char *keyName);
std::optional<QString>
readSecureValue(const QString &key);
void
writeSecureValue(const QString &key, const QString &value);
void
deleteSecureValue(const QString &key);

QString
encodeSecretsMap(const QMap<QString, QString> &secrets);
QMap<QString, QString>
decodeSecretsMap(const QString &serialized);

} // namespace settings::storage
