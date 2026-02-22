// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QString>

#include <optional>

#include <yaml-cpp/yaml.h>

namespace settings::storage {

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

/**
 * Load/serialize YAML settings files.
 */
YAML::Node
loadYamlFile(const QString &path, const char *label);
bool
writeYamlFile(const QString &path, const YAML::Node &root, bool ownerReadWriteOnly);

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
