// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include "komai-rust-cxxbridge/ffi.h"

#include <QString>
#include <string>
#include <utility>
#include <variant>

#include "logging/Logging.h"

#include "SettingsSerializerConfigConverters.h"
#include "SettingsSerializerConfigInternal.h"
#include "settings/SettingKeys.h"
#include "settings/StagedLoadPlan.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/core/StartupConfig.h"

namespace settings::serializer {

namespace cfg = settings::serializer::config;

namespace {

::komai::rust::SettingsConfigValue
configValue(const char *key, bool value)
{
    return {.key                   = std::string(key),
            .kind                  = ::komai::rust::SettingsConfigValueKind::Bool,
            .bool_value            = value,
            .int_value             = 0,
            .double_value          = 0.0,
            .string_value          = {},
            .string_list_value     = {},
            .string_list_map_value = {}};
}

::komai::rust::SettingsConfigValue
configValue(const char *key, int value)
{
    return {.key                   = std::string(key),
            .kind                  = ::komai::rust::SettingsConfigValueKind::Int,
            .bool_value            = false,
            .int_value             = value,
            .double_value          = 0.0,
            .string_value          = {},
            .string_list_value     = {},
            .string_list_map_value = {}};
}

::komai::rust::SettingsConfigValue
configValue(const char *key, double value)
{
    return {.key                   = std::string(key),
            .kind                  = ::komai::rust::SettingsConfigValueKind::Double,
            .bool_value            = false,
            .int_value             = 0,
            .double_value          = value,
            .string_value          = {},
            .string_list_value     = {},
            .string_list_map_value = {}};
}

::komai::rust::SettingsConfigValue
configValue(const char *key, std::string value)
{
    return {.key                   = std::string(key),
            .kind                  = ::komai::rust::SettingsConfigValueKind::String,
            .bool_value            = false,
            .int_value             = 0,
            .double_value          = 0.0,
            .string_value          = std::move(value),
            .string_list_value     = {},
            .string_list_map_value = {}};
}

::komai::rust::SettingsConfigValue
configValue(const char *key, const QStringList &values)
{
    ::rust::Vec<::rust::String> rustValues;
    for (const auto &value : values)
        rustValues.push_back(value.toStdString());

    return {.key                   = std::string(key),
            .kind                  = ::komai::rust::SettingsConfigValueKind::StringList,
            .bool_value            = false,
            .int_value             = 0,
            .double_value          = 0.0,
            .string_value          = {},
            .string_list_value     = std::move(rustValues),
            .string_list_map_value = {}};
}

void
appendCoreStoreConfigValues(const UserSettings &settings,
                            ::rust::Vec<::komai::rust::SettingsConfigValue> &values)
{
    const auto &store = settings.coreStore();

    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (definition.scope != settings::core::SettingScope::Config)
            continue;
        if (settings::core::definitions::isEnumTokenConfigSettingId(definition.id))
            continue;
        if (definition.id == settings::core::SettingId::UiThemeSlug ||
            definition.id == settings::core::SettingId::UiFontFamily ||
            definition.id == settings::core::SettingId::UiFontSizePt ||
            definition.id == settings::core::SettingId::UiFontEmojiFamily ||
            definition.id == settings::core::SettingId::UiMotionAnimationsEnabled ||
            definition.id == settings::core::SettingId::UiInputMode ||
            definition.id == settings::core::SettingId::UiInputTouchSwipeGesturesEnabled ||
            definition.id == settings::core::SettingId::UiScaleFactor ||
            definition.id == settings::core::SettingId::UiLayoutContentMaxWidthPx ||
            definition.id == settings::core::SettingId::UiAvatarsCircular ||
            definition.id == settings::core::SettingId::UiLayoutCompactMode ||
            definition.id == settings::core::SettingId::PrivacyWindowFocusBlurEnabled ||
            definition.id == settings::core::SettingId::PrivacyWindowFocusBlurDelaySeconds ||
            definition.id == settings::core::SettingId::PrivacyMaintenanceExpireEvents ||
            definition.id == settings::core::SettingId::CallsLegacyEnabled ||
            definition.id == settings::core::SettingId::CallsRelayUseFallbackServer ||
            definition.id == settings::core::SettingId::CallsDevicesMicrophone ||
            definition.id == settings::core::SettingId::CallsDevicesCamera ||
            definition.id == settings::core::SettingId::CallsDevicesCameraResolution ||
            definition.id == settings::core::SettingId::CallsDevicesCameraFrameRate ||
            definition.id == settings::core::SettingId::CallsAudioRingtone ||
            definition.id == settings::core::SettingId::CallsScreenshareFrameRate ||
            definition.id == settings::core::SettingId::CallsScreensharePictureInPicture ||
            definition.id == settings::core::SettingId::CallsScreenshareIncludeRemoteVideo ||
            definition.id == settings::core::SettingId::CallsScreenshareShowCursor ||
            definition.id == settings::core::SettingId::NotificationsEnabled ||
            definition.id == settings::core::SettingId::NotificationsAttentionOnIncoming)
            continue;

        const auto stored = store.value(definition.id);
        if (!stored.has_value()) {
            activeLoggers().ui->warn("Missing core-store value for config key '{}'",
                                     definition.persistedKey);
            continue;
        }

        if (const auto *value = std::get_if<bool>(&*stored)) {
            values.push_back(configValue(definition.persistedKey, *value));
        } else if (const auto *value = std::get_if<int>(&*stored)) {
            values.push_back(configValue(definition.persistedKey, *value));
        } else if (const auto *value = std::get_if<double>(&*stored)) {
            values.push_back(configValue(definition.persistedKey, *value));
        } else if (const auto *value = std::get_if<std::string>(&*stored)) {
            values.push_back(configValue(definition.persistedKey, *value));
        } else if (const auto *value =
                     std::get_if<settings::core::SettingsStore::StringList>(&*stored)) {
            QStringList stringList;
            for (const auto &entry : *value)
                stringList.push_back(QString::fromStdString(entry));
            values.push_back(configValue(definition.persistedKey, stringList));
        } else {
            activeLoggers().ui->warn("Unsupported core-store variant for config key '{}'",
                                     definition.persistedKey);
        }
    }
}

} // namespace

