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
#include "settings/ui/facade/UserSettingsPage.h"

namespace {

bool
hasSessionValue(const QString &value)
{
    return !value.trimmed().isEmpty();
}

QString
readNormalizedSessionValue(const YAML::Node &root, const char *key)
{
    const auto rawValue        = yaml_settings::readString(root, key, QString());
    const auto normalizedValue = rawValue.trimmed();
    if (rawValue != normalizedValue) {
        settings::serializer::activeLoggers().ui->warn(
          "Normalized value for '{}' while loading session identity", key);
    }
    return normalizedValue;
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
      .userId      = readNormalizedSessionValue(root, SettingKey::SessionAccountUserId),
      .accessToken = QString(),
      .deviceId    = readNormalizedSessionValue(root, SettingKey::SessionDeviceId),
      .homeserver  = readNormalizedSessionValue(root, SettingKey::SessionAccountHomeserver)});
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
