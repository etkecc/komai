// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerLoad.h"

#include "SettingsSerializer.h"

#include <QString>

#include "logging/Logging.h"
#include <yaml-cpp/yaml.h>

#include "SettingsSerializerConfigConverters.h"
#include "SettingsSerializerConfigInternal.h"
#include "settings/SettingKeys.h"
#include "settings/YamlSettings.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/core/StartupConfig.h"
#include "timeline/TimelineEventTypes.h"

namespace cfg = settings::serializer::config;

using yaml_settings::getNode;
using yaml_settings::readScalar;
using yaml_settings::readString;
using yaml_settings::readStringList;
using yaml_settings::readStringListMap;

namespace settings::serializer {

void
loadConfig(UserSettings &settings, const YAML::Node &root)
{
    detail::loadConfigByType(settings, root);

    const auto requestedTheme = readString(root, SettingKey::UiThemeSlug, settings.uiThemeSlug());
    settings.setUiThemeSlug(requestedTheme);
    if (settings.uiThemeSlug() != requestedTheme) {
        activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                 requestedTheme.toStdString(),
                                 SettingKey::UiThemeSlug,
                                 settings.uiThemeSlug().toStdString());
    }

    for (const auto &adapter : cfg::enumTokenAdapters()) {
        const auto rawToken =
          readString(root, adapter.key, QString::fromLatin1(adapter.defaultToken));
        adapter.applyFromStorage(settings, rawToken);
        const auto appliedToken = adapter.toStorage(settings);
        if (rawToken != appliedToken) {
            activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                     rawToken.toStdString(),
                                     adapter.key,
                                     appliedToken.toStdString());
        }
    }

    settings.setUiMotionAnimationsEnabled(
      readScalar<bool>(root,
                       SettingKey::UiMotionAnimationsEnabled,
                       settings::core::definitions::kDefaultUiMotionAnimationsEnabled));
    const auto inputModeToken =
      readString(root,
                 SettingKey::UiInputMode,
                 detail::toStorageUiInputMode(settings::core::definitions::kDefaultUiInputMode));
    if (!detail::isKnownUiInputModeToken(inputModeToken)) {
        activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                 inputModeToken.toStdString(),
                                 SettingKey::UiInputMode,
                                 detail::toStorageUiInputMode(false).toStdString());
    }
    settings.setUiInputMode(detail::fromStorageUiInputMode(inputModeToken));

    const auto scaleFactor = readScalar<double>(
      root, SettingKey::UiScaleFactor, settings::core::definitions::kDefaultScaleFactor);
    if (settings::core::isScaleFactorInRange(scaleFactor)) {
        settings.setUiScaleFactor(scaleFactor);
    } else {
        const auto scaleFactorNode = getNode(root, SettingKey::UiScaleFactor);
        if (scaleFactorNode && scaleFactorNode.IsScalar()) {
            activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                     scaleFactor,
                                     SettingKey::UiScaleFactor,
                                     settings::core::definitions::kDefaultScaleFactor);
        }
        settings.setUiScaleFactor(settings::core::definitions::kDefaultScaleFactor);
    }

    settings.setHiddenTimelineEventTypes(
      readStringList(root,
                     SettingKey::TimelineHiddenEventsGlobal,
                     qml_mtx_events::defaultHiddenTimelineEventTypeKeys()));
    settings.setHiddenTimelineEventTypesByRoom(
      readStringListMap(root, SettingKey::TimelineHiddenEventsByRoom));
}

} // namespace settings::serializer
