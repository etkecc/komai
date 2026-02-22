// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <iostream>
#include <string_view>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QStandardPaths>

#include <yaml-cpp/yaml.h>

#include "settings/StartupSettings.h"
#include "settings/core/StartupConfig.h"

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
    QStandardPaths::setTestModeEnabled(true);
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
    QStandardPaths::setTestModeEnabled(true);
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
    return expect(!snapshot.uiScaleFactor.has_value(), "core snapshot ignores malformed scale factor");
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

    return ok ? 0 : 1;
}
