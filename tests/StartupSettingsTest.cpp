// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include <QApplication>
#include <QFile>
#include <QTemporaryDir>

#include <yaml-cpp/yaml.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>

#include "settings/ui/facade/UserSettingsPage.h"
#include "settings/ui/SettingDescriptor.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsSerializer.h"
#include "settings/SettingsSerializerConfigConverters.h"
#include "settings/SettingsSerializerConfigSchema.h"
#include "settings/SettingsMigrations.h"
#include "settings/SettingsStorage.h"
#include "settings/StartupSettings.h"
#include "settings/StagedLoadPlan.h"
#include "settings/YamlSettings.h"
#include "settings/core/StartupConfig.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsCoreStoreBridge.h"
#include "ui/ThemeRegistry.h"
#include "TestEnvironment.h"

namespace {

struct StartupSettingsTestContext
{
    explicit StartupSettingsTestContext(QStringView profile)
      : profile_{profile}
      , baseDir_{QStringLiteral("/tmp/komai-startup-settings-test/") + profile.toString()}
      , writerOverride_{settings::storage::inMemoryReaderWriter(baseDir_)}
    {
    }

    bool isValid() const { return true; }

    bool writeConfig(const YAML::Node &configRoot)
    {
        const auto configFile = settings::storage::configFilePathForProfile(profile_);
        return settings::storage::writeYamlFile(configFile, configRoot, false);
    }

    bool writeState(const YAML::Node &stateRoot)
    {
        const auto stateFile = settings::storage::stateFilePathForProfile(profile_);
        return settings::storage::writeYamlFile(stateFile, stateRoot, false);
    }

    bool writeSession(const YAML::Node &sessionRoot)
    {
        const auto sessionFile = settings::storage::sessionFilePathForProfile(profile_);
        return settings::storage::writeYamlFile(sessionFile, sessionRoot, false);
    }

    QString configFile() const { return settings::storage::configFilePathForProfile(profile_); }

    QString stateFile() const { return settings::storage::stateFilePathForProfile(profile_); }

    QString sessionFile() const { return settings::storage::sessionFilePathForProfile(profile_); }

    QString secretsFile() const { return settings::storage::secretsFilePathForProfile(profile_); }

private:
    QString profile_;
    QString baseDir_;
    settings::storage::ReaderWriterOverride writerOverride_{nullptr};
};

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool
expectScalarString(const YAML::Node &root,
                   const char *dottedKey,
                   const QString &expected,
                   std::string_view message)
{
    const auto node = yaml_settings::getNode(root, dottedKey);
    if (!node || !node.IsScalar()) {
        std::cerr << "FAILED: " << message << " (missing scalar at '" << dottedKey << "')\n";
        return false;
    }

    try {
        return expect(QString::fromStdString(node.as<std::string>()) == expected, message);
    } catch (...) {
        std::cerr << "FAILED: " << message << " (unable to parse scalar at '" << dottedKey
                  << "' as string)\n";
        return false;
    }
}

bool
expectScalarInt(const YAML::Node &root,
                const char *dottedKey,
                int expected,
                std::string_view message)
{
    const auto node = yaml_settings::getNode(root, dottedKey);
    if (!node || !node.IsScalar()) {
        std::cerr << "FAILED: " << message << " (missing scalar at '" << dottedKey << "')\n";
        return false;
    }

    try {
        return expect(node.as<int>() == expected, message);
    } catch (...) {
        std::cerr << "FAILED: " << message << " (unable to parse scalar at '" << dottedKey
                  << "' as int)\n";
        return false;
    }
}

bool
testStartupConfigSnapshotLoads()
{
    const QString profile = QStringLiteral("profile-startup");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "temporary config root can be created");

    YAML::Node configRoot(YAML::NodeType::Map);
    configRoot["ui"]["scale"]["factor"] = 1.75;
    configRoot["ui"]["font"]["size_pt"] = 15;
    if (!ctx.writeConfig(configRoot))
        return expect(false, "test profile config can be persisted");

    const auto startup = settings::startup::readStartupConfig(profile);
    const bool scaleMatches =
      expect(startup.uiScaleFactor.has_value() && std::abs(*startup.uiScaleFactor - 1.75F) < 0.0001F,
             "scale factor is parsed from config.yml");
    const bool configLoaded = expect(startup.configRoot.IsDefined() &&
                                      startup.configRoot["ui"]["font"]["size_pt"].as<int>() == 15,
                                      "config root includes additional startup-read values");
    const bool keyPresent = expect(startup.configRoot["ui"]["scale"]["factor"].IsDefined(),
                                  "startup snapshot preserves nested key structure");

    return scaleMatches && configLoaded && keyPresent;
}

