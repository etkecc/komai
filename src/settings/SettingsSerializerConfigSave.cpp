// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include "komai-rust-cxxbridge/ffi.h"

#include <QString>

#include "SettingsSerializerConfigConverters.h"
#include "SettingsSerializerConfigInternal.h"
#include "settings/core/StartupConfig.h"

namespace settings::serializer {

namespace cfg = settings::serializer::config;

void
stageConfig(const UserSettings &settings,
            bool usesFileSecretsProvider,
            ::komai::rust::SettingsProfileHandle &profileHandle)
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
          .provider =
            (usesFileSecretsProvider ? QStringLiteral("file") : QStringLiteral("secret_service"))
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
    };

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

    ::komai::rust::settings_profile_replace_config_snapshot(profileHandle, snapshot);
}

} // namespace settings::serializer
