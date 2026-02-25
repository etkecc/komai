// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include <QString>

#include <spdlog/logger.h>
#include <yaml-cpp/yaml.h>

#include "SettingsSerializerConfigInternal.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsStorage.h"
#include "settings/StagedLoadPlan.h"
#include "settings/YamlSettings.h"

using settings::storage::writeYamlFile;
using yaml_settings::setNode;

namespace settings::serializer {

void
saveConfig(const UserSettings &settings,
           const QString &configFilePath,
           bool usesFileSecretsProvider)
{
    YAML::Node root(YAML::NodeType::Map);
    detail::makeConfigNode(settings, root);

    setNode(root,
            SettingKey::SecretsProvider,
            (usesFileSecretsProvider
               ? QString::fromLatin1(staged_load_plan::ProviderFileValue)
               : QString::fromLatin1(staged_load_plan::ProviderSecretServiceValue))
              .toStdString());

    if (writeYamlFile(configFilePath, root, false)) {
        activeLoggers().ui->debug("Saved config to: {}", configFilePath.toStdString());
    }
}

} // namespace settings::serializer