bool
testStartupConfigSnapshotMissingProfile()
{
    const QString profile = QStringLiteral("missing-startup-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "temporary config root can be created");

    const auto startup = settings::startup::readStartupConfig(profile);

    return expect(!startup.uiScaleFactor.has_value(),
                  "missing profile has no startup scale factor") &&
           expect(!startup.configRoot.IsDefined() || startup.configRoot.size() == 0,
                  "missing profile snapshot is empty");
}

bool
testCoreSnapshotExtraction()
{
    YAML::Node root(YAML::NodeType::Map);
    root["ui"]["scale"]["factor"] = 2.0;
    auto snapshot = settings::core::snapshotFromYamlConfig(root);
    if (!expect(snapshot.uiScaleFactor.has_value() &&
               std::abs(*snapshot.uiScaleFactor - 2.0F) < 0.0001F,
               "core snapshot extracts supported scale factor")) {
        return false;
    }
    if (!expect(!snapshot.configRoot["ui"]["scale"]["factor"].IsNull(),
                "core snapshot keeps full config root")) {
        return false;
    }

    root["ui"]["scale"]["factor"] = "invalid";
    snapshot = settings::core::snapshotFromYamlConfig(root);
    if (!expect(!snapshot.uiScaleFactor.has_value(), "core snapshot ignores malformed scale factor"))
        return false;

    root["ui"]["scale"]["factor"] = 5.0;
    snapshot = settings::core::snapshotFromYamlConfig(root);
    return expect(!snapshot.uiScaleFactor.has_value(),
                  "core snapshot ignores out-of-range scale factors");
}

bool
testCoreScaleRangeHelpers()
{
    bool ok = true;
    ok &= expect(settings::core::isScaleFactorInRange(1.0F),
                 "scale factor accepts lower bound");
    ok &= expect(settings::core::isScaleFactorInRange(3.0F),
                 "scale factor accepts upper bound");
    ok &= expect(!settings::core::isScaleFactorInRange(0.5F),
                 "scale factor rejects values below range");
    ok &= expect(!settings::core::isScaleFactorInRange(3.5F),
                 "scale factor rejects values above range");
    const auto normalized = settings::core::normalizeScaleFactor(1.75F);
    ok &= expect(normalized.has_value() && std::abs(*normalized - 1.75F) < 0.0001F,
                 "scale factor normalization preserves in-range value");
    ok &= expect(!settings::core::normalizeScaleFactor(0.5F).has_value(),
                 "scale factor normalization rejects out-of-range values");
    return ok;
}

bool
testCoreSnapshotFromFile()
{
    QTemporaryDir tmpDir;
    if (!tmpDir.isValid())
        return expect(false, "temporary directory can be created");

    const auto path = tmpDir.path() + QStringLiteral("/config.yml");
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return expect(false, "temporary snapshot file can be created");

    YAML::Node root(YAML::NodeType::Map);
    root["ui"]["scale"]["factor"] = 2.25;
    file.write(QString::fromUtf8(YAML::Dump(root)).toUtf8());
    file.close();

    const auto snapshot = settings::core::snapshotFromYamlFile(path.toStdString());
    return expect(snapshot.uiScaleFactor.has_value() &&
                  std::abs(*snapshot.uiScaleFactor - 2.25F) < 0.0001F,
                  "core snapshot loads from file path");
}

bool
testStartupPolicySkipsSessionWritesUntilCompleteSession()
{
    const QString profile = QStringLiteral("startup-policy-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "temporary config root can be created");

    const QString configFile = ctx.configFile();
    const QString stateFile  = ctx.stateFile();
    const QString sessionFile = ctx.sessionFile();
    const QString secretsFile = ctx.secretsFile();

    YAML::Node configRoot(YAML::NodeType::Map);
    configRoot["secrets"]["provider"] = "file";
    configRoot["ui"]["theme"]["slug"] = "komai-light";

    if (!ctx.writeConfig(configRoot))
        return expect(false, "startup-policy fixture config can be persisted");
    const auto persistedConfig =
      settings::storage::loadYamlFile(configFile, "startup-policy-fixture-config");
    if (!expect(staged_load_plan::providerFromConfig(persistedConfig) ==
                  staged_load_plan::SecretsProvider::File,
                "fixture config persists file secrets provider token"))
        return false;

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after initialize");
    if (!expect(settings->configFilePath() == configFile,
                "resolved config path matches fixture path")) {
        std::cerr << "expected: " << configFile.toStdString() << '\n'
                  << "actual:   " << settings->configFilePath().toStdString() << '\n';
        return false;
    }
    if (!expect(settings->usesFileSecretsProvider(),
                "file provider from config is applied during startup load"))
        return false;

    settings->save();

    if (!expect(settings::storage::pathExists(configFile),
                "startup save creates config.yml in config-only mode"))
        return false;
    if (!expect(!settings::storage::pathExists(stateFile) && !settings::storage::pathExists(sessionFile) &&
                  !settings::storage::pathExists(secretsFile),
                "startup save does not create state/session/secrets files")) {
        return false;
    }

    settings->setPersistenceSuspended(false);
    if (!settings->persistSessionSnapshot(
          UserSettings::SessionSnapshot{.userId      = QStringLiteral("@test:example.org"),
                                       .accessToken = QStringLiteral("token"),
                                       .deviceId    = QStringLiteral("DEVICE"),
                                       .homeserver  = QStringLiteral("https://example.org")})) {
        return expect(false, "persistSessionSnapshot accepts complete session identity");
    }

    return expect(settings::storage::pathExists(stateFile),
                  "full persistence writes state.yml after complete snapshot") &&
           expect(settings::storage::pathExists(sessionFile),
                  "full persistence writes session.yml after complete snapshot") &&
           expect(settings::storage::pathExists(secretsFile),
                  "full persistence writes secrets.yml after complete snapshot");
}

bool
testStartupPolicyConfigOnlyEditsDoNotCreateSessionOrSecrets()
{
    const QString profile = QStringLiteral("startup-policy-config-only-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "temporary config root can be created");

    const QString configFile = ctx.configFile();
    const QString stateFile  = ctx.stateFile();
    const QString sessionFile = ctx.sessionFile();
    const QString secretsFile = ctx.secretsFile();

    YAML::Node configRoot(YAML::NodeType::Map);
    configRoot["secrets"]["provider"] = "file";
    configRoot["ui"]["theme"]["slug"] = "komai-light";
    if (!ctx.writeConfig(configRoot))
        return expect(false, "startup-policy-config-only fixture config can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after initialize");

    settings->setPersistenceSuspended(false);
    settings->setUiThemeSlug(QStringLiteral("komai-dark"));

    if (!expect(settings::storage::pathExists(configFile),
                "theme change creates config.yml in config-only mode"))
        return false;

    auto configAfter = settings::storage::loadYamlFile(configFile, "config-after-theme-change");
    const auto themeNode = configAfter["ui"]["theme"]["slug"];
    if (!themeNode || !themeNode.IsScalar())
        return expect(false, "theme is persisted as scalar in config file");
    const auto storedTheme = QString::fromStdString(themeNode.as<std::string>());
    const bool persistedTheme = expect(
      storedTheme == QStringLiteral("komai-dark"), "theme change is persisted to config.yml");

    return persistedTheme &&
           expect(!settings::storage::pathExists(stateFile) &&
                    !settings::storage::pathExists(sessionFile) &&
                    !settings::storage::pathExists(secretsFile),
                  "theme change does not create state/session/secrets files");
}

bool
testEnumSettingsPersistAsStrings()
{
    const QString profile = QStringLiteral("enum-settings-string-persistence-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "enum persistence fixture config root can be created");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for enum persistence test");

    settings->setPersistenceSuspended(false);
    settings->setNetworkPresenceStatusPolicy(UserSettings::Presence::Offline);
    settings->setTimelineMediaImageDisplay(UserSettings::ShowImage::Never);
    settings->setTimelineMessagesSenderUsername(UserSettings::ShowSenderUsername::Always);
    settings->setComposerInputAutoReplaceEmoji(UserSettings::AutoReplaceEmoji::Never);
    settings->setComposerInputSendKey(UserSettings::SendMessageKey::CtrlEnter);
    settings->setSidebarsRoomListSort(UserSettings::RoomSortOrder::Alphabetical);
    settings->setSidebarsRoomListLastMessagePreview(UserSettings::LastMessagePreview::Never);
    settings->setTimelineMessageActionsActivationPolicy(
      UserSettings::TimelineMessageActionsActivationPolicy::OnHover);
    settings->setTimelineMessagesLayoutStyle(
      UserSettings::TimelineMessagesLayoutStyle::Minimal);
    settings->setNotificationsMessageContentPolicy(
      UserSettings::NotificationMessageContentPolicy::Never);
    settings->setIntegrationsDbusApiAccess(IntegrationsDbusAccessReadOnly);
    settings->setUiInputMode(true);
    settings->save();

    const auto configRoot = settings::storage::loadYamlFile(ctx.configFile(), "config");
    bool ok               = true;
    ok &= expectScalarString(configRoot,
                             SettingKey::NetworkPresenceStatusPolicy,
                             QStringLiteral("offline"),
                             "presence policy is persisted as string token");
    ok &= expectScalarString(configRoot,
                             SettingKey::TimelineMediaImageDisplay,
                             QStringLiteral("never"),
                             "image display policy is persisted as string token");
    ok &= expectScalarString(configRoot,
                             SettingKey::TimelineMessagesSenderUsername,
                             QStringLiteral("always"),
                             "sender username policy is persisted as string token");
    ok &= expectScalarString(configRoot,
                             SettingKey::ComposerInputAutoReplaceEmoji,
                             QStringLiteral("never"),
                             "auto-replace emoji policy is persisted as string token");
    ok &= expectScalarString(configRoot,
                             SettingKey::ComposerInputSendKey,
                             QStringLiteral("ctrl_enter"),
                             "send key policy is persisted as string token");
    ok &= expectScalarString(configRoot,
                             SettingKey::SidebarsRoomListSort,
                             QStringLiteral("alphabetical"),
                             "room sort policy is persisted as string token");
    ok &= expectScalarString(configRoot,
                             SettingKey::SidebarsRoomListLastMessagePreview,
                             QStringLiteral("never"),
                             "last message preview policy is persisted as string token");
    ok &= expectScalarString(configRoot,
                             SettingKey::TimelineMessageActionsActivationPolicy,
                             QStringLiteral("on_message_hover"),
                             "message actions activation policy is persisted as string token");
    ok &= expectScalarString(configRoot,
                             SettingKey::TimelineMessagesLayoutStyle,
                             QStringLiteral("minimal"),
                             "timeline layout style is persisted as string token");
    ok &= expectScalarString(configRoot,
                             SettingKey::NotificationsMessageContentPolicy,
                             QStringLiteral("never"),
                             "notification message content policy is persisted as string token");
    ok &= expectScalarString(configRoot,
                             SettingKey::IntegrationsDbusApiAccess,
                             QStringLiteral("read_only"),
                             "D-Bus access policy is persisted as string token");
    ok &= expectScalarString(configRoot,
                             SettingKey::UiInputMode,
                             QStringLiteral("touch"),
                             "input mode is persisted as string token");

    return ok;
}

bool
testInvalidConfigTokensFallbackToSafeValues()
{
    const QString profile = QStringLiteral("invalid-config-token-fallback-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "invalid token fixture config root can be created");

    YAML::Node configRoot(YAML::NodeType::Map);
    configRoot["ui"]["theme"]["slug"]                  = "not-a-real-theme";
    configRoot["ui"]["input"]["mode"]                  = "spaceship";
    configRoot["network"]["presence"]["status_policy"] = "not_a_real_presence";
    if (!ctx.writeConfig(configRoot))
        return expect(false, "invalid token fixture config can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for invalid token test");

    bool ok = true;
    ok &= expect(settings->uiThemeSlug() != QStringLiteral("not-a-real-theme"),
                 "invalid theme slug is ignored");
    ok &= expect(settings->networkPresenceStatusPolicy() == UserSettings::Presence::AutomaticPresence,
                 "invalid presence token falls back to automatic presence");
    ok &= expect(!settings->uiInputMode(),
                 "invalid input mode token falls back to desktop mode");

    const auto &store = settings->coreStore();
    const auto theme = store.valueAs<std::string>(settings::core::SettingId::UiThemeSlug);
    const auto presence = store.valueAs<int>(settings::core::SettingId::NetworkPresenceStatusPolicy);
    const auto inputMode = store.valueAs<bool>(settings::core::SettingId::UiInputMode);
    ok &= expect(theme.has_value() && *theme != std::string{"not-a-real-theme"},
                 "core store keeps valid theme after invalid theme token");
    ok &= expect(presence.has_value() &&
                   *presence == static_cast<int>(UserSettings::Presence::AutomaticPresence),
                 "core store keeps fallback presence for invalid token");
    ok &= expect(inputMode.has_value() && !*inputMode,
                 "core store keeps fallback input mode for invalid token");

    return ok;
}

bool
testInvalidStateDimensionsFallbackToSafeValues()
{
    const QString profile = QStringLiteral("invalid-state-dimensions-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "invalid state fixture root can be created");

    YAML::Node stateRoot(YAML::NodeType::Map);
    stateRoot["app"]["window"]["size"]["width"]            = -10;
    stateRoot["app"]["window"]["size"]["height"]           = 0;
    stateRoot["sidebars"]["room_list"]["width_px"]         = -20;
    stateRoot["sidebars"]["communities"]["width_px"]       = 0;
    if (!ctx.writeState(stateRoot))
        return expect(false, "invalid state fixture can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for invalid state test");

    bool ok = true;
    ok &= expect(settings->windowWidth() == settings::core::definitions::kDefaultWindowWidthPx,
                 "invalid window width falls back to default");
    ok &= expect(settings->windowHeight() == settings::core::definitions::kDefaultWindowHeightPx,
                 "invalid window height falls back to default");
    ok &= expect(settings->sidebarsRoomListWidthPx() ==
                   settings::core::definitions::kDefaultSidebarsRoomListWidthPx,
                 "invalid room list width falls back to default");
    ok &= expect(settings->sidebarsCommunitiesWidthPx() ==
                   settings::core::definitions::kDefaultSidebarsCommunitiesWidthPx,
                 "invalid communities width falls back to default");

    return ok;
}

bool
testSessionIdentityValuesAreTrimmedOnLoad()
{
    const QString profile = QStringLiteral("session-trim-normalization-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "session trim fixture root can be created");

    YAML::Node sessionRoot(YAML::NodeType::Map);
    sessionRoot["session"]["account"]["user_id"]   = "  @alice:example.org  ";
    sessionRoot["session"]["account"]["homeserver"] = "  https://example.org  ";
    sessionRoot["session"]["device"]["id"]         = "   ";
    if (!ctx.writeSession(sessionRoot))
        return expect(false, "session trim fixture can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for session trim test");

    bool ok = true;
    ok &= expect(settings->userId() == QStringLiteral("@alice:example.org"),
                 "user id is trimmed when loading session snapshot");
    ok &= expect(settings->homeserver() == QStringLiteral("https://example.org"),
                 "homeserver is trimmed when loading session snapshot");
    ok &= expect(settings->deviceId().isEmpty(),
                 "whitespace-only device id is normalized to empty");
    return ok;
}

bool
testMalformedSessionIdentityValuesFallbackToEmpty()
{
    const QString profile = QStringLiteral("session-malformed-values-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "malformed session fixture root can be created");

    YAML::Node sessionRoot(YAML::NodeType::Map);
    sessionRoot["session"]["account"]["user_id"]    = YAML::Node(YAML::NodeType::Map);
    sessionRoot["session"]["account"]["homeserver"] = YAML::Node(YAML::NodeType::Sequence);
    sessionRoot["session"]["device"]["id"]          = YAML::Node(YAML::NodeType::Map);
    if (!ctx.writeSession(sessionRoot))
        return expect(false, "malformed session fixture can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for malformed session test");

    bool ok = true;
    ok &= expect(settings->userId().isEmpty(), "non-string user id falls back to empty");
    ok &= expect(settings->homeserver().isEmpty(), "non-string homeserver falls back to empty");
    ok &= expect(settings->deviceId().isEmpty(), "non-string device id falls back to empty");
    ok &= expect(!settings->hasPersistedSessionIdentity(),
                 "malformed session data does not report persisted identity");
    ok &= expect(!settings->hasActiveSession(),
                 "malformed session data does not report active session");
    return ok;
}

bool
testSessionAuthStateHelpersForIncompleteLogin()
{
    const QString profile = QStringLiteral("session-auth-state-helpers-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "session auth helper fixture root can be created");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for session auth helper test");

    bool ok = true;
    ok &= expect(!settings->hasPersistedSessionIdentity(),
                 "fresh profile starts without persisted session identity");
    ok &= expect(!settings->hasActiveSession(), "fresh profile starts without active session");

    settings->setSessionSnapshot(UserSettings::SessionSnapshot{
      .userId      = QStringLiteral("@alice:example.org"),
      .accessToken = QString(),
      .deviceId    = QStringLiteral("DEVICE1"),
      .homeserver  = QStringLiteral("https://example.org")});
    ok &= expect(settings->hasPersistedSessionIdentity(),
                 "session identity can exist without access token");
    ok &= expect(!settings->hasActiveSession(),
                 "session without token is treated as incomplete login");

    settings->setAccessToken(QStringLiteral("token"));
    ok &= expect(settings->hasActiveSession(),
                 "adding access token marks session as active");

    settings->clearAuth();
    ok &= expect(!settings->hasPersistedSessionIdentity(),
                 "clearAuth removes persisted session identity");
    ok &= expect(!settings->hasActiveSession(), "clearAuth removes active session");

    return ok;
}

bool
testConfigSchemaVersionIsStampedOnSave()
{
    const QString profile = QStringLiteral("config-schema-version-stamp-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "config schema version fixture root can be created");

    YAML::Node configRoot(YAML::NodeType::Map);
    configRoot["ui"]["theme"]["slug"] = "komai-light";
    if (!ctx.writeConfig(configRoot))
        return expect(false, "config schema version fixture can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for schema version test");

    settings->setPersistenceSuspended(false);
    settings->setUiThemeSlug(QStringLiteral("komai-dark"));

    const auto persisted = settings::storage::loadYamlFile(ctx.configFile(), "schema-version");
    return expectScalarInt(persisted,
                           SettingKey::ConfigSchemaVersion,
                           settings::migrations::kCurrentConfigSchemaVersion,
                           "config save stamps current settings schema version");
}

bool
testConfigMigrationStampsVersionWhenMissing()
{
    YAML::Node configRoot(YAML::NodeType::Map);
    configRoot["ui"]["theme"]["slug"] = "komai-light";

    const auto outcome = settings::migrations::migrateConfigRoot(configRoot);
    bool ok            = true;
    ok &= expect(!outcome.hadFutureVersion,
                 "missing schema version is treated as migratable current-or-older config");
    ok &= expect(!outcome.hadUnsupportedPath,
                 "missing schema version has a supported migration path");
    ok &= expect(outcome.sourceVersion == 0, "missing schema version is treated as v0");
    ok &= expect(outcome.migratedVersion == settings::migrations::kCurrentConfigSchemaVersion,
                 "missing schema version migrates to current version");
    ok &= expectScalarInt(outcome.migratedRoot,
                          SettingKey::ConfigSchemaVersion,
                          settings::migrations::kCurrentConfigSchemaVersion,
                          "migration stamps current schema version on migrated root");
    ok &= expectScalarString(outcome.migratedRoot,
                             SettingKey::UiThemeSlug,
                             QStringLiteral("komai-light"),
                             "migration preserves existing config values");
    return ok;
}

bool
testConfigMigrationKeepsFutureVersionUntouched()
{
    YAML::Node configRoot(YAML::NodeType::Map);
    constexpr int futureVersion = settings::migrations::kCurrentConfigSchemaVersion + 7;
    configRoot["meta"]["settings_schema_version"] = futureVersion;
    configRoot["ui"]["theme"]["slug"]             = "komai-dark";

    const auto outcome = settings::migrations::migrateConfigRoot(configRoot);
    bool ok            = true;
    ok &= expect(outcome.hadFutureVersion, "future schema version is surfaced as future-version");
    ok &= expect(!outcome.hadUnsupportedPath,
                 "future schema version bypass does not report unsupported migration path");
    ok &= expect(outcome.sourceVersion == futureVersion,
                 "future schema version is reported in migration outcome");
    ok &= expect(outcome.migratedVersion == futureVersion,
                 "future schema version keeps migration target untouched");
    ok &= expectScalarInt(outcome.migratedRoot,
                          SettingKey::ConfigSchemaVersion,
                          futureVersion,
                          "future schema version remains untouched");
    ok &= expectScalarString(outcome.migratedRoot,
                             SettingKey::UiThemeSlug,
                             QStringLiteral("komai-dark"),
                             "future-version migration keeps existing values unchanged");
    return ok;
}

bool
testConfigMigrationNormalizesNonMapConfigRoot()
{
    YAML::Node nonMapRoot("not-a-map");
    const auto outcome = settings::migrations::migrateConfigRoot(nonMapRoot);
    bool ok            = true;
    ok &= expect(outcome.migratedRoot.IsMap(),
                 "migration normalizes non-map config root to an empty map");
    ok &= expect(!outcome.hadUnsupportedPath,
                 "non-map config normalization follows a supported migration path");
    ok &= expectScalarInt(outcome.migratedRoot,
                          SettingKey::ConfigSchemaVersion,
                          settings::migrations::kCurrentConfigSchemaVersion,
                          "normalized map still gets current schema version stamp");
    return ok;
}

bool
testMalformedFileSecretsPayloadFallsBackSafely()
{
    const QString profile = QStringLiteral("malformed-file-secrets-payload-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "malformed file secrets fixture root can be created");

    YAML::Node configRoot(YAML::NodeType::Map);
    configRoot["secrets"]["provider"] = staged_load_plan::ProviderFileValue;
    if (!ctx.writeConfig(configRoot))
        return expect(false, "malformed file secrets fixture config can be persisted");

    YAML::Node sessionRoot(YAML::NodeType::Map);
    sessionRoot["session"]["account"]["user_id"]    = "@alice:example.org";
    sessionRoot["session"]["account"]["homeserver"] = "https://example.org";
    sessionRoot["session"]["device"]["id"]          = "DEVICE1";
    if (!ctx.writeSession(sessionRoot))
        return expect(false, "malformed file secrets fixture session can be persisted");

    YAML::Node secretsRoot(YAML::NodeType::Map);
    secretsRoot["auth"]["access_token"] = YAML::Node(YAML::NodeType::Sequence);
    secretsRoot["secrets"]              = "not-a-map";
    if (!settings::storage::writeYamlFile(ctx.secretsFile(), secretsRoot, false))
        return expect(false, "malformed file secrets fixture payload can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for malformed file secrets test");

    bool ok = true;
    ok &= expect(settings->usesFileSecretsProvider(),
                 "file-provider mode is selected for malformed file secrets test");
    ok &= expect(settings->accessToken().isEmpty(), "malformed file secrets access token falls back to empty");
    ok &= expect(settings->secret(QLatin1String("unknown")).isEmpty(),
                 "malformed file secrets map falls back to empty map");
    ok &= expect(settings->hasPersistedSessionIdentity(),
                 "session identity remains available from session.yml");
    ok &= expect(!settings->hasActiveSession(),
                 "missing token in malformed file secrets keeps session inactive");
    return ok;
}

bool
testSerializerLoggerInjection()
{
    const QString profile = QStringLiteral("serializer-logger-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "serializer logger fixture config root can be created");

    settings::serializer::setLoggers({});
    auto loggerState = settings::serializer::activeLoggers();
    if (!expect(!!loggerState.ui, "serializer defaults null-injected logger values"))
        return false;

    YAML::Node configRoot(YAML::NodeType::Map);
    configRoot["ui"]["theme"]["slug"] = "komai-light";
    if (!ctx.writeConfig(configRoot))
        return expect(false, "serializer logger fixture config can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after initialize");

    settings->setWindowWidth(1366);
    settings->setWindowHeight(768);
    settings->setSidebarsRoomListWidthPx(260);
    settings->setSidebarsCommunitiesWidthPx(240);
    const auto stateFile = ctx.stateFile();
    settings::storage::removePath(stateFile);

    settings::serializer::saveState(*settings, stateFile);
    const bool nullLoggerWrite = expect(
      settings::storage::pathExists(stateFile), "state write succeeds with null-injected serializer logger");

    auto injectedLogger = std::make_shared<spdlog::logger>(
      QStringLiteral("serializer-ui").toStdString(), std::make_shared<spdlog::sinks::null_sink_mt>());
    settings::serializer::setLoggers({.ui = injectedLogger});
    loggerState = settings::serializer::activeLoggers();
    if (!expect(loggerState.ui == injectedLogger, "serializer stores injected ui logger"))
        return false;

    const bool injectedLoggerWrite = [&] {
        settings::storage::removePath(stateFile);
        settings::serializer::saveState(*settings, stateFile);
        return settings::storage::pathExists(stateFile);
    }();

    const auto sessionFile = ctx.sessionFile();
    settings->setAccessToken(QStringLiteral(""));
    settings::storage::removePath(sessionFile);
    settings::serializer::saveSession(*settings, sessionFile);
    const bool noSessionFileWithoutToken = expect(
      !settings::storage::pathExists(sessionFile), "session save is no-op when token is missing");

    return nullLoggerWrite && injectedLoggerWrite && noSessionFileWithoutToken;
}

bool
testSettingDescriptorReadSettingValueHelper()
{
    int parsedInt = 0;
    QString parsedString;

    const bool intOk = expect(settings::ui::readSettingValue(QVariant{42}, parsedInt) &&
                                parsedInt == 42,
                              "settings descriptor helper reads int values");
    const bool strOk =
      expect(settings::ui::readSettingValue(QVariant{QStringLiteral("abc")}, parsedString) &&
               parsedString == QStringLiteral("abc"),
             "settings descriptor helper reads QString values");
    const bool rejectBadType = expect(
      !settings::ui::readSettingValue(QVariant{QVariantList{}}, parsedInt),
      "settings descriptor helper rejects incompatible types");

    return intOk && strOk && rejectBadType;
}

bool
testControllerSyncsCoreStore()
{
    const QString profile = QStringLiteral("core-store-sync-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "core store sync fixture config root can be created");

    YAML::Node configRoot(YAML::NodeType::Map);
    configRoot["ui"]["theme"]["slug"]                   = "komai-dark";
    configRoot["ui"]["font"]["size_pt"]                 = 15.5;
    configRoot["network"]["presence"]["status_policy"]  = "offline";
    configRoot["composer"]["input"]["markdown"]["enabled"] = true;
    if (!ctx.writeConfig(configRoot))
        return expect(false, "core store sync fixture config can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after initialize");

    const auto &store = settings->coreStore();
    const auto theme = store.valueAs<std::string>(settings::core::SettingId::UiThemeSlug);
    const auto fontSize = store.valueAs<double>(settings::core::SettingId::UiFontSizePt);
    const auto presence = store.valueAs<int>(settings::core::SettingId::NetworkPresenceStatusPolicy);
    const auto markdown = store.valueAs<bool>(settings::core::SettingId::ComposerInputMarkdownEnabled);

    bool ok = true;
    ok &= expect(theme.has_value() && *theme == settings->uiThemeSlug().toStdString(),
                 "controller sync stores theme value in core settings store");
    ok &= expect(fontSize.has_value() && std::abs(*fontSize - settings->uiFontSizePt()) < 0.0001,
                 "controller sync stores font size value in core settings store");
    ok &= expect(presence.has_value() &&
                   *presence == static_cast<int>(settings->networkPresenceStatusPolicy()),
                 "controller sync stores presence policy in core settings store");
    ok &= expect(markdown.has_value() && *markdown == settings->composerInputMarkdownEnabled(),
                 "controller sync stores markdown setting in core settings store");
    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (!settings::ui::facade::hasCoreStoreValueMapping(definition.id)) {
            std::cerr << "FAILED: controller bridge table missing persisted setting id "
                      << static_cast<int>(definition.id) << '\n';
            ok = false;
            continue;
        }
        const auto mappedValue =
          settings::ui::facade::coreStoreValueForSettingId(*settings, definition.id);
        if (!mappedValue.has_value()) {
            std::cerr << "FAILED: controller bridge missing persisted setting id "
                      << static_cast<int>(definition.id) << '\n';
            ok = false;
        }
    }

    settings->setPersistenceSuspended(false);
    settings->setUiThemeSlug(QStringLiteral("komai-light"));
    const auto updatedTheme =
      settings->coreStore().valueAs<std::string>(settings::core::SettingId::UiThemeSlug);
    ok &= expect(updatedTheme.has_value() && *updatedTheme == settings->uiThemeSlug().toStdString(),
                 "controller save path refreshes core settings store values");

    return ok;
}

bool
testControllerResolvesProfilePathsPerProfile()
{
    StartupSettingsTestContext ctx{QStringLiteral("profile-path-fixture")};
    if (!ctx.isValid())
        return expect(false, "profile path fixture can be created");

    const QString profileA = QStringLiteral("profile-a");
    UserSettings::initialize(profileA);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for profile path test");

    bool ok = true;
    ok &= expect(settings->profileId() == profileA, "profile id reflects initialized profile");
    ok &= expect(settings->profileDirPath() == settings::storage::profileDirPath(profileA),
                 "profile dir path resolves via storage helpers");
    ok &= expect(settings->configFilePath() == settings::storage::configFilePathForProfile(profileA),
                 "config path resolves via storage helpers");
    ok &= expect(settings->stateFilePath() == settings::storage::stateFilePathForProfile(profileA),
                 "state path resolves via storage helpers");
    ok &= expect(settings->sessionFilePath() == settings::storage::sessionFilePathForProfile(profileA),
                 "session path resolves via storage helpers");
    ok &= expect(settings->secretsFilePath() == settings::storage::secretsFilePathForProfile(profileA),
                 "secrets path resolves via storage helpers");

    const QString profileB = QStringLiteral("profile-b");
    UserSettings::initialize(profileB);
    const auto settingsAfter = UserSettings::instance();
    if (!settingsAfter)
        return expect(false, "UserSettings instance is available after profile switch");

    ok &= expect(settingsAfter->profileId() == profileB,
                 "profile id updates when reinitializing profile");
    ok &= expect(settingsAfter->profileDirPath() == settings::storage::profileDirPath(profileB),
                 "profile dir path updates with profile change");
    ok &= expect(settingsAfter->configFilePath() == settings::storage::configFilePathForProfile(profileB),
                 "config path updates with profile change");

    return ok;
}

bool
testConstrainedIntSettersRejectInvalidUpdates()
{
    const QString profile = QStringLiteral("constrained-int-setters-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "constrained-int fixture config root can be created");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for constrained-int test");

    settings->setPersistenceSuspended(false);

    settings->setUiLayoutContentMaxWidthPx(1200);
    settings->setTimelineMessagesMaxWidthPx(900);
    settings->setPrivacyWindowFocusBlurDelaySeconds(5);

    const auto baselineContentWidth = settings->uiLayoutContentMaxWidthPx();
    const auto baselineTimelineWidth = settings->timelineMessagesMaxWidthPx();
    const auto baselineBlurDelay     = settings->privacyWindowFocusBlurDelaySeconds();

    settings->setUiLayoutContentMaxWidthPx(50000);         // invalid: > 20000
    settings->setTimelineMessagesMaxWidthPx(50000);        // invalid: > 20000
    settings->setPrivacyWindowFocusBlurDelaySeconds(-3); // invalid: < 0

    bool ok = true;
    ok &= expect(settings->uiLayoutContentMaxWidthPx() == baselineContentWidth,
                 "invalid max content width update is ignored");
    ok &= expect(settings->timelineMessagesMaxWidthPx() == baselineTimelineWidth,
                 "invalid max timeline width update is ignored");
    ok &= expect(settings->privacyWindowFocusBlurDelaySeconds() == baselineBlurDelay,
                 "invalid window blur delay update is ignored");

    const auto &store = settings->coreStore();
    const auto contentWidthValue =
      store.valueAs<int>(settings::core::SettingId::UiLayoutContentMaxWidthPx);
    const auto timelineWidthValue =
      store.valueAs<int>(settings::core::SettingId::TimelineMessagesMaxWidthPx);
    const auto blurDelayValue =
      store.valueAs<int>(settings::core::SettingId::PrivacyWindowFocusBlurDelaySeconds);

    ok &= expect(contentWidthValue.has_value() && *contentWidthValue == baselineContentWidth,
                 "core store keeps previous max content width on invalid update");
    ok &= expect(timelineWidthValue.has_value() && *timelineWidthValue == baselineTimelineWidth,
                 "core store keeps previous max timeline width on invalid update");
    ok &= expect(blurDelayValue.has_value() && *blurDelayValue == baselineBlurDelay,
                 "core store keeps previous window blur delay on invalid update");

    const auto configRoot = settings::storage::loadYamlFile(ctx.configFile(), "config");
    ok &= expectScalarInt(configRoot,
                          SettingKey::UiLayoutContentMaxWidthPx,
                          baselineContentWidth,
                          "config keeps previous max content width on invalid update");
    ok &= expectScalarInt(configRoot,
                          SettingKey::TimelineMessagesMaxWidthPx,
                          baselineTimelineWidth,
                          "config keeps previous max timeline width on invalid update");
    ok &= expectScalarInt(configRoot,
                          SettingKey::PrivacyWindowFocusBlurDelaySeconds,
                          baselineBlurDelay,
                          "config keeps previous window blur delay on invalid update");

    return ok;
}

bool
testConfigSchemaCoverageAndKeyUniqueness()
{
    bool ok = true;
    const std::set<QString> schemaOnlyConfigKeys{
      QString::fromLatin1(SettingKey::DbMaxStores),
      QString::fromLatin1(SettingKey::DbMaxSizeBytes),
    };

    auto hasPersistedConfigKey = [](const QString &key) {
        for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
            if (definition.scope != settings::core::SettingScope::Config)
                continue;
            if (key == QLatin1String(definition.persistedKey))
                return true;
        }
        return false;
    };

    std::set<QString> typedKeys;
    const auto collectTyped = [&](auto descriptors, std::string_view label) {
        for (const auto &descriptor : descriptors) {
            const QString key = QString::fromLatin1(descriptor.key);
            if (key.isEmpty()) {
                std::cerr << "FAILED: empty key in " << label << '\n';
                ok = false;
                continue;
            }

            if (!typedKeys.insert(key).second) {
                std::cerr << "FAILED: duplicate typed descriptor key '" << key.toStdString()
                          << "' in " << label << '\n';
                ok = false;
            }

            if (!hasPersistedConfigKey(key) && schemaOnlyConfigKeys.count(key) == 0) {
                std::cerr << "FAILED: typed descriptor key '" << key.toStdString()
                          << "' missing persisted config definition (and not in schema-only allowlist)\n";
                ok = false;
            }
        }
    };

    collectTyped(settings::serializer::config::boolConfigSettings(), "boolConfigSettings");
    collectTyped(settings::serializer::config::intConfigSettings(), "intConfigSettings");
    collectTyped(settings::serializer::config::uintConfigSettings(), "uintConfigSettings");
    collectTyped(settings::serializer::config::ulonglongConfigSettings(), "ulonglongConfigSettings");
    collectTyped(settings::serializer::config::doubleConfigSettings(), "doubleConfigSettings");
    collectTyped(settings::serializer::config::stringConfigSettings(), "stringConfigSettings");

    std::set<QString> enumTokenKeys;
    std::set<settings::core::SettingId> enumTokenIds;
    for (const auto &adapter : settings::serializer::config::enumTokenAdapters()) {
        const QString key = QString::fromLatin1(adapter.key);

        if (!enumTokenIds.insert(adapter.id).second) {
            std::cerr << "FAILED: duplicate enum token adapter id "
                      << static_cast<int>(adapter.id) << '\n';
            ok = false;
        }

        if (!enumTokenKeys.insert(key).second) {
            std::cerr << "FAILED: duplicate enum token adapter key '" << key.toStdString() << "'\n";
            ok = false;
        }

        if (typedKeys.count(key) != 0) {
            std::cerr << "FAILED: enum token adapter key '" << key.toStdString()
                      << "' overlaps typed descriptor key set\n";
            ok = false;
        }

        if (QString::fromLatin1(adapter.defaultToken).isEmpty()) {
            std::cerr << "FAILED: enum token adapter default token is empty for key '"
                      << key.toStdString() << "'\n";
            ok = false;
        }

        const auto definition = settings::core::definitions::persistedDefinitionFor(adapter.id);
        if (!definition.has_value()) {
            std::cerr << "FAILED: enum token adapter id " << static_cast<int>(adapter.id)
                      << " has no persisted definition\n";
            ok = false;
            continue;
        }

        if (definition->scope != settings::core::SettingScope::Config) {
            std::cerr << "FAILED: enum token adapter id " << static_cast<int>(adapter.id)
                      << " is not a config-scoped persisted definition\n";
            ok = false;
        }

        if (key != QLatin1String(definition->persistedKey)) {
            std::cerr << "FAILED: enum token adapter key mismatch for id "
                      << static_cast<int>(adapter.id) << " ('" << key.toStdString() << "' vs '"
                      << definition->persistedKey << "')\n";
            ok = false;
        }
    }

    std::set<QString> serializerHandledConfigKeys = typedKeys;
    serializerHandledConfigKeys.insert(enumTokenKeys.begin(), enumTokenKeys.end());
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiThemeSlug));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiMotionAnimationsEnabled));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiInputMode));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiScaleFactor));

    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (definition.scope != settings::core::SettingScope::Config)
            continue;

        const QString key = QString::fromLatin1(definition.persistedKey);
        if (serializerHandledConfigKeys.count(key) == 0) {
            std::cerr << "FAILED: persisted config definition key '" << definition.persistedKey
                      << "' is not covered by serializer schema/adapters\n";
            ok = false;
        }
    }

    for (const auto &key : serializerHandledConfigKeys) {
        if (!hasPersistedConfigKey(key) && schemaOnlyConfigKeys.count(key) == 0) {
            std::cerr << "FAILED: serializer key '" << key.toStdString()
                      << "' has no persisted config definition\n";
            ok = false;
        }
    }

    return ok;
}

} // namespace

