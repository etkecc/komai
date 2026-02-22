// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <fstream>

#if __has_include(<keychain.h>)
#include <keychain.h>
#else
#include <qt6keychain/keychain.h>
#endif

#include "Logging.h"
#include "Paths.h"

namespace settings::storage {

namespace {

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
      QCryptographicHash::hash(normalizedProfileId(profile).toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString
settingsSecretStoreKey(QStringView profile, QStringView keyName)
{
    return QStringLiteral("komai.") + profileHashHex(profile) + QStringLiteral(".settings.") +
           keyName.toString();
}

const QString keychainServiceName()
{
    return QCoreApplication::applicationName();
}

} // namespace

QString
profileDirPath(const QString &profile)
{
    return QFileInfo(app_paths::config::profileConfigFile(profile)).absolutePath();
}

QString
configFilePathForProfile(const QString &profile)
{
    return app_paths::config::profileConfigFile(profile);
}

QString
stateFilePathForProfile(const QString &profile)
{
    return app_paths::config::profileStateFile(profile);
}

QString
sessionFilePathForProfile(const QString &profile)
{
    return app_paths::config::profileSessionFile(profile);
}

QString
secretsFilePathForProfile(const QString &profile)
{
    return app_paths::config::profileSecretsFile(profile);
}

YAML::Node
loadYamlFile(const QString &path, const char *label)
{
    QFileInfo info(path);
    if (!info.exists()) {
        nhlog::ui()->info("{} file does not exist, using defaults: {}", label, path.toStdString());
        return YAML::Node(YAML::NodeType::Map);
    }

    try {
        auto root = YAML::LoadFile(path.toStdString());
        nhlog::ui()->info("Loaded {} from: {}", label, path.toStdString());
        return root.IsMap() ? root : YAML::Node(YAML::NodeType::Map);
    } catch (const YAML::Exception &e) {
        nhlog::ui()->error("Failed to parse {} file {}: {}", label, path.toStdString(), e.what());
        return YAML::Node(YAML::NodeType::Map);
    }
}

bool
writeYamlFile(const QString &path, const YAML::Node &root, bool ownerReadWriteOnly)
{
    auto dir = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir)) {
        nhlog::ui()->error("Failed to create settings directory: {}", dir.toStdString());
        return false;
    }

    YAML::Emitter out;
    out.SetIndent(2);
    out << (root && root.IsMap() ? root : YAML::Node(YAML::NodeType::Map));

    std::ofstream file(path.toStdString());
    if (!file.is_open()) {
        nhlog::ui()->error("Failed to write settings file: {}", path.toStdString());
        return false;
    }
    file << out.c_str();
    file.close();

    if (ownerReadWriteOnly) {
        if (!QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
            nhlog::ui()->warn("Failed to restrict permissions for {}", path.toStdString());
        }
    }

    return true;
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
        nhlog::db()->warn("Failed to read secret '{}' from secure backend: {}",
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
                  nhlog::db()->warn("Failed to write secret '{}' to secure backend: {}",
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
                  nhlog::db()->warn("Failed to delete secret '{}' from secure backend: {}",
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
