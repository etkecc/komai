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

    settings.setUiFontSizePt(snapshot.ui.has_font_size_pt
                               ? snapshot.ui.font_size_pt
                               : settings::core::definitions::kDefaultFontSizePt);
    settings.setUiFontFamily(
      QString::fromStdString(static_cast<std::string>(snapshot.ui.font_family)));
    settings.setUiFontEmojiFamily(
      QString::fromStdString(static_cast<std::string>(snapshot.ui.font_emoji_family)));
    settings.setUiMotionAnimationsEnabled(
      snapshot.ui.has_motion_animations_enabled
        ? snapshot.ui.motion_animations_enabled
        : settings::core::definitions::kDefaultUiMotionAnimationsEnabled);

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

    const auto loadedScrollbarPolicy =
      QString::fromStdString(static_cast<std::string>(snapshot.ui.scrollbar_policy)).trimmed();
    const auto scrollbarPolicyToken =
      loadedScrollbarPolicy.isEmpty()
        ? cfg::toStorageValue(UserSettings::ScrollbarPolicy::WhenNeeded)
        : loadedScrollbarPolicy;
    settings.setUiScrollbarPolicy(cfg::scrollbarPolicyFromStorage(
      scrollbarPolicyToken, UserSettings::ScrollbarPolicy::WhenNeeded));
    if (scrollbarPolicyToken != cfg::toStorageValue(settings.uiScrollbarPolicy())) {
        activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                 scrollbarPolicyToken.toStdString(),
                                 SettingKey::UiScrollbarPolicy,
                                 cfg::toStorageValue(settings.uiScrollbarPolicy()).toStdString());
    }

    const auto loadedDefaultAvatarStyle =
      QString::fromStdString(static_cast<std::string>(snapshot.ui.default_avatar_style)).trimmed();
    const auto defaultAvatarStyleToken =
      loadedDefaultAvatarStyle.isEmpty()
        ? cfg::toStorageValue(UserSettings::DefaultAvatarStyle::BoringAvatarsBauhaus)
        : loadedDefaultAvatarStyle;
    settings.setUiAvatarsDefaultAvatarStyle(cfg::defaultAvatarStyleFromStorage(
      defaultAvatarStyleToken, UserSettings::DefaultAvatarStyle::BoringAvatarsBauhaus));
    if (defaultAvatarStyleToken != cfg::toStorageValue(settings.uiAvatarsDefaultAvatarStyle())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          defaultAvatarStyleToken.toStdString(),
          SettingKey::UiAvatarsDefaultAvatarStyle,
          cfg::toStorageValue(settings.uiAvatarsDefaultAvatarStyle()).toStdString());
    }

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
    settings.setUiInputTouchSwipeGesturesEnabled(snapshot.ui.has_input_touch_swipe_gestures_enabled
                                                   ? snapshot.ui.input_touch_swipe_gestures_enabled
                                                   : false);
    settings.setUiLayoutContentMaxWidthPx(
      snapshot.ui.has_layout_content_max_width_px
        ? snapshot.ui.layout_content_max_width_px
        : settings::core::definitions::kDefaultUiLayoutContentMaxWidthPx);
    settings.setUiAvatarsCircular(snapshot.ui.has_avatars_circular ? snapshot.ui.avatars_circular
                                                                   : false);
    settings.setUiLayoutCompactMode(
      snapshot.ui.has_layout_compact_mode ? snapshot.ui.layout_compact_mode : false);

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
