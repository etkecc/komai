// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"
#include "SettingsStorageInternal.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <fstream>
#include <spdlog/logger.h>

#include "Paths.h"

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

    YAML::Node loadYamlFile(const QString &path, const char *label) const override
    {
        QFileInfo info(path);
        const char *safeLabel = label ? label : "settings";
        if (!info.exists()) {
            activeLoggers().ui->info(
              "{} file does not exist, using defaults: {}", safeLabel, path.toStdString());
            return YAML::Node(YAML::NodeType::Map);
        }

        try {
            auto root = YAML::LoadFile(path.toStdString());
            activeLoggers().ui->info("Loaded {} from: {}", safeLabel, path.toStdString());
            return root.IsMap() ? root : YAML::Node(YAML::NodeType::Map);
        } catch (const YAML::Exception &e) {
            activeLoggers().ui->error(
              "Failed to parse {} file {}: {}", safeLabel, path.toStdString(), e.what());
            return YAML::Node(YAML::NodeType::Map);
        }
    }

    bool writeYamlFile(const QString &path,
                       const YAML::Node &root,
                       bool ownerReadWriteOnly) const override
    {
        const auto dir = QFileInfo(path).absolutePath();
        if (!QDir().mkpath(dir)) {
            activeLoggers().ui->error("Failed to create settings directory: {}", dir.toStdString());
            return false;
        }

        YAML::Emitter out;
        out.SetIndent(2);
        out << (root && root.IsMap() ? root : YAML::Node(YAML::NodeType::Map));

        std::ofstream file(path.toStdString());
        if (!file.is_open()) {
            activeLoggers().ui->error("Failed to write settings file: {}", path.toStdString());
            return false;
        }
        file << out.c_str();
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
