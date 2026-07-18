// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string_view>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QTemporaryDir>

#include <memory>

#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"

#include "profile/KeyringEnvironment.h"
#include "profile/Paths.h"
#include "profile/ProfileId.h"
#include "profile/ProfileSecrets.h"
#include "matrix/backend/MatrixSessionSecrets.h"
#include "settings/SettingsStorage.h"
#include "support/settings/SettingsStorageSecretsCodec.h"
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
expectWithContext(bool condition, std::string_view message, const QString &context)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    std::cerr << context.toStdString() << '\n';
    return false;
}

::rust::Vec<::komai::rust::SettingsStringMapEntry>
toRustStringMapEntries(const QMap<QString, QString> &entries)
{
    ::rust::Vec<::komai::rust::SettingsStringMapEntry> rustEntries;
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        rustEntries.push_back({
          .key   = it.key().toStdString(),
          .value = it.value().toStdString(),
        });
    }
    return rustEntries;
}

QMap<QString, QString>
fromRustStringMapEntries(const ::rust::Vec<::komai::rust::SettingsStringMapEntry> &entries)
{
    QMap<QString, QString> decoded;
    for (const auto &entry : entries) {
        decoded.insert(QString::fromStdString(static_cast<std::string>(entry.key)),
                       QString::fromStdString(static_cast<std::string>(entry.value)));
    }
    return decoded;
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
    const auto content = QStringLiteral("ui:\n  motion:\n    enable_animations: true\nnavigation:\n  room_list:\n    width_px: 42\n");

    ok &= expect(settings::storage::writeTextFile(filePath, content, false),
                 "writeTextFile persists config text");

    const auto read = settings::storage::readTextFile(filePath, "settings-test");
    ok &= expect(read == content, "readTextFile returns written config text");

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
    const auto missingText = settings::storage::readTextFile(missingPath, "missing");
    ok &= expect(missingText.isEmpty(), "readTextFile returns empty string for missing file");

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

    const auto dataProfileDir = app_paths::data::profileDirectory(profile);
    ok &= expect(dataProfileDir.endsWith(QStringLiteral("/profiles/profile-123")),
                 "profile data directory uses the profile root directly");

#if defined(Q_OS_LINUX)
    const auto defaultDesktopId =
      app_paths::desktop::profileDesktopEntryId(QStringLiteral("default"));
    const auto workDesktopId = app_paths::desktop::profileDesktopEntryId(QStringLiteral("work"));
    const auto underscoredDesktopId =
      app_paths::desktop::profileDesktopEntryId(QStringLiteral("work_2"));
    const auto workDesktopFile =
      app_paths::desktop::profileDesktopEntryFile(QStringLiteral("work"));
    const auto missingDesktopFile =
      app_paths::desktop::findInstalledProfileDesktopEntry(QStringLiteral("work"));

    ok &= expect(app_paths::desktop::supportsProfileDesktopEntries(),
                 "native Linux tests support explicit profile desktop launchers");
    ok &= expect(defaultDesktopId == QStringLiteral("cc.etke.komai.profile.default"),
                 "default profile desktop id stays explicit");
    ok &= expect(workDesktopId == QStringLiteral("cc.etke.komai.profile.work"),
                 "safe profile desktop id stays readable");
    ok &= expect(underscoredDesktopId == QStringLiteral("cc.etke.komai.profile.work_2"),
                 "desktop profile ids use the validated profile name directly");
    ok &= expect(!app_paths::desktop::applicationsDirectory().isEmpty(),
                 "applications directory path is available");
    ok &= expect(workDesktopFile.startsWith(app_paths::desktop::applicationsDirectory() +
                                            QStringLiteral("/")),
                 "profile desktop entry file lives under the applications directory");
    ok &= expect(workDesktopFile.endsWith(QStringLiteral("/applications/cc.etke.komai.profile.work.desktop")),
                 "profile desktop entry file lives in the applications directory");
    ok &= expect(missingDesktopFile.isEmpty(),
                 "profile desktop entry lookup returns empty when no launcher exists");
#endif

    return ok;
}

