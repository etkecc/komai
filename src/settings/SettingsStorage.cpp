// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"
#include "SettingsStorageInternal.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>

#include <fstream>
#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>

#include <string>
#include <string_view>

#include "Paths.h"

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

class FilesystemReaderWriter : public ReaderWriter
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

class InMemoryReaderWriter final : public ReaderWriter
{
public:
    explicit InMemoryReaderWriter(QStringView baseDir)
      : baseDir_(QString::fromUtf8(baseDir.toUtf8()))
    {
    }

    QString profileDirPath(const QString &profile) const override
    {
        return profileBasePath(profile);
    }

    QString configFilePathForProfile(const QString &profile) const override
    {
        return profileBasePath(profile) + QStringLiteral("/config.yml");
    }

    QString stateFilePathForProfile(const QString &profile) const override
    {
        return profileBasePath(profile) + QStringLiteral("/state.yml");
    }

    QString sessionFilePathForProfile(const QString &profile) const override
    {
        return profileBasePath(profile) + QStringLiteral("/session.yml");
    }

    QString secretsFilePathForProfile(const QString &profile) const override
    {
        return profileBasePath(profile) + QStringLiteral("/secrets.yml");
    }

    YAML::Node loadYamlFile(const QString &path, const char *label) const override
    {
        Q_UNUSED(label)

        const auto it = nodes_.find(path);
        if (it == nodes_.end()) {
            const char *safeLabel = label ? label : "settings";
            activeLoggers().ui->info(
              "{} file does not exist, using defaults: {}", safeLabel, path.toStdString());
            return YAML::Node(YAML::NodeType::Map);
        }
        return it.value();
    }

    bool writeYamlFile(const QString &path, const YAML::Node &root, bool) const override
    {
        nodes_[path] = root;
        return true;
    }

    bool pathExists(const QString &path) const override { return nodes_.contains(path); }

    bool createDir(const QString &path) const override
    {
        Q_UNUSED(path)
        return true;
    }

    bool removePath(const QString &path) const override { return nodes_.remove(path) > 0; }

private:
    QString profileBasePath(const QString &profile) const
    {
        return baseDir_ + QStringLiteral("/profiles/") + app_paths::normalizedProfileId(profile);
    }

    QString baseDir_;
    mutable QHash<QString, YAML::Node> nodes_;
};

ReaderWriterPtr &
currentReaderWriter()
{
    static ReaderWriterPtr activeWriter = std::make_shared<FilesystemReaderWriter>();
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
    return std::make_shared<InMemoryReaderWriter>(baseDir);
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
