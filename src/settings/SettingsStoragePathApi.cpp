// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"
#include "SettingsStorageInternal.h"

namespace settings::storage {

QString
profileDirPath(const QString &profile)
{
    return detail::defaultReaderWriter().profileDirPath(profile);
}

QString
configFilePathForProfile(const QString &profile)
{
    return detail::defaultReaderWriter().configFilePathForProfile(profile);
}

QString
stateFilePathForProfile(const QString &profile)
{
    return detail::defaultReaderWriter().stateFilePathForProfile(profile);
}

QString
sessionFilePathForProfile(const QString &profile)
{
    return detail::defaultReaderWriter().sessionFilePathForProfile(profile);
}

QString
secretsFilePathForProfile(const QString &profile)
{
    return detail::defaultReaderWriter().secretsFilePathForProfile(profile);
}

bool
pathExists(const QString &path)
{
    return detail::defaultReaderWriter().pathExists(path);
}

bool
createDir(const QString &path)
{
    return detail::defaultReaderWriter().createDir(path);
}

bool
removePath(const QString &path)
{
    return detail::defaultReaderWriter().removePath(path);
}

QString
readTextFile(const QString &path, const char *label)
{
    return detail::defaultReaderWriter().readTextFile(path, label);
}

YAML::Node
loadYamlFile(const QString &path, const char *label)
{
    return detail::defaultReaderWriter().loadYamlFile(path, label);
}

bool
writeYamlFile(const QString &path, const YAML::Node &root, bool ownerReadWriteOnly)
{
    return detail::defaultReaderWriter().writeYamlFile(path, root, ownerReadWriteOnly);
}

QString
secureStoreKey(const QString &profile, const char *keyName)
{
    return detail::settingsSecretStoreKey(profile, QString::fromLatin1(keyName));
}

} // namespace settings::storage