bool
testProfileDesktopEntryRoundtrip()
{
#if !defined(Q_OS_LINUX)
    return true;
#else
    bool ok = true;

    const QString profile = QStringLiteral("work_2");
    const QString executablePath = QStringLiteral("/tmp/Komai $Dev/bin/komai");
    QString error;

    ok &= expect(app_paths::desktop::ensureProfileDesktopEntry(profile, executablePath, &error),
                 "profile desktop launcher can be written");
    ok &= expect(error.isEmpty(), "desktop entry write does not set an error on success");

    const auto filePath = app_paths::desktop::profileDesktopEntryFile(profile);
    const auto contents = settings::storage::readTextFile(filePath, "profile desktop entry test");
    const auto foundDesktopEntry = app_paths::desktop::findInstalledProfileDesktopEntry(profile);

    ok &= expect(QFileInfo::exists(filePath), "profile desktop launcher file exists");
    ok &= expect(foundDesktopEntry == filePath,
                 "profile desktop entry lookup finds the explicit user-local launcher");
    ok &= expect(contents.contains(QStringLiteral("Name=Komai (work_2)\n")),
                 "profile desktop launcher includes a profile-specific display name");
    ok &= expectWithContext(contents.contains(QStringLiteral("Exec=\"/tmp/Komai ")),
                            "profile desktop launcher starts the executable path as a quoted argument",
                            contents);
    ok &= expectWithContext(
      contents.contains(QStringLiteral("$Dev/bin/komai\" -p work_2 %u\n")),
      "profile desktop launcher keeps the executable path quoted through the profile args",
      contents);
    ok &= expect(contents.contains(QStringLiteral("Categories=Network;InstantMessaging;Qt;\n")),
                 "profile desktop launcher advertises standard app categories");
    ok &= expect(!contents.contains(QStringLiteral("NoDisplay=")),
                 "profile desktop launcher stays visible in app menus");
    ok &= expect(!contents.contains(QStringLiteral("MimeType=")),
                 "profile desktop launcher does not register duplicate URI handlers");

    error.clear();
    ok &= expect(app_paths::desktop::removeProfileDesktopEntry(profile, &error),
                 "profile desktop launcher can be removed");
    ok &= expect(error.isEmpty(), "desktop entry removal does not set an error on success");
    ok &= expect(!QFileInfo::exists(filePath), "profile desktop launcher file is removed");

    return ok;
#endif
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
    ok &= expect(!profile_id::validate(QStringLiteral("etke_cc-1")).has_value(),
                 "underscore and dash profile id is valid");
    ok &= expect(!profile_id::validate(QStringLiteral("_scratch")).has_value(),
                 "profile id may start with underscore");

    ok &= expect(profile_id::validate(QStringLiteral("../komai/.")).has_value(),
                 "path traversal style profile id is rejected");
    ok &= expect(profile_id::validate(QStringLiteral("test8.1")).has_value(),
                 "profile id containing dot is rejected");
    ok &= expect(profile_id::validate(QStringLiteral("8test")).has_value(),
                 "profile id starting with digit is rejected");
    ok &= expect(profile_id::validate(QStringLiteral("-test")).has_value(),
                 "profile id starting with dash is rejected");
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
    ok &= expect(settings::storage::writeTextFile(
                   file, QStringLiteral("ui:\n  motion:\n    enable_animations: false\n"), false),
                 "settings storage can write with null logger");
    ok &= expect(
      settings::storage::readTextFile(file, "settings-test").contains(
        QStringLiteral("enable_animations: false")),
                 "settings storage can read with null logger");

    auto uiLogger = std::make_shared<komai::logging::Logger>("test-ui");
    auto dbLogger = std::make_shared<komai::logging::Logger>("test-db");
    settings::storage::setLoggers({.ui = uiLogger, .db = dbLogger});
    current = settings::storage::activeLoggers();
    ok &= expect(current.ui == uiLogger, "settings storage stores injected ui logger");
    ok &= expect(current.db == dbLogger, "settings storage stores injected db logger");

    ok &= expect(
      settings::storage::readTextFile(file, "settings-test").contains(
        QStringLiteral("enable_animations: false")),
                 "settings storage can read with injected logger");

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

    ok &= expect(settings::storage::writeTextFile(
                   configPath, QStringLiteral("integrations:\n  dbus:\n    access: 2\n"), false),
                 "in-memory writer can persist text content");
    const auto loaded = settings::storage::readTextFile(configPath, "readerwriter-config");
    ok &= expect(loaded.contains(QStringLiteral("access: 2")),
                 "in-memory writer stores written data");

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
testMatrixSessionSecretsRoundtripWithFileProvider()
{
    bool ok = true;

    const auto writer =
      settings::storage::inMemoryReaderWriter(QStringLiteral("/tmp/komai-test-matrix-session-secrets"));
    settings::storage::ReaderWriterOverride writerOverride{writer};

    const QString profile = QStringLiteral("matrix-sdk");
    const auto configPath = settings::storage::configFilePathForProfile(profile);
    const auto secretsPath = settings::storage::secretsFilePathForProfile(profile);
    const auto matrixSdkSecretsPath =
      QDir(settings::storage::profileDirPath(profile)).filePath(QStringLiteral("matrix-sdk-secrets.yml"));

    ok &= expect(settings::storage::writeTextFile(
                   configPath, QStringLiteral("secrets:\n  provider: file\n"), false),
                 "matrix session secrets test writes config");

    const auto configOverview =
      ::komai::rust::settings_load_config_overview_for_profile(profile.toStdString());
    ok &= expect(configOverview.uses_file_secrets_provider,
                 "matrix session secrets test loads file provider from config overview");

    const bool savedSecrets = ::komai::rust::settings_write_persisted_secrets_file_for_profile(
      profile.toStdString(),
      QStringLiteral("existing-access-token").toStdString(),
      toRustStringMapEntries(
        QMap<QString, QString>{{QStringLiteral("existing.secret"), QStringLiteral("keep-me")}}),
      true);
    ok &= expect(savedSecrets, "matrix session secrets test saves initial secrets payload");

    const bool savedSessionSecrets = komai::matrix_backend::savePersistedMatrixSessionSecrets(
      profile,
      {
        .storePassphrase = QStringLiteral("store-passphrase"),
        .homeserverUrl = QStringLiteral("https://matrix.example.com"),
        .serializedSession = QStringLiteral("serialized-session"),
      });
    ok &= expect(savedSessionSecrets, "matrix session secrets save reports success");

    const auto persisted = komai::matrix_backend::loadPersistedMatrixSessionSecrets(profile);
    ok &= expect(persisted.storePassphrase == QStringLiteral("store-passphrase"),
                 "matrix session secrets load returns saved store passphrase");
    ok &= expect(persisted.homeserverUrl == QStringLiteral("https://matrix.example.com"),
                 "matrix session secrets load returns saved homeserver url");
    ok &= expect(persisted.serializedSession == QStringLiteral("serialized-session"),
                 "matrix session secrets load returns saved session blob");

    const auto payload =
      ::komai::rust::settings_load_persisted_secrets_file_for_profile(profile.toStdString());
    const auto decodedPayloadSecrets = fromRustStringMapEntries(payload.secrets);
    ok &= expect(QString::fromStdString(static_cast<std::string>(payload.access_token)) ==
                   QStringLiteral("existing-access-token"),
                 "matrix session save preserves access token");
    ok &= expect(decodedPayloadSecrets.value(QStringLiteral("existing.secret")) ==
                   QStringLiteral("keep-me"),
                 "matrix session save preserves unrelated secrets");
    ok &= expect(decodedPayloadSecrets.value(QStringLiteral("matrix_sdk.store_passphrase")).isEmpty(),
                 "matrix session save no longer writes store passphrase into profile secrets");
    ok &= expect(decodedPayloadSecrets.value(QStringLiteral("matrix_sdk.homeserver_url")).isEmpty(),
                 "matrix session save no longer writes homeserver into profile secrets");
    ok &= expect(decodedPayloadSecrets.value(QStringLiteral("matrix_sdk.serialized_session"))
                   .isEmpty(),
                 "matrix session save no longer writes serialized session into profile secrets");

    const auto matrixSdkSecrets = settings::storage::decodeSecretsFilePayload(
      settings::storage::readTextFile(matrixSdkSecretsPath, "matrix-sdk secrets test"));
    ok &= expect(matrixSdkSecrets.value(QStringLiteral("matrix_sdk.store_passphrase")) ==
                   QStringLiteral("store-passphrase"),
                 "matrix session save persists store passphrase in dedicated matrix-sdk secret store");
    ok &= expect(matrixSdkSecrets.value(QStringLiteral("matrix_sdk.homeserver_url")) ==
                   QStringLiteral("https://matrix.example.com"),
                 "matrix session save persists homeserver in dedicated matrix-sdk secret store");
    ok &= expect(matrixSdkSecrets.value(QStringLiteral("matrix_sdk.serialized_session")) ==
                   QStringLiteral("serialized-session"),
                 "matrix session save persists serialized session in dedicated matrix-sdk secret store");

    komai::matrix_backend::clearPersistedMatrixSessionSecrets(profile);

    const auto clearedPayload =
      ::komai::rust::settings_load_persisted_secrets_file_for_profile(profile.toStdString());
    const auto decodedClearedSecrets = fromRustStringMapEntries(clearedPayload.secrets);
    ok &= expect(
      decodedClearedSecrets.value(QStringLiteral("matrix_sdk.store_passphrase")).isEmpty(),
      "matrix session clear removes store passphrase secret");
    ok &= expect(
      decodedClearedSecrets.value(QStringLiteral("matrix_sdk.homeserver_url")).isEmpty(),
      "matrix session clear removes homeserver secret");
    ok &= expect(
      decodedClearedSecrets.value(QStringLiteral("matrix_sdk.serialized_session")).isEmpty(),
      "matrix session clear removes serialized session secret");
    ok &= expect(QString::fromStdString(static_cast<std::string>(clearedPayload.access_token)) ==
                   QStringLiteral("existing-access-token"),
                 "matrix session clear preserves access token");
    ok &= expect(
      decodedClearedSecrets.value(QStringLiteral("existing.secret")) == QStringLiteral("keep-me"),
      "matrix session clear preserves unrelated secrets");
    ok &= expect(!settings::storage::pathExists(matrixSdkSecretsPath),
                 "matrix session clear removes dedicated matrix-sdk secret store");

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
    ok &= testProfileDesktopEntryRoundtrip();
    ok &= testProfileIdNormalizationAndSecretKeyIds();
    ok &= testKeyringEnvironmentTagResolution();
    ok &= testProfileIdValidation();
    ok &= testLoggerInjectionNullAndInjectedLoggers();
    ok &= testMatrixSessionSecretsRoundtripWithFileProvider();
    ok &= testInMemoryReaderWriterOverride();

    return ok ? 0 : 1;
}
