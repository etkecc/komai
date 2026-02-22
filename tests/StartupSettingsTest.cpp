// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <iostream>
#include <string_view>

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    QStandardPaths::setTestModeEnabled(true);
    const QString profileRoot = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                               QStringLiteral("/komai/profiles/") + profile;
    const QString path = profileRoot + QStringLiteral("/config.yml");

    const auto profileDir = QFileInfo(path).dir();
    if (!profileDir.mkpath(profileDir.absolutePath())) {
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

} // namespace

int
main()
{
    bool ok = true;
    ok &= testStartupConfigSnapshotLoads();
    ok &= testStartupConfigSnapshotMissingProfile();
    ok &= testCoreSnapshotExtraction();

    return ok ? 0 : 1;
}
