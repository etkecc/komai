// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string_view>

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QTemporaryDir>

#include <memory>
#include <yaml-cpp/yaml.h>

#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>

#include "settings/SettingsPersistence.h"
#include "settings/SettingsStorage.h"
#include "CacheApiWrappers.h"

namespace {

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool
testYamlRoundtrip()
{
    bool ok = true;
    const QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return expect(false, "temporary directory is valid");
    }

    const auto filePath = tempDir.path() + QStringLiteral("/settings.yml");
    YAML::Node root(YAML::NodeType::Map);
    root["ui"]["motion"]["enable_animations"] = true;
    root["sidebars"]["room_list"]["width_px"] = 42;

    ok &= expect(settings::storage::writeYamlFile(filePath, root, false), "writeYamlFile persists map");

    const auto read = settings::storage::loadYamlFile(filePath, "settings-test");
    ok &= expect(read["ui"]["motion"]["enable_animations"].as<bool>() ==
                 root["ui"]["motion"]["enable_animations"].as<bool>(),
                 "read back bool from written YAML");
    ok &= expect(read["sidebars"]["room_list"]["width_px"].as<int>() ==
                 root["sidebars"]["room_list"]["width_px"].as<int>(),
                 "read back integer from written YAML");

    return ok;
}

bool
testMissingAndInvalidFiles()
{
    bool ok = true;

    const QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return expect(false, "temporary directory is valid");
    }

    const auto missingPath = tempDir.path() + QStringLiteral("/missing.yml");
    const auto missingRoot = settings::storage::loadYamlFile(missingPath, "missing");
    ok &= expect(missingRoot.IsMap(), "loadYamlFile returns map for missing file");
    ok &= expect(missingRoot.size() == 0, "missing file load returns empty map");

    const auto invalidPath = tempDir.path() + QStringLiteral("/invalid.yml");
    {
        QFile invalidFile{invalidPath};
        if (!invalidFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return expect(false, "failed to create invalid YAML fixture file");
        }
        invalidFile.write("ui: [\n");
        invalidFile.close();
    }

    const auto invalidRoot = settings::storage::loadYamlFile(invalidPath, "invalid");
    ok &= expect(invalidRoot.IsMap(), "loadYamlFile returns map for invalid YAML");
    ok &= expect(invalidRoot.size() == 0, "invalid YAML load falls back to empty map");

    return ok;
}

bool
testSecretsMapSerialization()
{
    bool ok = true;

    const QMap<QString, QString> source{{"access-token", "abcd"}, {"device-id", "dev123"}};
    const auto packed = settings::storage::encodeSecretsMap(source);
    const auto unpacked = settings::storage::decodeSecretsMap(packed);

    ok &= expect(unpacked == source, "secrets map encode/decode roundtrip");
    ok &= expect(settings::storage::decodeSecretsMap(QString{}).isEmpty(),
                 "decodeSecretsMap returns empty for blank input");
    ok &= expect(settings::storage::decodeSecretsMap(QStringLiteral("not: [yaml")).isEmpty(),
                 "decodeSecretsMap returns empty for parse errors");

    return ok;
}

bool
testPathHelpers()
{
    bool ok = true;

    const QString profile = QStringLiteral("profile-123");
    const auto profileDir = settings::storage::profileDirPath(profile);
    const auto configPath = settings::storage::configFilePathForProfile(profile);
    const auto statePath = settings::storage::stateFilePathForProfile(profile);
    const auto sessionPath = settings::storage::sessionFilePathForProfile(profile);
    const auto secretsPath = settings::storage::secretsFilePathForProfile(profile);

    ok &= expect(!profileDir.isEmpty(), "profile directory path is not empty");
    ok &= expect(profileDir.contains(profile), "profile directory path includes profile id");
    ok &= expect(configPath.endsWith(QStringLiteral("config.yml")), "config path ends with config.yml");
    ok &= expect(statePath.endsWith(QStringLiteral("state.yml")), "state path ends with state.yml");
    ok &= expect(sessionPath.endsWith(QStringLiteral("session.yml")), "session path ends with session.yml");
    ok &= expect(secretsPath.endsWith(QStringLiteral("secrets.yml")), "secrets path ends with secrets.yml");
    ok &= expect(QFileInfo(configPath).dir().absolutePath() == profileDir, "config path dir is profile dir");

    return ok;
}

