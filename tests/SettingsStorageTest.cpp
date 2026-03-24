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

#include "profile/KeyringEnvironment.h"
#include "profile/Paths.h"
#include "profile/ProfileId.h"
#include "profile/ProfileSecrets.h"
#include "matrix/backend/MatrixSessionSecrets.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsStorage.h"
#include "cache/api/CacheApiContext.h"
#include "TestEnvironment.h"

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

    const auto dbPath =
      app_paths::data::databaseDirectory(QStringLiteral("@username:example.com/path"), profile);
    ok &= expect(dbPath.endsWith(
                   QStringLiteral("/db/@username%3Aexample.com%2Fpath")),
                 "database path escapes user id with %hh encoding");

    return ok;
}

bool
testProfileIdNormalizationAndSecretKeyIds()
{
    bool ok = true;

    ok &= expect(app_paths::normalizedProfileId(QStringLiteral("")) == QStringLiteral("default"),
                 "empty profile id normalizes to default");
    ok &= expect(app_paths::normalizedProfileId(QStringLiteral("default")) ==
                   QStringLiteral("default"),
                 "default profile id stays default");
    ok &= expect(app_paths::normalizedProfileId(QStringLiteral("etke")) == QStringLiteral("etke"),
                 "custom profile id stays unchanged");

    const auto emptyProfileKey =
      settings::storage::secureStoreKey(QStringLiteral(""), "session.secrets");
    const auto defaultProfileKey =
      settings::storage::secureStoreKey(QStringLiteral("default"), "session.secrets");
    const auto customProfileKey =
      settings::storage::secureStoreKey(QStringLiteral("etke"), "session.secrets");

    const auto &kp = keyring_environment::prefix();
    ok &= expect(emptyProfileKey == kp + QStringLiteral("default.settings.session.secrets"),
                 "secure key for empty profile uses normalized default id");
    ok &= expect(defaultProfileKey == kp + QStringLiteral("default.settings.session.secrets"),
                 "secure key for explicit default uses normalized default id");
    ok &= expect(customProfileKey == kp + QStringLiteral("etke.settings.session.secrets"),
                 "secure key uses custom normalized profile id");
    ok &= expect(customProfileKey ==
                   profile_secrets::settingsSecretStoreKey(QStringLiteral("etke"),
                                                           QStringLiteral("session.secrets")),
                 "settings and profile-secrets key builders stay aligned");

    return ok;
}

bool
testProfileIdValidation()
{
    bool ok = true;

    ok &= expect(!profile_id::validate(QStringLiteral("")).has_value(),
                 "empty profile id is allowed and maps to default");
    ok &= expect(!profile_id::validate(QStringLiteral("default")).has_value(),
                 "default profile id is valid");
    ok &= expect(!profile_id::validate(QStringLiteral("test8.1")).has_value(),
                 "dot-separated ASCII profile id is valid");
    ok &= expect(!profile_id::validate(QStringLiteral("etke_cc-1")).has_value(),
                 "underscore and dash profile id is valid");

    ok &= expect(profile_id::validate(QStringLiteral("../komai/.")).has_value(),
                 "path traversal style profile id is rejected");
    ok &= expect(profile_id::validate(QStringLiteral("кирилица")).has_value(),
                 "cyrillic profile id is rejected");
    ok &= expect(profile_id::validate(QStringLiteral("こまい")).has_value(),
                 "japanese profile id is rejected");
    ok &= expect(profile_id::validate(QStringLiteral("line\nbreak")).has_value(),
                 "profile id with newline is rejected");
    ok &= expect(profile_id::validate(QStringLiteral("a/b")).has_value(),
                 "profile id with path separator is rejected");
    ok &= expect(profile_id::validate(QStringLiteral("a\\b")).has_value(),
                 "profile id with backslash is rejected");
    ok &= expect(profile_id::validate(QStringLiteral(".hidden")).has_value(),
                 "profile id starting with dot is rejected");
    ok &= expect(profile_id::validate(QStringLiteral("trailing.")).has_value(),
                 "profile id ending with dot is rejected");

    return ok;
}

