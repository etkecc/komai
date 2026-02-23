// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include <QString>

#include <yaml-cpp/yaml.h>

#include <spdlog/logger.h>

#include "UserSettingsPage.h"
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

using settings::storage::writeYamlFile;
using yaml_settings::readString;
using yaml_settings::setNode;

namespace settings::serializer {

void
loadSession(UserSettings &settings, const YAML::Node &root)
{
    settings.setSessionSnapshot(UserSettings::SessionSnapshot{
      .userId      = readString(root, SettingKey::SessionAccountUserId, QString()),
      .accessToken = QString(),
      .deviceId    = readString(root, SettingKey::SessionDeviceId, QString()),
      .homeserver  = readString(root, SettingKey::SessionAccountHomeserver, QString())});
}

void
saveSession(const UserSettings &settings, const QString &sessionFilePath)
{
    const bool hasUserId      = hasSessionValue(settings.userId());
    const bool hasDeviceId    = hasSessionValue(settings.deviceId());
    const bool hasHomeserver  = hasSessionValue(settings.homeserver());
    const bool hasAccessToken = hasSessionValue(settings.accessToken());

    if (!hasAccessToken)
        return;

    if (!hasUserId || !hasDeviceId || !hasHomeserver) {
        activeLoggers().ui->warn(
          "Skipping session.yml write because session identity is incomplete "
          "(has_user_id={}, has_device_id={}, has_homeserver={})",
          hasUserId,
          hasDeviceId,
          hasHomeserver);
        return;
    }

    YAML::Node root(YAML::NodeType::Map);
    setNode(root, SettingKey::SessionAccountUserId, settings.userId().toStdString());
    setNode(root, SettingKey::SessionAccountHomeserver, settings.homeserver().toStdString());
    setNode(root, SettingKey::SessionDeviceId, settings.deviceId().toStdString());

    if (writeYamlFile(sessionFilePath, root, false)) {
        activeLoggers().ui->debug("Saved session to: {}", sessionFilePath.toStdString());
    }
}

} // namespace settings::serializer
