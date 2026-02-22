// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <iostream>
#include <string_view>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <yaml-cpp/yaml.h>

#include "UserSettingsPage.h"
#include "settings/StartupSettings.h"
#include "settings/SettingsStorage.h"
#include "settings/core/StartupConfig.h"
#include "ui/ThemeRegistry.h"

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
testStartupConfigSnapshotLoads()
{
    const QString profile = QStringLiteral("profile-startup");
    QTemporaryDir tmpRoot;
    if (!tmpRoot.isValid())
        return expect(false, "temporary config root can be created");

    qputenv("XDG_CONFIG_HOME", tmpRoot.path().toUtf8());
    const QString profileRoot = tmpRoot.path() + QStringLiteral("/komai/profiles/") + profile;
    const QString path = profileRoot + QStringLiteral("/config.yml");

    if (!QDir(profileRoot).mkpath(QStringLiteral("."))) {
        return expect(false, "temporary profile config directory can be created");
    }

    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return expect(false, "unable to create test profile config file");
    }

    YAML::Node configRoot(YAML::NodeType::Map);
    configRoot["ui"]["scale"]["factor"] = 1.75;
    configRoot["ui"]["font"]["size_pt"] = 15;
    file.write(QString::fromUtf8(YAML::Dump(configRoot)).toUtf8());
    file.close();

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
    QTemporaryDir tmpRoot;
    if (!tmpRoot.isValid())
        return expect(false, "temporary config root can be created");

    qputenv("XDG_CONFIG_HOME", tmpRoot.path().toUtf8());
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
    QTemporaryDir tmpRoot;
    if (!tmpRoot.isValid())
        return expect(false, "temporary config root can be created");

    qputenv("XDG_CONFIG_HOME", tmpRoot.path().toUtf8());

    int argc = 1;
    char arg0[] = "komai-startup-policy-test";
    char *argv[] = {arg0, nullptr};
    QApplication app(argc, argv);
    ThemeRegistry::initialize();

    const QString configFile = settings::storage::configFilePathForProfile(profile);
    const QString stateFile  = settings::storage::stateFilePathForProfile(profile);
    const QString sessionFile = settings::storage::sessionFilePathForProfile(profile);
    const QString secretsFile = settings::storage::secretsFilePathForProfile(profile);

    const auto profileDir = QFileInfo(configFile).absolutePath();
    QDir().mkpath(profileDir);

    YAML::Node configRoot(YAML::NodeType::Map);
    configRoot["secrets"]["provider"] = "file";
    configRoot["ui"]["theme"]["slug"] = "komai-light";

    if (!settings::storage::writeYamlFile(configFile, configRoot, false))
        return expect(false, "startup-policy fixture config can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after initialize");

    settings->setRunWithoutSecureSecretsService(true);
    settings->save();

    if (!expect(QFileInfo(configFile).exists(), "startup save creates config.yml in config-only mode"))
        return false;
    if (!expect(!QFileInfo(stateFile).exists() && !QFileInfo(sessionFile).exists() &&
                  !QFileInfo(secretsFile).exists(),
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

    return expect(QFileInfo(stateFile).exists(), "full persistence writes state.yml after complete snapshot") &&
           expect(QFileInfo(sessionFile).exists(),
                  "full persistence writes session.yml after complete snapshot") &&
           expect(QFileInfo(secretsFile).exists(),
                  "full persistence writes secrets.yml after complete snapshot");
}

bool
testStartupPolicyConfigOnlyEditsDoNotCreateSessionOrSecrets()
{
    const QString profile = QStringLiteral("startup-policy-config-only-profile");
    QTemporaryDir tmpRoot;
    if (!tmpRoot.isValid())
        return expect(false, "temporary config root can be created");

    qputenv("XDG_CONFIG_HOME", tmpRoot.path().toUtf8());

    int argc = 1;
    char arg0[] = "komai-startup-policy-config-only-test";
    char *argv[] = {arg0, nullptr};
    QApplication app(argc, argv);
    ThemeRegistry::initialize();

    const QString configFile = settings::storage::configFilePathForProfile(profile);
    const QString stateFile  = settings::storage::stateFilePathForProfile(profile);
    const QString sessionFile = settings::storage::sessionFilePathForProfile(profile);
    const QString secretsFile = settings::storage::secretsFilePathForProfile(profile);

    const auto profileDir = QFileInfo(configFile).absolutePath();
    QDir().mkpath(profileDir);

    YAML::Node configRoot(YAML::NodeType::Map);
    configRoot["secrets"]["provider"] = "file";
    configRoot["ui"]["theme"]["slug"] = "komai-light";
    if (!settings::storage::writeYamlFile(configFile, configRoot, false))
        return expect(false, "startup-policy-config-only fixture config can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after initialize");

    settings->setRunWithoutSecureSecretsService(true);
    settings->setPersistenceSuspended(false);
    settings->setTheme(QStringLiteral("komai-dark"));

    if (!expect(QFileInfo(configFile).exists(), "theme change creates config.yml in config-only mode"))
        return false;

    auto configAfter = settings::storage::loadYamlFile(configFile, "config-after-theme-change");
    const auto storedTheme = QString::fromStdString(configAfter["ui"]["theme"]["slug"].as<std::string>());
    const bool persistedTheme = expect(
      storedTheme == QStringLiteral("komai-dark"), "theme change is persisted to config.yml");

    return persistedTheme &&
           expect(!QFileInfo(stateFile).exists() && !QFileInfo(sessionFile).exists() &&
                    !QFileInfo(secretsFile).exists(),
                  "theme change does not create state/session/secrets files");
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testStartupConfigSnapshotLoads();
    ok &= testStartupConfigSnapshotMissingProfile();
    ok &= testCoreSnapshotExtraction();
    ok &= testCoreSnapshotFromFile();
    ok &= testCoreScaleRangeHelpers();
    ok &= testStartupPolicySkipsSessionWritesUntilCompleteSession();
    ok &= testStartupPolicyConfigOnlyEditsDoNotCreateSessionOrSecrets();

    return ok ? 0 : 1;
}
