// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTimer>

#include <fstream>
#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>

#include <string>
#include <string_view>

#if __has_include(<keychain.h>)
#include <keychain.h>
#else
#include <qt6keychain/keychain.h>
#endif

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

QString
settingsSecretStoreKey(QStringView profile, QStringView keyName)
{
    return QStringLiteral("komai.") + profileHashHex(profile) + QStringLiteral(".settings.") +
           keyName.toString();
}

const QString
keychainServiceName()
{
    return QCoreApplication::applicationName();
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

ReaderWriter &
defaultReaderWriter()
{
    return *currentReaderWriter();
}

} // namespace

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

QString
profileDirPath(const QString &profile)
{
    return defaultReaderWriter().profileDirPath(profile);
}

QString
configFilePathForProfile(const QString &profile)
{
    return defaultReaderWriter().configFilePathForProfile(profile);
}

QString
stateFilePathForProfile(const QString &profile)
{
    return defaultReaderWriter().stateFilePathForProfile(profile);
}

QString
sessionFilePathForProfile(const QString &profile)
{
    return defaultReaderWriter().sessionFilePathForProfile(profile);
}

QString
secretsFilePathForProfile(const QString &profile)
{
    return defaultReaderWriter().secretsFilePathForProfile(profile);
}

bool
pathExists(const QString &path)
{
    return defaultReaderWriter().pathExists(path);
}

bool
createDir(const QString &path)
{
    return defaultReaderWriter().createDir(path);
}

bool
removePath(const QString &path)
{
    return defaultReaderWriter().removePath(path);
}

YAML::Node
loadYamlFile(const QString &path, const char *label)
{
    return defaultReaderWriter().loadYamlFile(path, label);
}

bool
writeYamlFile(const QString &path, const YAML::Node &root, bool ownerReadWriteOnly)
{
    return defaultReaderWriter().writeYamlFile(path, root, ownerReadWriteOnly);
}

QString
secureStoreKey(const QString &profile, const char *keyName)
{
    return settingsSecretStoreKey(profile, QString::fromLatin1(keyName));
}

std::optional<QString>
readSecureValue(const QString &key)
{
    QEventLoop loop;
    auto job = std::make_unique<QKeychain::ReadPasswordJob>(keychainServiceName());
    job->setAutoDelete(false);
    job->setInsecureFallback(false);
    job->setKey(key);
    QObject::connect(job.get(), &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job->start();
    loop.exec();

    if (job->error() == QKeychain::Error::NoError)
        return job->textData();

    if (job->error() != QKeychain::Error::EntryNotFound) {
        activeLoggers().db->warn("Failed to read secret '{}' from secure backend: {}",
                                 key.toStdString(),
                                 static_cast<int>(job->error()));
    }
    return std::nullopt;
}

void
writeSecureValue(const QString &key, const QString &value)
{
    QTimer::singleShot(0, QCoreApplication::instance(), [key, value] {
        auto *job = new QKeychain::WritePasswordJob(QCoreApplication::applicationName());
        job->setAutoDelete(true);
        job->setInsecureFallback(false);
        job->setKey(key);
        job->setTextData(value);
        QObject::connect(
          job, &QKeychain::WritePasswordJob::finished, job, [key](QKeychain::Job *j) {
              if (j->error() != QKeychain::Error::NoError) {
                  activeLoggers().db->warn("Failed to write secret '{}' to secure backend: {}",
                                           key.toStdString(),
                                           static_cast<int>(j->error()));
              }
          });
        job->start();
    });
}

void
deleteSecureValue(const QString &key)
{
    QTimer::singleShot(0, QCoreApplication::instance(), [key] {
        auto *job = new QKeychain::DeletePasswordJob(QCoreApplication::applicationName());
        job->setAutoDelete(true);
        job->setInsecureFallback(false);
        job->setKey(key);
        QObject::connect(
          job, &QKeychain::DeletePasswordJob::finished, job, [key](QKeychain::Job *j) {
              if (j->error() != QKeychain::Error::NoError &&
                  j->error() != QKeychain::Error::EntryNotFound) {
                  activeLoggers().db->warn("Failed to delete secret '{}' from secure backend: {}",
                                           key.toStdString(),
                                           static_cast<int>(j->error()));
              }
          });
        job->start();
    });
}

QString
encodeSecretsMap(const QMap<QString, QString> &secrets)
{
    YAML::Node root(YAML::NodeType::Map);
    for (auto it = secrets.constBegin(); it != secrets.constEnd(); ++it)
        root[it.key().toStdString()] = it.value().toStdString();

    YAML::Emitter out;
    out << root;
    return QString::fromStdString(out.c_str());
}

QMap<QString, QString>
decodeSecretsMap(const QString &serialized)
{
    if (serialized.trimmed().isEmpty())
        return {};

    try {
        YAML::Node root = YAML::Load(serialized.toStdString());
        if (!root.IsMap())
            return {};

        QMap<QString, QString> result;
        for (const auto &item : root) {
            if (!item.first.IsScalar() || !item.second.IsScalar())
                continue;
            result[QString::fromStdString(item.first.as<std::string>())] =
              QString::fromStdString(item.second.as<std::string>());
        }
        return result;
    } catch (const YAML::Exception &) {
        return {};
    }
}

} // namespace settings::storage
