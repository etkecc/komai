// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include <QString>

#include <yaml-cpp/yaml.h>

#include "UserSettingsPage.h"
#include "Logging.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsStorage.h"
#include "settings/YamlSettings.h"

namespace {

bool
hasSessionValue(const QString &value)
{
    return !value.trimmed().isEmpty();
}

} // namespace

using yaml_settings::readString;
using yaml_settings::setNode;
using settings::storage::writeYamlFile;

namespace settings::serializer {

void
loadSession(UserSettings &settings, const YAML::Node &root)
{
    settings.setHomeserver(readString(root, SettingKey::SessionAccountHomeserver, QString()));
    settings.setUserId(readString(root, SettingKey::SessionAccountUserId, QString()));
    settings.setDeviceId(readString(root, SettingKey::SessionDeviceId, QString()));
}

void
saveSession(const UserSettings &settings, const QString &sessionFilePath)
{
    const bool hasUserId      = hasSessionValue(settings.userId());
    const bool hasDeviceId    = hasSessionValue(settings.deviceId());
    const bool hasAccessToken = hasSessionValue(settings.accessToken());

    if (hasAccessToken && (!hasUserId || !hasDeviceId)) {
        nhlog::ui()->warn(
          "Skipping session.yml write because session identity is incomplete "
          "(has_user_id={}, has_device_id={}, has_access_token=true)",
          hasUserId,
          hasDeviceId);
        return;
    }

    YAML::Node root(YAML::NodeType::Map);
    setNode(root, SettingKey::SessionAccountUserId, settings.userId().toStdString());
    setNode(root, SettingKey::SessionAccountHomeserver, settings.homeserver().toStdString());
    setNode(root, SettingKey::SessionDeviceId, settings.deviceId().toStdString());

    if (writeYamlFile(sessionFilePath, root, false))
        nhlog::ui()->debug("Saved session to: {}", sessionFilePath.toStdString());
}

} // namespace settings::serializer
