// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include <QString>
#include <yaml-cpp/yaml.h>

#include "Logging.h"
#include "UserSettingsPage.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsStorage.h"
#include "settings/YamlSettings.h"

using settings::storage::writeYamlFile;
using yaml_settings::readNestedStringLists;
using yaml_settings::readScalar;
using yaml_settings::readString;
using yaml_settings::readStringList;
using yaml_settings::setNode;
using yaml_settings::writeNestedStringLists;
using yaml_settings::writeStringList;

namespace settings::serializer {

void
loadState(UserSettings &settings, const YAML::Node &root)
{
    const auto roomListWidth = readScalar<int>(root, SettingKey::SidebarsRoomListWidthPx, -1);
    const auto communityListWidth =
      readScalar<int>(root, SettingKey::SidebarsCommunitiesWidthPx, 200);

    settings.setWindowWidth(readScalar<int>(root, SettingKey::AppWindowSizeWidth, 0));
    settings.setWindowHeight(readScalar<int>(root, SettingKey::AppWindowSizeHeight, 0));
    settings.setRoomListWidth(roomListWidth < -1 ? -1 : roomListWidth);
    settings.setCommunityListWidth(communityListWidth < 0 ? 0 : communityListWidth);
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

    if (writeYamlFile(stateFilePath, root, false))
        nhlog::ui()->debug("Saved state to: {}", stateFilePath.toStdString());
}

} // namespace settings::serializer
