// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"
#include "SettingsStorageInternal.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "logging/Logging.h"

#include "profile/Paths.h"

namespace settings::storage::detail {

namespace {

class FilesystemReaderWriter final : public ReaderWriter
{
public:
    QString profileDirPath(const QString &profile) const override
    {
        return QFileInfo(app_paths::config::profileConfigFile(profile)).absolutePath();
    }

    QString configFilePathForProfile(const QString &profile) const override
    {
        return app_paths::config::profileConfigFile(profile);
    }

    QString stateFilePathForProfile(const QString &profile) const override
    {
        return app_paths::config::profileStateFile(profile);
    }

    QString sessionFilePathForProfile(const QString &profile) const override
    {
        return app_paths::config::profileSessionFile(profile);
    }

    QString secretsFilePathForProfile(const QString &profile) const override
    {
        return app_paths::config::profileSecretsFile(profile);
    }

    QString readTextFile(const QString &path, const char *label) const override
    {
        QFile file(path);
        const char *safeLabel = label ? label : "settings";
        if (!file.exists()) {
            activeLoggers().ui->info(
              "{} file does not exist, using defaults: {}", safeLabel, path.toStdString());
            return {};
        }

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            activeLoggers().ui->error(
              "Failed to read {} file {} as text", safeLabel, path.toStdString());
            return {};
        }

        const auto contents = QString::fromUtf8(file.readAll());
        activeLoggers().ui->info("Loaded {} from: {}", safeLabel, path.toStdString());
        return contents;
    }

    bool writeTextFile(const QString &path,
                       const QString &content,
                       bool ownerReadWriteOnly) const override
    {
        const auto dir = QFileInfo(path).absolutePath();
        if (!QDir().mkpath(dir)) {
            activeLoggers().ui->error("Failed to create settings directory: {}", dir.toStdString());
            return false;
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            activeLoggers().ui->error("Failed to write settings file: {}", path.toStdString());
            return false;
        }

        if (file.write(content.toUtf8()) < 0) {
            activeLoggers().ui->error("Failed to write settings file: {}", path.toStdString());
            return false;
        }
        file.close();

        if (ownerReadWriteOnly) {
            if (!QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
                activeLoggers().ui->warn("Failed to restrict permissions for {}",
                                         path.toStdString());
            }
        }

        return true;
    }

    bool pathExists(const QString &path) const override { return QFileInfo::exists(path); }

    bool createDir(const QString &path) const override { return QDir().mkpath(path); }

    bool removePath(const QString &path) const override
    {
        if (!QFileInfo::exists(path))
            return true;
        return QFile::remove(path);
    }
};

} // namespace

ReaderWriterPtr
makeFilesystemReaderWriter()
{
    return std::make_shared<FilesystemReaderWriter>();
}

} // namespace settings::storage::detail
