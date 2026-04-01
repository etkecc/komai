// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerLoad.h"

#include "SettingsRustConfigValues.h"
#include "SettingsSerializer.h"

#include <QString>

#include "logging/Logging.h"

#include "SettingsSerializerConfigConverters.h"
#include "SettingsSerializerConfigInternal.h"
#include "SettingsSerializerConfigSchema.h"
#include "settings/SettingKeys.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/core/StartupConfig.h"
#include "timeline/TimelineEventTypes.h"

namespace cfg      = settings::serializer::config;
namespace rust_cfg = settings::rust_config_values;

namespace settings::serializer {

void
loadConfig(UserSettings &settings, const ::rust::Vec<::komai::rust::SettingsConfigValue> &values)
{
    cfg::validateConfigSchemaDescriptors();

    for (const auto &descriptor : cfg::boolConfigSettings()) {
        (settings.*descriptor.setter)(
          rust_cfg::readBoolValue(values, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::intConfigSettings()) {
        (settings.*descriptor.setter)(
          rust_cfg::readIntValue(values, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::uintConfigSettings()) {
        (settings.*descriptor.setter)(static_cast<uint>(
          rust_cfg::readIntValue(values, descriptor.key, descriptor.defaultValue)));
    }
    for (const auto &descriptor : cfg::ulonglongConfigSettings()) {
        (settings.*descriptor.setter)(static_cast<qulonglong>(rust_cfg::readIntValue(
          values, descriptor.key, static_cast<int>(descriptor.defaultValue))));
    }
    for (const auto &descriptor : cfg::doubleConfigSettings()) {
        (settings.*descriptor.setter)(
          rust_cfg::readDoubleValue(values, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::stringConfigSettings()) {
        (settings.*descriptor.setter)(
          rust_cfg::readStringValue(values, descriptor.key, descriptor.defaultValue));
    }

    const auto requestedTheme =
      rust_cfg::readStringValue(values, SettingKey::UiThemeSlug, settings.uiThemeSlug());
    settings.setUiThemeSlug(requestedTheme);
    if (settings.uiThemeSlug() != requestedTheme) {
        activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                 requestedTheme.toStdString(),
                                 SettingKey::UiThemeSlug,
                                 settings.uiThemeSlug().toStdString());
    }

    for (const auto &adapter : cfg::enumTokenAdapters()) {
        const auto rawToken =
          rust_cfg::readStringValue(values, adapter.key, QString::fromLatin1(adapter.defaultToken));
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
      rust_cfg::readBoolValue(values,
                              SettingKey::UiMotionAnimationsEnabled,
                              settings::core::definitions::kDefaultUiMotionAnimationsEnabled));
    const auto inputModeToken = rust_cfg::readStringValue(
      values,
      SettingKey::UiInputMode,
      detail::toStorageUiInputMode(settings::core::definitions::kDefaultUiInputMode));
    if (!detail::isKnownUiInputModeToken(inputModeToken)) {
        activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                 inputModeToken.toStdString(),
                                 SettingKey::UiInputMode,
                                 detail::toStorageUiInputMode(false).toStdString());
    }
    settings.setUiInputMode(detail::fromStorageUiInputMode(inputModeToken));

    const auto scaleFactor = rust_cfg::readDoubleValue(
      values, SettingKey::UiScaleFactor, settings::core::definitions::kDefaultScaleFactor);
    if (settings::core::isScaleFactorInRange(scaleFactor)) {
        settings.setUiScaleFactor(scaleFactor);
    } else {
        if (rust_cfg::find(values, SettingKey::UiScaleFactor)) {
            activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                     scaleFactor,
                                     SettingKey::UiScaleFactor,
                                     settings::core::definitions::kDefaultScaleFactor);
        }
        settings.setUiScaleFactor(settings::core::definitions::kDefaultScaleFactor);
    }

    settings.setHiddenTimelineEventTypes(
      rust_cfg::readStringListValue(values,
                                    SettingKey::TimelineHiddenEventsGlobal,
                                    qml_mtx_events::defaultHiddenTimelineEventTypeKeys()));
    settings.setHiddenTimelineEventTypesByRoom(
      rust_cfg::readStringListMapValue(values, SettingKey::TimelineHiddenEventsByRoom));
}

} // namespace settings::serializer
