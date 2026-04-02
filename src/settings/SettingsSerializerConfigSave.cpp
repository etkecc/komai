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
            definition.id == settings::core::SettingId::SidebarsRoomListShowLastMessageTime ||
            definition.id == settings::core::SettingId::SidebarsRoomListShowCommunityCounts ||
            definition.id == settings::core::SettingId::SidebarsCommunitiesVisible ||
            definition.id == settings::core::SettingId::SidebarsCommunitiesFilterFavourites ||
            definition.id == settings::core::SettingId::SidebarsCommunitiesFilterPeople ||
            definition.id == settings::core::SettingId::SidebarsCommunitiesFilterBots ||
            definition.id == settings::core::SettingId::SidebarsCommunitiesFilterGroups ||
            definition.id == settings::core::SettingId::SidebarsCommunitiesFilterServerNotices ||
            definition.id == settings::core::SettingId::SidebarsCommunitiesFilterLowPriority ||
            definition.id == settings::core::SettingId::TimelineMessagesLayoutSmallAvatars ||
            definition.id == settings::core::SettingId::TimelineMessagesLayoutShowOwnAvatar ||
            definition.id == settings::core::SettingId::TimelineMessagesEmojiOnlyEnlarge ||
            definition.id == settings::core::SettingId::TimelineMessagesHoverHighlight ||
            definition.id == settings::core::SettingId::TimelineFormattedCodeSyntaxHighlighting ||
            definition.id == settings::core::SettingId::TimelineTypingShowEnabled ||
            definition.id == settings::core::SettingId::TimelineReadReceiptsEnabled ||
            definition.id == settings::core::SettingId::TimelineMessageActionsPinnedReactions ||
            definition.id == settings::core::SettingId::TimelineMediaEffectsEnabled ||
            definition.id == settings::core::SettingId::TimelineMediaAnimateOnHover ||
            definition.id == settings::core::SettingId::TimelineMediaOpenImagesExternal ||
            definition.id == settings::core::SettingId::TimelineMediaOpenVideosExternal ||
            definition.id == settings::core::SettingId::TimelineMediaAutoplayGifVideos ||
            definition.id == settings::core::SettingId::TimelineMediaOpenAudioExternal ||
            definition.id == settings::core::SettingId::TimelineMediaDefaultAudioPlaybackSpeed ||
            definition.id == settings::core::SettingId::PrivacyWindowFocusBlurEnabled ||
            definition.id == settings::core::SettingId::PrivacyWindowFocusBlurDelaySeconds ||
            definition.id == settings::core::SettingId::PrivacyMaintenanceExpireEvents ||
            definition.id == settings::core::SettingId::EncryptionKeySharingOnlyVerifiedUsers ||
            definition.id == settings::core::SettingId::EncryptionKeySharingShareWithTrusted ||
            definition.id == settings::core::SettingId::EncryptionBackupOnlineEnabled ||
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
            definition.id == settings::core::SettingId::NotificationsAttentionOnIncoming ||
            definition.id == settings::core::SettingId::NetworkTlsEnableCertificateValidation ||
            definition.id == settings::core::SettingId::NetworkMrsEnabled ||
            definition.id == settings::core::SettingId::NetworkMrsServerName ||
            definition.id == settings::core::SettingId::NetworkHttp3Enabled ||
            definition.id == settings::core::SettingId::IntegrationsSystemTrayEnabled ||
            definition.id == settings::core::SettingId::IntegrationsSystemTrayAutostart ||
            definition.id == settings::core::SettingId::IntegrationsBrowserCommand ||
            definition.id == settings::core::SettingId::ComposerInputMarkdownToHtmlEnabled ||
            definition.id == settings::core::SettingId::ComposerInputInlineEmojiPickerEnabled ||
            definition.id == settings::core::SettingId::ComposerInputInlineRoomPickerEnabled ||
            definition.id == settings::core::SettingId::ComposerInputInlineUserPickerEnabled ||
            definition.id == settings::core::SettingId::ComposerTypingSendEnabled ||
            definition.id == settings::core::SettingId::ComposerExtrasStickersEnabled)
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
      .sidebars =
        {
          .room_list =
            {
              .has_show_last_message_time = true,
              .show_last_message_time     = settings.sidebarsRoomListShowLastMessageTime(),
              .last_message_preview =
                cfg::toStorageValue(settings.sidebarsRoomListLastMessagePreview()).toStdString(),
              .has_show_community_counts = true,
              .show_community_counts     = settings.sidebarsRoomListShowCommunityCounts(),
              .sort = cfg::toStorageValue(settings.sidebarsRoomListSort()).toStdString(),
              .unread_detection_policy =
                cfg::toStorageValue(settings.sidebarsRoomListUnreadDetectionPolicy()).toStdString(),
            },
          .communities =
            {
              .has_visible               = true,
              .visible                   = settings.sidebarsCommunitiesVisible(),
              .has_filter_favourites     = true,
              .filter_favourites         = settings.sidebarsCommunitiesFilterFavourites(),
              .has_filter_people         = true,
              .filter_people             = settings.sidebarsCommunitiesFilterPeople(),
              .has_filter_bots           = true,
              .filter_bots               = settings.sidebarsCommunitiesFilterBots(),
              .has_filter_groups         = true,
              .filter_groups             = settings.sidebarsCommunitiesFilterGroups(),
              .has_filter_server_notices = true,
              .filter_server_notices     = settings.sidebarsCommunitiesFilterServerNotices(),
              .has_filter_low_priority   = true,
              .filter_low_priority       = settings.sidebarsCommunitiesFilterLowPriority(),
            },
        },
      .timeline =
        {
          .messages =
            {
              .style = cfg::toStorageValue(settings.timelineMessagesStyle()).toStdString(),
              .positioning =
                cfg::toStorageValue(settings.timelineMessagesPositioning()).toStdString(),
              .user_color_coding_policy =
                cfg::toStorageValue(settings.timelineUserColorCodingPolicy()).toStdString(),
              .has_layout_small_avatars   = true,
              .layout_small_avatars       = settings.timelineMessagesLayoutSmallAvatars(),
              .has_layout_show_own_avatar = true,
              .layout_show_own_avatar     = settings.timelineMessagesLayoutShowOwnAvatar(),
              .sender_username =
                cfg::toStorageValue(settings.timelineMessagesSenderUsername()).toStdString(),
              .has_emoji_only_enlarge = true,
              .emoji_only_enlarge     = settings.timelineMessagesEmojiOnlyEnlarge(),
              .has_hover_highlight    = true,
              .hover_highlight        = settings.timelineMessagesHoverHighlight(),
            },
          .formatted =
            {
              .has_code_syntax_highlighting = true,
              .code_syntax_highlighting     = settings.timelineFormattedCodeSyntaxHighlighting(),
            },
          .typing =
            {
              .has_show_enabled = true,
              .show_enabled     = settings.timelineTypingShowEnabled(),
            },
          .read_receipts =
            {
              .has_enabled = true,
              .enabled     = settings.timelineReadReceiptsEnabled(),
            },
          .message_actions =
            {
              .activation_policy =
                cfg::toStorageValue(settings.timelineMessageActionsActivationPolicy())
                  .toStdString(),
              .pinned_reactions = settings.timelineMessageActionsPinnedReactions().toStdString(),
            },
          .media =
            {
              .has_effects_enabled  = true,
              .effects_enabled      = settings.timelineMediaEffectsEnabled(),
              .has_animate_on_hover = true,
              .animate_on_hover     = settings.timelineMediaAnimateOnHover(),
              .image_display =
                cfg::toStorageValue(settings.timelineMediaImageDisplay()).toStdString(),
              .has_open_images_external         = true,
              .open_images_external             = settings.timelineMediaOpenImagesExternal(),
              .has_open_videos_external         = true,
              .open_videos_external             = settings.timelineMediaOpenVideosExternal(),
              .has_autoplay_gif_videos          = true,
              .autoplay_gif_videos              = settings.timelineMediaAutoplayGifVideos(),
              .has_open_audio_external          = true,
              .open_audio_external              = settings.timelineMediaOpenAudioExternal(),
              .has_default_audio_playback_speed = true,
              .default_audio_playback_speed     = settings.timelineMediaDefaultAudioPlaybackSpeed(),
            },
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
      .encryption =
        {
          .key_sharing =
            {
              .has_only_verified_users = true,
              .only_verified_users     = settings.encryptionKeySharingOnlyVerifiedUsers(),
              .has_share_with_trusted  = true,
              .share_with_trusted      = settings.encryptionKeySharingShareWithTrusted(),
            },
          .backup =
            {
              .online =
                {
                  .has_enabled = true,
                  .enabled     = settings.encryptionBackupOnlineEnabled(),
                },
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
      .network =
        {
          .presence_status_policy =
            cfg::toStorageValue(settings.networkPresenceStatusPolicy()).toStdString(),
          .has_tls_enable_certificate_validation = true,
          .tls_enable_certificate_validation     = settings.networkTlsEnableCertificateValidation(),
          .has_mrs_enabled                       = true,
          .mrs_enabled                           = settings.networkMrsEnabled(),
          .mrs_server_name                       = settings.networkMrsServerName().toStdString(),
          .has_http3_enabled                     = true,
          .http3_enabled                         = settings.networkHttp3Enabled(),
        },
      .integrations =
        {
          .has_system_tray_enabled   = true,
          .system_tray_enabled       = settings.integrationsSystemTrayEnabled(),
          .has_system_tray_autostart = true,
          .system_tray_autostart     = settings.integrationsSystemTrayAutostart(),
          .dbus_api_access =
            cfg::dbusAccessToStorage(settings.integrationsDbusApiAccess()).toStdString(),
          .browser_command = settings.integrationsBrowserCommand().toStdString(),
        },
      .composer =
        {
          .has_input_markdown_to_html_enabled = true,
          .input_markdown_to_html_enabled     = settings.composerInputMarkdownToHtmlEnabled(),
          .input_send_key = cfg::toStorageValue(settings.composerInputSendKey()).toStdString(),
          .input_auto_replace_emoji =
            cfg::toStorageValue(settings.composerInputAutoReplaceEmoji()).toStdString(),
          .input_emoji_preferred_gender =
            cfg::toStorageValue(settings.composerInputEmojiPreferredGender()).toStdString(),
          .input_emoji_preferred_skin_tone =
            cfg::toStorageValue(settings.composerInputEmojiPreferredSkinTone()).toStdString(),
          .has_input_inline_emoji_picker_enabled = true,
          .input_inline_emoji_picker_enabled     = settings.composerInputInlineEmojiPickerEnabled(),
          .has_input_inline_room_picker_enabled  = true,
          .input_inline_room_picker_enabled      = settings.composerInputInlineRoomPickerEnabled(),
          .has_input_inline_user_picker_enabled  = true,
          .input_inline_user_picker_enabled      = settings.composerInputInlineUserPickerEnabled(),
          .has_typing_send_enabled               = true,
          .typing_send_enabled                   = settings.composerTypingSendEnabled(),
          .has_extras_stickers_enabled           = true,
          .extras_stickers_enabled               = settings.composerExtrasStickersEnabled(),
        },
      .values = {},
    };

    appendCoreStoreConfigValues(settings, snapshot.values);

    for (const auto &adapter : config::enumTokenAdapters()) {
        if (adapter.id == settings::core::SettingId::UiScrollbarPolicy ||
            adapter.id == settings::core::SettingId::UiAvatarsDefaultAvatarStyle ||
            adapter.id == settings::core::SettingId::NotificationsMessageContentPolicy ||
            adapter.id == settings::core::SettingId::NetworkPresenceStatusPolicy ||
            adapter.id == settings::core::SettingId::IntegrationsDbusApiAccess ||
            adapter.id == settings::core::SettingId::SidebarsRoomListLastMessagePreview ||
            adapter.id == settings::core::SettingId::SidebarsRoomListSort ||
            adapter.id == settings::core::SettingId::SidebarsRoomListUnreadDetectionPolicy ||
            adapter.id == settings::core::SettingId::TimelineMessagesStyle ||
            adapter.id == settings::core::SettingId::TimelineMessagesPositioning ||
            adapter.id == settings::core::SettingId::TimelineUserColorCodingPolicy ||
            adapter.id == settings::core::SettingId::TimelineMessagesSenderUsername ||
            adapter.id == settings::core::SettingId::TimelineMessageActionsActivationPolicy ||
            adapter.id == settings::core::SettingId::TimelineMediaImageDisplay ||
            adapter.id == settings::core::SettingId::ComposerInputSendKey ||
            adapter.id == settings::core::SettingId::ComposerInputAutoReplaceEmoji ||
            adapter.id == settings::core::SettingId::ComposerInputEmojiPreferredGender ||
            adapter.id == settings::core::SettingId::ComposerInputEmojiPreferredSkinTone)
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
