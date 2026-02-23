// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <iostream>
#include <string>
#include <string_view>
#include <memory>

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
#include "settings/SettingsStorage.h"
#include "settings/StartupSettings.h"
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

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after initialize");

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
    settings->setTheme(QStringLiteral("komai-dark"));

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
    settings->setPresence(UserSettings::Presence::Offline);
    settings->setShowImage(UserSettings::ShowImage::Never);
    settings->setShowSenderUsername(UserSettings::ShowSenderUsername::Always);
    settings->setAutoReplaceEmoji(UserSettings::AutoReplaceEmoji::Never);
    settings->setSendMessageKey(UserSettings::SendMessageKey::CtrlEnter);
    settings->setRoomSortOrder(UserSettings::RoomSortOrder::Alphabetical);
    settings->setShowLastMessagePreview(UserSettings::LastMessagePreview::Never);
    settings->setTimelineMessageActionsPolicy(UserSettings::TimelineMessageActionsPolicy::OnHover);
    settings->setTimelineMessageLayout(UserSettings::TimelineMessageLayout::Minimal);
    settings->setNotificationMessageContentPolicy(
      UserSettings::NotificationMessageContentPolicy::Never);
    settings->setIntegrationsDbusApiAccess(IntegrationsDbusAccessReadOnly);
    settings->setTouchInputModeEnabled(true);
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
    settings->setRoomListWidth(260);
    settings->setCommunityListWidth(240);
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
    ok &= expect(theme.has_value() && *theme == settings->theme().toStdString(),
                 "controller sync stores theme value in core settings store");
    ok &= expect(fontSize.has_value() && std::abs(*fontSize - settings->fontSize()) < 0.0001,
                 "controller sync stores font size value in core settings store");
    ok &= expect(presence.has_value() &&
                   *presence == static_cast<int>(settings->presence()),
                 "controller sync stores presence policy in core settings store");
    ok &= expect(markdown.has_value() && *markdown == settings->markdownEnabled(),
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
    settings->setTheme(QStringLiteral("komai-light"));
    const auto updatedTheme =
      settings->coreStore().valueAs<std::string>(settings::core::SettingId::UiThemeSlug);
    ok &= expect(updatedTheme.has_value() && *updatedTheme == settings->theme().toStdString(),
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
    ok &= testSerializerLoggerInjection();
    ok &= testSettingDescriptorReadSettingValueHelper();
    ok &= testControllerSyncsCoreStore();
    ok &= testControllerResolvesProfilePathsPerProfile();

    return ok ? 0 : 1;
}