int
main()
{
    test_env::ScopedTestHome testHome{QStringLiteral("komai-startup-settings-test")};
    if (!testHome.isValid()) {
        std::cerr << "FAILED: test home environment can be created\n";
        return 1;
    }
    if (!testHome.isIsolated()) {
        std::cerr << "FAILED: test home environment is isolated\n";
        return 1;
    }

    int argc = 1;
    char arg0[] = "komai-startup-settings-test";
    char *argv[] = {arg0, nullptr};
    QApplication app(argc, argv);
    ThemeRegistry::initialize();

    bool ok = true;
    ok &= testStartupConfigSnapshotLoads();
    ok &= testStartupConfigSnapshotMissingProfile();
    ok &= testCoreSnapshotExtraction();
    ok &= testCoreSnapshotFromFile();
    ok &= testCoreScaleRangeHelpers();
    ok &= testStartupPolicySkipsSessionWritesUntilCompleteSession();
    ok &= testStartupPolicyConfigOnlyEditsDoNotCreateSessionOrSecrets();
    ok &= testEnumSettingsPersistAsStrings();
    ok &= testInvalidConfigTokensFallbackToSafeValues();
    ok &= testInvalidStateDimensionsFallbackToSafeValues();
    ok &= testSessionIdentityValuesAreTrimmedOnLoad();
    ok &= testMalformedSessionIdentityValuesFallbackToEmpty();
    ok &= testSessionAuthStateHelpersForIncompleteLogin();
    ok &= testConfigSchemaVersionIsStampedOnSave();
    ok &= testConfigMigrationStampsVersionWhenMissing();
    ok &= testConfigMigrationKeepsFutureVersionUntouched();
    ok &= testConfigMigrationNormalizesNonMapConfigRoot();
    ok &= testMalformedFileSecretsPayloadFallsBackSafely();
    ok &= testSerializerLoggerInjection();
    ok &= testSettingDescriptorReadSettingValueHelper();
    ok &= testControllerSyncsCoreStore();
    ok &= testControllerResolvesProfilePathsPerProfile();
    ok &= testConstrainedIntSettersRejectInvalidUpdates();
    ok &= testConfigSchemaCoverageAndKeyUniqueness();

    return ok ? 0 : 1;
}