bool
testLoggerInjectionNullAndInjectedLoggers()
{
    bool ok = true;

    settings::storage::setLoggers({});
    auto current = settings::storage::activeLoggers();
    ok &= expect(!current.ui && !current.db, "settings storage accepts null-injected loggers");

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return expect(false, "temporary directory for logger smoke test is valid");
    const auto file = tempDir.path() + QStringLiteral("/config.yml");
    YAML::Node root(YAML::NodeType::Map);
    root["ui"]["motion"]["enable_animations"] = false;
    ok &= expect(settings::storage::writeYamlFile(file, root, false),
                 "settings storage can write with null logger");
    ok &= expect(settings::storage::loadYamlFile(file, "settings-test").IsMap(),
                 "settings storage can read with null logger");

    auto sharedSink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto uiLogger   = std::make_shared<spdlog::logger>(QStringLiteral("test-ui").toStdString(),
                                                       std::move(sharedSink));
    auto dbLogger   = std::make_shared<spdlog::logger>(QStringLiteral("test-db").toStdString(),
                                                       std::move(std::make_shared<spdlog::sinks::null_sink_mt>()));
    settings::storage::setLoggers({.ui = uiLogger, .db = dbLogger});
    current = settings::storage::activeLoggers();
    ok &= expect(current.ui == uiLogger, "settings storage stores injected ui logger");
    ok &= expect(current.db == dbLogger, "settings storage stores injected db logger");

    ok &= expect(settings::storage::loadYamlFile(file, "settings-test").IsMap(),
                 "settings storage can read with injected logger");

    return ok;
}

bool
testCacheLoggerInjection()
{
    bool ok = true;

    cache::setLoggers({});
    auto cacheLoggers = cache::activeLoggers();
    ok &= expect(!cacheLoggers.db, "cache wrappers accept null-injected logger");

    auto logger = std::make_shared<spdlog::logger>(
      QStringLiteral("cache-test").toStdString(), std::make_shared<spdlog::sinks::null_sink_mt>());
    cache::setLoggers({.db = logger});
    cacheLoggers = cache::activeLoggers();
    ok &= expect(cacheLoggers.db == logger, "cache wrappers store injected db logger");

    return ok;
}

bool
testInMemoryReaderWriterOverride()
{
    bool ok = true;

    const auto writer = settings::storage::inMemoryReaderWriter(QStringLiteral("/tmp/komai-test-storage-reader"));
    settings::storage::ReaderWriterOverride writerOverride{writer};

    const QString profile = QStringLiteral("readerwriter");
    const auto configPath = settings::storage::configFilePathForProfile(profile);
    const auto statePath  = settings::storage::stateFilePathForProfile(profile);

    YAML::Node root(YAML::NodeType::Map);
    root["integrations"]["dbus"]["access"] = 2;
    ok &= expect(settings::storage::writeYamlFile(configPath, root, false),
                "in-memory writer can persist YAML nodes");
    const auto loaded = settings::storage::loadYamlFile(configPath, "readerwriter-config");
    ok &= expect(
      loaded["integrations"]["dbus"]["access"].as<int>() == 2, "in-memory writer stores written data");

    ok &= expect(settings::storage::pathExists(configPath), "in-memory writer reports existing paths");
    settings::storage::removePath(configPath);
    ok &= expect(!settings::storage::pathExists(configPath), "in-memory writer removes paths");

    ok &= expect(settings::storage::createDir(QStringLiteral("/tmp/irrelevant")),
                "in-memory writer tolerates createDir calls");
    ok &= expect(!settings::storage::pathExists(statePath),
                  "state file still does not exist in in-memory backend");

    return ok;
}

bool
testProviderSelectionHonorsConfigAndOverrides()
{
    YAML::Node root(YAML::NodeType::Map);
    root["secrets"]["provider"] = staged_load_plan::ProviderSecretServiceValue;
    auto fromConfig = settings::persistence::providerFromConfig(root, false);
    auto defaultSecretService =
      expect(fromConfig == staged_load_plan::SecretsProvider::SecretService,
             "secret provider defaults to secret_service");
    const bool explicitFile = expect(settings::persistence::providerFromConfig(root, true) ==
                                      staged_load_plan::SecretsProvider::File,
                                    "forced file provider override still returns file");

    root["secrets"]["provider"] = staged_load_plan::ProviderFileValue;
    const bool explicitFileConfig = expect(
      settings::persistence::providerFromConfig(root, false) ==
        staged_load_plan::SecretsProvider::File,
      "file provider is honored from config");

    return defaultSecretService && explicitFile && explicitFileConfig;
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testYamlRoundtrip();
    ok &= testMissingAndInvalidFiles();
    ok &= testSecretsMapSerialization();
    ok &= testPathHelpers();
    ok &= testLoggerInjectionNullAndInjectedLoggers();
    ok &= testCacheLoggerInjection();
    ok &= testProviderSelectionHonorsConfigAndOverrides();
    ok &= testInMemoryReaderWriterOverride();

    return ok ? 0 : 1;
}
