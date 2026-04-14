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
          .has_layout_compact_mode                = true,
          .layout_compact_mode                    = settings.uiLayoutCompactMode(),
          .has_avatars_circular                   = true,
          .avatars_circular                       = settings.uiAvatarsCircular(),
          .scrollbar_policy                       = {},
          .default_avatar_style                   = {},
        },
      .navigation =
        {
          .room_list =
            {
              .has_show_last_message_time = true,
              .show_last_message_time     = settings.navigationRoomListShowLastMessageTime(),
              .last_message_preview =
                cfg::toStorageValue(settings.navigationRoomListLastMessagePreview()).toStdString(),
              .has_show_community_counts = true,
              .show_community_counts     = settings.navigationRoomListShowCommunityCounts(),
              .sort = cfg::toStorageValue(settings.navigationRoomListSort()).toStdString(),
              .unread_detection_policy =
                cfg::toStorageValue(settings.navigationRoomListUnreadDetectionPolicy())
                  .toStdString(),
              .opening_policy =
                cfg::toStorageValue(settings.navigationRoomListOpeningPolicy()).toStdString(),
            },
          .communities =
            {
              .has_visible               = true,
              .visible                   = settings.navigationCommunitiesVisible(),
              .has_filter_favourites     = true,
              .filter_favourites         = settings.navigationCommunitiesFilterFavourites(),
              .has_filter_people         = true,
              .filter_people             = settings.navigationCommunitiesFilterPeople(),
              .has_filter_bots           = true,
              .filter_bots               = settings.navigationCommunitiesFilterBots(),
              .has_filter_groups         = true,
              .filter_groups             = settings.navigationCommunitiesFilterGroups(),
              .has_filter_server_notices = true,
              .filter_server_notices     = settings.navigationCommunitiesFilterServerNotices(),
              .has_filter_low_priority   = true,
              .filter_low_priority       = settings.navigationCommunitiesFilterLowPriority(),
            },
          .tabs =
            {
              .show_pin_button =
                cfg::toStorageValue(settings.navigationTabsShowPinButton()).toStdString(),
              .pinned_tab_label =
                cfg::toStorageValue(settings.navigationTabsPinnedTabLabel()).toStdString(),
              .tab_label = cfg::toStorageValue(settings.navigationTabsTabLabel()).toStdString(),
              .has_preferred_width_px            = true,
              .preferred_width_px                = settings.navigationTabsPreferredWidthPx(),
              .has_minimum_width_px              = true,
              .minimum_width_px                  = settings.navigationTabsMinimumWidthPx(),
              .has_max_recently_closed_timelines = true,
              .max_recently_closed_timelines = settings.navigationTabsMaxRecentlyClosedTimelines(),
            },
        },
      .timeline =
        {
          .messages =
            {
              .style = cfg::toStorageValue(settings.timelineMessagesStyle()).toStdString(),
              .layout_positioning =
                cfg::toStorageValue(settings.timelineMessagesLayoutPositioning()).toStdString(),
              .user_color_coding_policy =
                cfg::toStorageValue(settings.timelineUserColorCodingPolicy()).toStdString(),
              .layout_avatar_size =
                cfg::toStorageValue(settings.timelineMessagesLayoutAvatarSize()).toStdString(),
              .has_layout_show_own_avatar   = true,
              .layout_show_own_avatar       = settings.timelineMessagesLayoutShowOwnAvatar(),
              .has_layout_max_width_percent = true,
              .layout_max_width_percent     = settings.timelineMessagesLayoutMaxWidthPercent(),
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
      .desktop =
        {
          .notifications =
            {
              .has_enabled               = true,
              .enabled                   = settings.desktopNotificationsEnabled(),
              .has_attention_on_incoming = true,
              .attention_on_incoming     = settings.desktopNotificationsAttentionOnIncoming(),
              .message_content_policy =
                cfg::toStorageValue(settings.desktopNotificationsMessageContentPolicy())
                  .toStdString(),
            },
          .attention =
            {
              .window_title =
                {
                  .has_enabled = true,
                  .enabled     = settings.desktopAttentionWindowTitleEnabled(),
                },
              .app_badge =
                {
                  .has_enabled = true,
                  .enabled     = settings.desktopAttentionAppBadgeEnabled(),
                },
            },
          .system_tray =
            {
              .has_enabled   = true,
              .enabled       = settings.desktopSystemTrayEnabled(),
              .has_autostart = true,
              .autostart     = settings.desktopSystemTrayAutostart(),
            },
          .window_focus_blur =
            {
              .has_enabled       = true,
              .enabled           = settings.desktopWindowFocusBlurEnabled(),
              .has_delay_seconds = true,
              .delay_seconds     = settings.desktopWindowFocusBlurDelaySeconds(),
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
      .network =
        {
          .encryption =
            {
              .has_only_verified_users = true,
              .only_verified_users     = settings.encryptionKeySharingOnlyVerifiedUsers(),
              .has_share_with_trusted  = true,
              .share_with_trusted      = settings.encryptionKeySharingShareWithTrusted(),
              .has_key_backup          = true,
              .key_backup              = settings.encryptionBackupOnlineEnabled(),
            },
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
