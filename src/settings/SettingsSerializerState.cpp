// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include <QString>
#include <yaml-cpp/yaml.h>

#include <spdlog/logger.h>

#include "settings/SettingKeys.h"
#include "settings/SettingsStorage.h"
#include "settings/YamlSettings.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsPage.h"

using settings::storage::writeYamlFile;
using yaml_settings::readNestedStringLists;
using yaml_settings::readScalar;
using yaml_settings::readString;
using yaml_settings::readStringList;
using yaml_settings::setNode;
using yaml_settings::writeNestedStringLists;
using yaml_settings::writeStringList;

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
                             SettingKey::AppWindowSizeWidth,
                             settings::core::definitions::kDefaultWindowWidthPx,
                             settings::core::definitions::normalizeWindowWidthPx));
    settings.setWindowHeight(
      readNormalizedStateInt(root,
                             SettingKey::AppWindowSizeHeight,
                             settings::core::definitions::kDefaultWindowHeightPx,
                             settings::core::definitions::normalizeWindowHeightPx));
    settings.setRoomListWidth(
      readNormalizedStateInt(root,
                             SettingKey::SidebarsRoomListWidthPx,
                             settings::core::definitions::kDefaultSidebarsRoomListWidthPx,
                             settings::core::definitions::normalizeRoomListWidthPx));
    settings.setCommunityListWidth(
      readNormalizedStateInt(root,
                             SettingKey::SidebarsCommunitiesWidthPx,
                             settings::core::definitions::kDefaultSidebarsCommunitiesWidthPx,
                             settings::core::definitions::normalizeCommunitiesWidthPx));
    settings.setCurrentTagId(
      readString(root, SettingKey::SessionNavigationCurrentTagId, QString()));
    settings.setHiddenTags(readStringList(root, SettingKey::SidebarsCommunitiesHiddenTags));
    settings.setMutedTags(readStringList(
      root, SettingKey::SidebarsCommunitiesMutedTags, QStringList{QStringLiteral("global")}));
    settings.setHiddenPins(readStringList(root, SettingKey::TimelinePinsHidden));
    settings.setHiddenWidgets(readStringList(root, SettingKey::TimelineWidgetsHidden));
    settings.setRecentReactions(readStringList(root, SettingKey::ComposerReactionsRecent));
    settings.setCollapsedSpaces(
      readNestedStringLists(root, SettingKey::SidebarsCommunitiesCollapsedSpaces));
}

void
saveState(const UserSettings &settings, const QString &stateFilePath)
{
    YAML::Node root(YAML::NodeType::Map);

    setNode(root, SettingKey::AppWindowSizeWidth, settings.windowWidth());
    setNode(root, SettingKey::AppWindowSizeHeight, settings.windowHeight());
    setNode(root, SettingKey::SidebarsRoomListWidthPx, settings.roomListWidth());
    setNode(root, SettingKey::SidebarsCommunitiesWidthPx, settings.communityListWidth());
    setNode(root, SettingKey::SessionNavigationCurrentTagId, settings.currentTagId().toStdString());
    writeStringList(root, SettingKey::SidebarsCommunitiesHiddenTags, settings.hiddenTags());
    writeStringList(root, SettingKey::SidebarsCommunitiesMutedTags, settings.mutedTags());
    writeNestedStringLists(
      root, SettingKey::SidebarsCommunitiesCollapsedSpaces, settings.collapsedSpaces());
    writeStringList(root, SettingKey::TimelinePinsHidden, settings.hiddenPins());
    writeStringList(root, SettingKey::TimelineWidgetsHidden, settings.hiddenWidgets());
    writeStringList(root, SettingKey::ComposerReactionsRecent, settings.recentReactions());

    if (writeYamlFile(stateFilePath, root, false)) {
        activeLoggers().ui->debug("Saved state to: {}", stateFilePath.toStdString());
    }
}

} // namespace settings::serializer
