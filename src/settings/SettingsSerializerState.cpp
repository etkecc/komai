// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include <QString>
#include <yaml-cpp/yaml.h>

#include <spdlog/logger.h>

#include "settings/SettingKeys.h"
#include "settings/SettingsMigrations.h"
#include "settings/SettingsStorage.h"
#include "settings/YamlSettings.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsPage.h"

using settings::storage::writeYamlFile;
using yaml_settings::readScalar;
using yaml_settings::readString;
using yaml_settings::readStringList;
using yaml_settings::readStringMap;
using yaml_settings::setNode;
using yaml_settings::writeStringList;
using yaml_settings::writeStringMap;

namespace settings::serializer {

namespace {

int
readNormalizedStateInt(const YAML::Node &root,
                       const char *key,
                       int defaultValue,
                       int (*normalize)(int))
{
    const auto rawValue        = readScalar<int>(root, key, defaultValue);
    const auto normalizedValue = normalize(rawValue);
    if (rawValue != normalizedValue) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'", rawValue, key, normalizedValue);
    }
    return normalizedValue;
}

} // namespace

void
loadState(UserSettings &settings, const YAML::Node &root)
{
    settings.setWindowWidth(
      readNormalizedStateInt(root,
                             SettingKey::UiWindowWidthPx,
                             settings::core::definitions::kDefaultWindowWidthPx,
                             settings::core::definitions::normalizeWindowWidthPx));
    settings.setWindowHeight(
      readNormalizedStateInt(root,
                             SettingKey::UiWindowHeightPx,
                             settings::core::definitions::kDefaultWindowHeightPx,
                             settings::core::definitions::normalizeWindowHeightPx));
    settings.setSidebarsRoomListWidthPx(
      readNormalizedStateInt(root,
                             SettingKey::SidebarsRoomListWidthPx,
                             settings::core::definitions::kDefaultSidebarsRoomListWidthPx,
                             settings::core::definitions::normalizeRoomListWidthPx));
    settings.setSidebarsCommunitiesWidthPx(
      readNormalizedStateInt(root,
                             SettingKey::SidebarsCommunitiesWidthPx,
                             settings::core::definitions::kDefaultSidebarsCommunitiesWidthPx,
                             settings::core::definitions::normalizeCommunitiesWidthPx));
    settings.setCurrentFilterId(
      readString(root, SettingKey::SidebarsCommunitiesFilteringCurrent, QString()));
    settings.setCurrentRoomId(
      readString(root, SettingKey::SidebarsRoomListCurrentRoomId, QString()));
    settings.setGlobalExcludes(
      readStringList(root, SettingKey::SidebarsCommunitiesFilteringGlobalExcludes));
    settings.setBadgesHiddenFilters(
      readStringList(root,
                     SettingKey::SidebarsCommunitiesFilteringBadgesHidden,
                     QStringList{QStringLiteral("global"), QStringLiteral("tag:m.lowpriority")}));
    settings.setHiddenPins(readStringList(root, SettingKey::TimelinePinsHidden));
    settings.setHiddenWidgets(readStringList(root, SettingKey::TimelineWidgetsHidden));
    settings.setComposerDraftsByRoom(readStringMap(root, SettingKey::ComposerDraftsByRoom));
    settings.setCollapsedSpaces(
      readStringList(root, SettingKey::SidebarsCommunitiesFilteringCollapsedSpaces));
}

void
saveState(const UserSettings &settings, const QString &stateFilePath)
{
    YAML::Node root(YAML::NodeType::Map);
    settings::migrations::stampCurrentStateSchemaVersion(root);

    setNode(root, SettingKey::UiWindowWidthPx, settings.windowWidth());
    setNode(root, SettingKey::UiWindowHeightPx, settings.windowHeight());
    setNode(root, SettingKey::SidebarsRoomListWidthPx, settings.sidebarsRoomListWidthPx());
    setNode(root, SettingKey::SidebarsCommunitiesWidthPx, settings.sidebarsCommunitiesWidthPx());
    setNode(root,
            SettingKey::SidebarsCommunitiesFilteringCurrent,
            settings.currentFilterId().toStdString());
    setNode(
      root, SettingKey::SidebarsRoomListCurrentRoomId, settings.currentRoomId().toStdString());
    writeStringList(
      root, SettingKey::SidebarsCommunitiesFilteringGlobalExcludes, settings.globalExcludes());
    writeStringList(
      root, SettingKey::SidebarsCommunitiesFilteringBadgesHidden, settings.badgesHiddenFilters());
    writeStringList(
      root, SettingKey::SidebarsCommunitiesFilteringCollapsedSpaces, settings.collapsedSpaces());
    writeStringList(root, SettingKey::TimelinePinsHidden, settings.hiddenPins());
    writeStringList(root, SettingKey::TimelineWidgetsHidden, settings.hiddenWidgets());
    writeStringMap(root, SettingKey::ComposerDraftsByRoom, settings.composerDraftsByRoom());

    if (writeYamlFile(stateFilePath, root, false)) {
        activeLoggers().ui->debug("Saved state to: {}", stateFilePath.toStdString());
    }
}

} // namespace settings::serializer