void
saveConfig(const UserSettings &settings,
           const QString &configFilePath,
           bool usesFileSecretsProvider)
{
    ::komai::rust::SettingsConfigSnapshot snapshot{
      .ui =
        {
          .has_scale_factor  = settings::core::isScaleFactorInRange(settings.uiScaleFactor()),
          .scale_factor      = static_cast<float>(settings.uiScaleFactor()),
          .theme_slug        = settings.uiThemeSlug().toStdString(),
          .has_font_size_pt  = true,
          .font_size_pt      = settings.uiFontSizePt(),
          .font_family       = settings.uiFontFamily().toStdString(),
          .font_emoji_family = settings.uiFontEmojiFamilyStorageValue().toStdString(),
          .has_motion_animations_enabled = true,
          .motion_animations_enabled     = settings.uiMotionAnimationsEnabled(),
          .input_mode = detail::toStorageUiInputMode(settings.uiInputMode()).toStdString(),
          .has_input_touch_swipe_gestures_enabled = true,
          .input_touch_swipe_gestures_enabled     = settings.uiInputTouchSwipeGesturesEnabled(),
          .has_layout_content_max_width_px        = true,
          .layout_content_max_width_px            = settings.uiLayoutContentMaxWidthPx(),
          .has_layout_compact_mode                = true,
          .layout_compact_mode                    = settings.uiLayoutCompactMode(),
          .has_avatars_circular                   = true,
          .avatars_circular                       = settings.uiAvatarsCircular(),
          .scrollbar_policy                       = {},
          .default_avatar_style                   = {},
        },
      .timeline =
        {
          .hidden_events =
            {
              .has_global = true,
              .global     = {},
              .by_room    = {},
            },
        },
      .secrets =
        {
          .provider = (usesFileSecretsProvider
                         ? QString::fromLatin1(staged_load_plan::ProviderFileValue)
                         : QString::fromLatin1(staged_load_plan::ProviderSecretServiceValue))
                        .toStdString(),
        },
      .privacy =
        {
          .window_focus_blur =
            {
              .has_enabled       = true,
              .enabled           = settings.privacyWindowFocusBlurEnabled(),
              .has_delay_seconds = true,
              .delay_seconds     = settings.privacyWindowFocusBlurDelaySeconds(),
            },
          .maintenance =
            {
              .has_expire_events = true,
              .expire_events     = settings.privacyMaintenanceExpireEvents(),
            },
        },
      .calls =
        {
          .legacy =
            {
              .has_enabled = true,
              .enabled     = settings.callsLegacyEnabled(),
            },
          .relay =
            {
              .has_use_fallback_server = true,
              .use_fallback_server     = settings.callsRelayUseFallbackServer(),
            },
          .devices =
            {
              .microphone        = settings.callsDevicesMicrophone().toStdString(),
              .camera            = settings.callsDevicesCamera().toStdString(),
              .camera_resolution = settings.callsDevicesCameraResolution().toStdString(),
              .camera_frame_rate = settings.callsDevicesCameraFrameRate().toStdString(),
            },
          .audio =
            {
              .ringtone = settings.callsAudioRingtone().toStdString(),
            },
          .screenshare =
            {
              .has_frame_rate           = true,
              .frame_rate               = settings.callsScreenshareFrameRate(),
              .has_picture_in_picture   = true,
              .picture_in_picture       = settings.callsScreensharePictureInPicture(),
              .has_include_remote_video = true,
              .include_remote_video     = settings.callsScreenshareIncludeRemoteVideo(),
              .has_show_cursor          = true,
              .show_cursor              = settings.callsScreenshareShowCursor(),
            },
        },
      .notifications =
        {
          .has_enabled               = true,
          .enabled                   = settings.notificationsEnabled(),
          .has_attention_on_incoming = true,
          .attention_on_incoming     = settings.notificationsAttentionOnIncoming(),
          .message_content_policy =
            cfg::toStorageValue(settings.notificationsMessageContentPolicy()).toStdString(),
        },
      .values = {},
    };

    appendCoreStoreConfigValues(settings, snapshot.values);

    for (const auto &adapter : config::enumTokenAdapters()) {
        if (adapter.id == settings::core::SettingId::UiScrollbarPolicy ||
            adapter.id == settings::core::SettingId::UiAvatarsDefaultAvatarStyle ||
            adapter.id == settings::core::SettingId::NotificationsMessageContentPolicy)
            continue;
        snapshot.values.push_back(
          configValue(adapter.key, adapter.toStorage(settings).toStdString()));
    }

    snapshot.ui.scrollbar_policy = cfg::toStorageValue(settings.uiScrollbarPolicy()).toStdString();
    snapshot.ui.default_avatar_style =
      cfg::toStorageValue(settings.uiAvatarsDefaultAvatarStyle()).toStdString();

    for (const auto &value : settings.hiddenTimelineEventTypes())
        snapshot.timeline.hidden_events.global.push_back(value.toStdString());
    for (auto it = settings.hiddenTimelineEventTypesByRoom().constBegin();
         it != settings.hiddenTimelineEventTypesByRoom().constEnd();
         ++it) {
        ::rust::Vec<::rust::String> rustValues;
        for (const auto &value : it.value())
            rustValues.push_back(value.toStdString());
        snapshot.timeline.hidden_events.by_room.push_back(
          {.key = it.key().toStdString(), .values = std::move(rustValues)});
    }

    if (::komai::rust::settings_write_config_snapshot_to_path(configFilePath.toStdString(),
                                                              snapshot)) {
        activeLoggers().ui->debug("Saved config to: {}", configFilePath.toStdString());
    }
}

} // namespace settings::serializer