bool
testLoggerInjectionNullAndInjectedLoggers()
{
    bool ok = true;

    settings::storage::setLoggers({});
    auto current = settings::storage::activeLoggers();
    ok &= expect(current.ui && current.db,
                 "settings storage defaults to injected null logger values");

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
    ok &= expect(cacheLoggers.db && cacheLoggers.crypto && cacheLoggers.net,
                 "cache wrappers default missing loggers when none are injected");

    auto logger = std::make_shared<spdlog::logger>(
      QStringLiteral("cache-test").toStdString(), std::make_shared<spdlog::sinks::null_sink_mt>());
    cache::setLoggers({.db = logger, .crypto = logger, .net = logger});
    cacheLoggers = cache::activeLoggers();
    ok &= expect(cacheLoggers.db == logger, "cache wrappers store injected db logger");
    ok &= expect(cacheLoggers.crypto == logger, "cache wrappers store injected crypto logger");
    ok &= expect(cacheLoggers.net == logger, "cache wrappers store injected net logger");

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
testKeyringEnvironmentTagResolution()
{
    bool ok = true;

    // Native (non-sandboxed) Linux path produces "native" tag
    ok &= expect(keyring_environment::tagForConfigRoot(
                   QStringLiteral("/home/user/.config/komai")) == QStringLiteral("native"),
                 "native config path produces 'native' tag");

    // Flatpak-sandboxed path produces "flatpak" tag
    ok &= expect(keyring_environment::tagForConfigRoot(
                   QStringLiteral("/home/user/.var/app/cc.etke.komai/config/komai")) ==
                   QStringLiteral("flatpak"),
                 "flatpak config path produces 'flatpak' tag");

    // Snap path produces "snap" tag (contains /snap/ and ends with /.config/komai)
    ok &= expect(keyring_environment::tagForConfigRoot(
                   QStringLiteral("/home/user/snap/komai/current/.config/komai")) ==
                   QStringLiteral("snap"),
                 "snap config path produces 'snap' tag");
    ok &= expect(keyring_environment::tagForConfigRoot(
                   QStringLiteral("/home/user/snap/komai/42/.config/komai")) ==
                   QStringLiteral("snap"),
                 "snap config path with numeric revision produces 'snap' tag");

    // macOS native path produces "native" tag
    ok &= expect(keyring_environment::tagForConfigRoot(
                   QStringLiteral("/Users/user/Library/Preferences/komai")) ==
                   QStringLiteral("native"),
                 "macOS config path produces 'native' tag");

    // Windows native path produces "native" tag
    ok &= expect(keyring_environment::tagForConfigRoot(
                   QStringLiteral("C:/Users/user/AppData/Local/komai")) ==
                   QStringLiteral("native"),
                 "Windows config path produces 'native' tag");

    // Unknown path produces a 6-char hex hash
    const auto unknownTag =
      keyring_environment::tagForConfigRoot(QStringLiteral("/some/unusual/path/komai"));
    ok &= expect(unknownTag.length() == 6, "unknown config path produces 6-char tag");
    ok &= expect(
      std::all_of(unknownTag.cbegin(),
                  unknownTag.cend(),
                  [](QChar c) {
                      return (c >= QLatin1Char('0') && c <= QLatin1Char('9')) ||
                             (c >= QLatin1Char('a') && c <= QLatin1Char('f'));
                  }),
      "unknown config path tag is lowercase hex");

    // Same input produces same hash
    const auto repeatTag =
      keyring_environment::tagForConfigRoot(QStringLiteral("/some/unusual/path/komai"));
    ok &= expect(unknownTag == repeatTag, "hash is deterministic for same path");

    // Different unknown paths produce different hashes
    const auto otherTag =
      keyring_environment::tagForConfigRoot(QStringLiteral("/other/path/komai"));
    ok &= expect(unknownTag != otherTag, "different paths produce different hashes");

    // prefix() and tag() return consistent values
    ok &= expect(keyring_environment::prefix() ==
                   QStringLiteral("komai.") + keyring_environment::tag() + QStringLiteral("."),
                 "prefix() is 'komai.<tag>.'");

    return ok;
}

bool
testProviderSelectionHonorsConfigAndOverrides()
{
    YAML::Node root(YAML::NodeType::Map);
    root["secrets"]["provider"] = staged_load_plan::ProviderSecretServiceValue;
    auto fromConfig = settings::persistence::providerFromConfig(root);
    auto defaultSecretService =
      expect(fromConfig == staged_load_plan::SecretsProvider::SecretService,
             "secret provider defaults to secret_service");

    root["secrets"]["provider"] = staged_load_plan::ProviderFileValue;
    const bool explicitFileConfig = expect(
      settings::persistence::providerFromConfig(root) == staged_load_plan::SecretsProvider::File,
      "file provider is honored from config");

    return defaultSecretService && explicitFileConfig;
}

bool
testMatrixSessionSecretsRoundtripWithFileProvider()
{
    bool ok = true;

    const auto writer =
      settings::storage::inMemoryReaderWriter(QStringLiteral("/tmp/komai-test-matrix-session-secrets"));
    settings::storage::ReaderWriterOverride writerOverride{writer};

    const QString profile = QStringLiteral("matrix-sdk");
    const auto configPath = settings::storage::configFilePathForProfile(profile);
    const auto secretsPath = settings::storage::secretsFilePathForProfile(profile);

    YAML::Node config(YAML::NodeType::Map);
    config["secrets"]["provider"] = staged_load_plan::ProviderFileValue;
    ok &= expect(settings::storage::writeYamlFile(configPath, config, false),
                 "matrix session secrets test writes config");

    settings::persistence::saveProfileSecrets(
      profile,
      true,
      secretsPath,
      QStringLiteral("existing-access-token"),
      QMap<QString, QString>{{QStringLiteral("existing.secret"), QStringLiteral("keep-me")}});

    komai::matrix_backend::savePersistedMatrixSessionSecrets(
      profile,
      {
        .storePassphrase = QStringLiteral("store-passphrase"),
        .serializedSession = QStringLiteral("serialized-session"),
      });

    const auto persisted = komai::matrix_backend::loadPersistedMatrixSessionSecrets(profile);
    ok &= expect(persisted.storePassphrase == QStringLiteral("store-passphrase"),
                 "matrix session secrets load returns saved store passphrase");
    ok &= expect(persisted.serializedSession == QStringLiteral("serialized-session"),
                 "matrix session secrets load returns saved session blob");

    const auto payload = settings::persistence::loadProfileSecrets(profile, true, secretsPath);
    ok &= expect(payload.accessToken == QStringLiteral("existing-access-token"),
                 "matrix session save preserves access token");
    ok &= expect(payload.secrets.value(QStringLiteral("existing.secret")) == QStringLiteral("keep-me"),
                 "matrix session save preserves unrelated secrets");
    ok &= expect(
      payload.secrets.value(QStringLiteral("matrix_sdk.store_passphrase")) ==
        QStringLiteral("store-passphrase"),
      "matrix session save persists store passphrase secret");
    ok &= expect(
      payload.secrets.value(QStringLiteral("matrix_sdk.serialized_session")) ==
        QStringLiteral("serialized-session"),
      "matrix session save persists serialized session secret");

    komai::matrix_backend::clearPersistedMatrixSessionSecrets(profile);

    const auto clearedPayload = settings::persistence::loadProfileSecrets(profile, true, secretsPath);
    ok &= expect(
      clearedPayload.secrets.value(QStringLiteral("matrix_sdk.store_passphrase")).isEmpty(),
      "matrix session clear removes store passphrase secret");
    ok &= expect(
      clearedPayload.secrets.value(QStringLiteral("matrix_sdk.serialized_session")).isEmpty(),
      "matrix session clear removes serialized session secret");
    ok &= expect(clearedPayload.accessToken == QStringLiteral("existing-access-token"),
                 "matrix session clear preserves access token");
    ok &= expect(
      clearedPayload.secrets.value(QStringLiteral("existing.secret")) == QStringLiteral("keep-me"),
      "matrix session clear preserves unrelated secrets");

    return ok;
}

} // namespace

int
main()
{
    test_env::ScopedTestHome testHome{QStringLiteral("komai-settings-storage-test")};
    if (!testHome.isValid()) {
        std::cerr << "FAILED: test home environment can be created\n";
        return 1;
    }
    if (!testHome.isIsolated()) {
        std::cerr << "FAILED: test home environment is isolated\n";
        return 1;
    }

    bool ok = true;
    ok &= testYamlRoundtrip();
    ok &= testMissingAndInvalidFiles();
    ok &= testSecretsMapSerialization();
    ok &= testPathHelpers();
    ok &= testProfileIdNormalizationAndSecretKeyIds();
    ok &= testKeyringEnvironmentTagResolution();
    ok &= testProfileIdValidation();
    ok &= testLoggerInjectionNullAndInjectedLoggers();
    ok &= testCacheLoggerInjection();
    ok &= testProviderSelectionHonorsConfigAndOverrides();
    ok &= testMatrixSessionSecretsRoundtripWithFileProvider();
    ok &= testInMemoryReaderWriterOverride();

    return ok ? 0 : 1;
}
