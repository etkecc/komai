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
loadConfig(UserSettings &settings, const ::komai::rust::SettingsLoadedConfig &snapshot)
{
    const auto &values = snapshot.values;
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
      QString::fromStdString(static_cast<std::string>(snapshot.ui.theme_slug)).trimmed().isEmpty()
        ? settings.uiThemeSlug()
        : QString::fromStdString(static_cast<std::string>(snapshot.ui.theme_slug)).trimmed();
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
    const auto loadedInputModeToken =
      QString::fromStdString(static_cast<std::string>(snapshot.ui.input_mode)).trimmed();
    const auto inputModeToken =
      loadedInputModeToken.isEmpty()
        ? detail::toStorageUiInputMode(settings::core::definitions::kDefaultUiInputMode)
        : loadedInputModeToken;
    if (!detail::isKnownUiInputModeToken(inputModeToken)) {
        activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                 inputModeToken.toStdString(),
                                 SettingKey::UiInputMode,
                                 detail::toStorageUiInputMode(false).toStdString());
    }
    settings.setUiInputMode(detail::fromStorageUiInputMode(inputModeToken));

    settings.setUiScaleFactor(snapshot.ui.has_scale_factor
                                ? snapshot.ui.scale_factor
                                : settings::core::definitions::kDefaultScaleFactor);

    settings.setHiddenTimelineEventTypes(
      snapshot.timeline.hidden_events.has_global
        ? [&snapshot]() {
              QStringList values;
              for (const auto &value : snapshot.timeline.hidden_events.global)
                  values.push_back(QString::fromStdString(static_cast<std::string>(value)));
              return values;
          }()
        : qml_mtx_events::defaultHiddenTimelineEventTypeKeys());
    {
        QMap<QString, QStringList> byRoom;
        for (const auto &entry : snapshot.timeline.hidden_events.by_room) {
            QStringList roomValues;
            for (const auto &value : entry.values)
                roomValues.push_back(QString::fromStdString(static_cast<std::string>(value)));
            byRoom.insert(QString::fromStdString(static_cast<std::string>(entry.key)), roomValues);
        }
        settings.setHiddenTimelineEventTypesByRoom(byRoom);
    }
}

} // namespace settings::serializer
