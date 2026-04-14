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
          .font_size_pt      = settings.uiFontSizePt(),
          .font_family       = settings.uiFontFamily().toStdString(),
          .font_emoji_family = settings.uiFontEmojiFamilyStorageValue().toStdString(),
          .motion_animations_enabled = settings.uiMotionAnimationsEnabled(),
          .input_mode = detail::toStorageUiInputMode(settings.uiInputMode()).toStdString(),
          .input_touch_swipe_gestures_enabled = settings.uiInputTouchSwipeGesturesEnabled(),
          .layout_compact_mode                = settings.uiLayoutCompactMode(),
          .avatars_circular                   = settings.uiAvatarsCircular(),
          .scrollbar_policy                   = {},
          .default_avatar_style               = {},
        },
      .navigation =
        {
          .room_list =
            {
              .show_last_message_time = settings.navigationRoomListShowLastMessageTime(),
              .last_message_preview =
                cfg::toStorageValue(settings.navigationRoomListLastMessagePreview()).toStdString(),
              .show_community_counts = settings.navigationRoomListShowCommunityCounts(),
              .sort = cfg::toStorageValue(settings.navigationRoomListSort()).toStdString(),
              .unread_detection_policy =
                cfg::toStorageValue(settings.navigationRoomListUnreadDetectionPolicy())
                  .toStdString(),
              .opening_policy =
                cfg::toStorageValue(settings.navigationRoomListOpeningPolicy()).toStdString(),
            },
          .communities =
            {
              .visible               = settings.navigationCommunitiesVisible(),
              .filter_favourites     = settings.navigationCommunitiesFilterFavourites(),
              .filter_people         = settings.navigationCommunitiesFilterPeople(),
              .filter_bots           = settings.navigationCommunitiesFilterBots(),
              .filter_groups         = settings.navigationCommunitiesFilterGroups(),
              .filter_server_notices = settings.navigationCommunitiesFilterServerNotices(),
              .filter_low_priority   = settings.navigationCommunitiesFilterLowPriority(),
            },
          .tabs =
            {
              .show_pin_button =
                cfg::toStorageValue(settings.navigationTabsShowPinButton()).toStdString(),
              .pinned_tab_label =
                cfg::toStorageValue(settings.navigationTabsPinnedTabLabel()).toStdString(),
              .tab_label = cfg::toStorageValue(settings.navigationTabsTabLabel()).toStdString(),
              .preferred_width_px            = settings.navigationTabsPreferredWidthPx(),
              .minimum_width_px              = settings.navigationTabsMinimumWidthPx(),
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
              .layout_show_own_avatar   = settings.timelineMessagesLayoutShowOwnAvatar(),
              .layout_max_width_percent = settings.timelineMessagesLayoutMaxWidthPercent(),
              .sender_username =
                cfg::toStorageValue(settings.timelineMessagesSenderUsername()).toStdString(),
              .emoji_only_enlarge = settings.timelineMessagesEmojiOnlyEnlarge(),
              .hover_highlight    = settings.timelineMessagesHoverHighlight(),
            },
          .formatted =
            {
              .code_syntax_highlighting = settings.timelineFormattedCodeSyntaxHighlighting(),
            },
          .typing =
            {
              .show_enabled = settings.timelineTypingShowEnabled(),
            },
          .read_receipts =
            {
              .enabled = settings.timelineReadReceiptsEnabled(),
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
              .effects_enabled  = settings.timelineMediaEffectsEnabled(),
              .animate_on_hover = settings.timelineMediaAnimateOnHover(),
              .image_display =
                cfg::toStorageValue(settings.timelineMediaImageDisplay()).toStdString(),
              .open_images_external         = settings.timelineMediaOpenImagesExternal(),
              .open_videos_external         = settings.timelineMediaOpenVideosExternal(),
              .autoplay_gif_videos          = settings.timelineMediaAutoplayGifVideos(),
              .open_audio_external          = settings.timelineMediaOpenAudioExternal(),
              .default_audio_playback_speed = settings.timelineMediaDefaultAudioPlaybackSpeed(),
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
              .enabled               = settings.desktopNotificationsEnabled(),
              .attention_on_incoming = settings.desktopNotificationsAttentionOnIncoming(),
              .message_content_policy =
                cfg::toStorageValue(settings.desktopNotificationsMessageContentPolicy())
                  .toStdString(),
            },
          .attention =
            {
              .window_title =
                {
                  .enabled = settings.desktopAttentionWindowTitleEnabled(),
                },
              .app_badge =
                {
                  .enabled = settings.desktopAttentionAppBadgeEnabled(),
                },
            },
          .system_tray =
            {
              .enabled   = settings.desktopSystemTrayEnabled(),
              .autostart = settings.desktopSystemTrayAutostart(),
            },
          .window_focus_blur =
            {
              .enabled       = settings.desktopWindowFocusBlurEnabled(),
              .delay_seconds = settings.desktopWindowFocusBlurDelaySeconds(),
            },
        },
      .calls =
        {
          .legacy =
            {
              .enabled = settings.callsLegacyEnabled(),
            },
          .relay =
            {
              .use_fallback_server = settings.callsRelayUseFallbackServer(),
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
              .frame_rate           = settings.callsScreenshareFrameRate(),
              .picture_in_picture   = settings.callsScreensharePictureInPicture(),
              .include_remote_video = settings.callsScreenshareIncludeRemoteVideo(),
              .show_cursor          = settings.callsScreenshareShowCursor(),
            },
        },
      .network =
        {
          .encryption =
            {
              .only_verified_users = settings.encryptionKeySharingOnlyVerifiedUsers(),
              .share_with_trusted  = settings.encryptionKeySharingShareWithTrusted(),
              .key_backup          = settings.encryptionBackupOnlineEnabled(),
            },
          .presence_status_policy =
            cfg::toStorageValue(settings.networkPresenceStatusPolicy()).toStdString(),
          .tls_enable_certificate_validation = settings.networkTlsEnableCertificateValidation(),
          .mrs_enabled                       = settings.networkMrsEnabled(),
          .mrs_server_name                   = settings.networkMrsServerName().toStdString(),
          .http3_enabled                     = settings.networkHttp3Enabled(),
        },
      .integrations =
        {
          .dbus_api_access =
            cfg::dbusAccessToStorage(settings.integrationsDbusApiAccess()).toStdString(),
          .browser_command = settings.integrationsBrowserCommand().toStdString(),
        },
      .composer =
        {
          .input_markdown_to_html_enabled = settings.composerInputMarkdownToHtmlEnabled(),
          .input_send_key = cfg::toStorageValue(settings.composerInputSendKey()).toStdString(),
          .input_auto_replace_emoji =
            cfg::toStorageValue(settings.composerInputAutoReplaceEmoji()).toStdString(),
          .input_emoji_preferred_gender =
            cfg::toStorageValue(settings.composerInputEmojiPreferredGender()).toStdString(),
          .input_emoji_preferred_skin_tone =
            cfg::toStorageValue(settings.composerInputEmojiPreferredSkinTone()).toStdString(),
          .input_inline_emoji_picker_enabled = settings.composerInputInlineEmojiPickerEnabled(),
          .input_inline_room_picker_enabled  = settings.composerInputInlineRoomPickerEnabled(),
          .input_inline_user_picker_enabled  = settings.composerInputInlineUserPickerEnabled(),
          .typing_send_enabled               = settings.composerTypingSendEnabled(),
          .extras_stickers_enabled           = settings.composerExtrasStickersEnabled(),
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
