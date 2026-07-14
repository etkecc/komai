// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include "komai-rust-cxxbridge/ffi.h"

#include <QString>

#include "SettingsSerializerConfigConverters.h"
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
          .scale_factor              = static_cast<float>(settings.uiScaleFactor()),
          .theme_slug                = settings.uiThemeSlug().toStdString(),
          .theme_mode                = cfg::toStorageValue(settings.uiThemeMode()).toStdString(),
          .font_size_pt              = settings.uiFontSizePt(),
          .font_family               = settings.uiFontFamily().toStdString(),
          .font_emoji_family         = settings.uiFontEmojiFamilyStorageValue().toStdString(),
          .motion_animations_enabled = settings.uiMotionAnimationsEnabled(),
          .layout_density       = cfg::toStorageValue(settings.uiLayoutDensity()).toStdString(),
          .avatars_circular     = settings.uiAvatarsCircular(),
          .scrollbar_policy     = {},
          .default_avatar_style = {},
          .language             = settings.uiLanguage().toStdString(),
        },
      .navigation =
        {
          .room_list =
            {
              .show_last_message_time = settings.navigationRoomListShowLastMessageTime(),
              .last_message_preview =
                cfg::toStorageValue(settings.navigationRoomListLastMessagePreview()).toStdString(),
              .show_unread_indicators = settings.navigationRoomListShowUnreadIndicators(),
              .sort = cfg::toStorageValue(settings.navigationRoomListSort()).toStdString(),
              .opening_policy =
                cfg::toStorageValue(settings.navigationRoomListOpeningPolicy()).toStdString(),
            },
          .communities =
            {
              .show_unread_indicators = settings.navigationCommunitiesShowUnreadIndicators(),
              .filter_favourites      = settings.navigationCommunitiesFilterFavourites(),
              .filter_people          = settings.navigationCommunitiesFilterPeople(),
              .filter_bots            = settings.navigationCommunitiesFilterBots(),
              .filter_groups          = settings.navigationCommunitiesFilterGroups(),
              .filter_server_notices  = settings.navigationCommunitiesFilterServerNotices(),
              .filter_low_priority    = settings.navigationCommunitiesFilterLowPriority(),
            },
          .tabs =
            {
              .auto_hide_with_single_tab = settings.navigationTabsAutoHideSingle(),
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
              .layout_adaptive_positioning_breakpoint_px =
                settings.timelineMessagesLayoutAdaptivePositioningBreakpointPx(),
              .sender_username =
                cfg::toStorageValue(settings.timelineMessagesSenderUsername()).toStdString(),
              .emoji_only_enlarge = settings.timelineMessagesEmojiOnlyEnlarge(),
              .hover_highlight    = settings.timelineMessagesHoverHighlight(),
              .drag_select        = settings.timelineMessagesDragSelect(),
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
              .global = settings.timelineReadReceiptsEnabled(),
              .by_room =
                [&settings]() {
                    rust::Vec<komai::rust::SettingsBoolMapEntry> entries;
                    const auto byRoom = settings.timelineReadReceiptsEnabledByRoom();
                    for (auto it = byRoom.begin(); it != byRoom.end(); ++it)
                        entries.push_back({.key = it.key().toStdString(), .value = it.value()});
                    return entries;
                }(),
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
              .global  = {},
              .by_room = {},
            },
          .threads =
            {
              .collapse_replies_global = settings.timelineThreadsCollapseReplies(),
              .collapse_replies_by_room =
                [&settings]() {
                    rust::Vec<komai::rust::SettingsBoolMapEntry> entries;
                    const auto byRoom = settings.timelineThreadsCollapseRepliesByRoom();
                    for (auto it = byRoom.begin(); it != byRoom.end(); ++it)
                        entries.push_back({.key = it.key().toStdString(), .value = it.value()});
                    return entries;
                }(),
            },
          .date_dividers =
            {
              .enabled = settings.timelineDateDividersEnabled(),
            },
          .room_header =
            {
              .button_labels =
                cfg::toStorageValue(settings.timelineRoomHeaderButtonLabels()).toStdString(),
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
              .icon_style =
                cfg::toStorageValue(settings.desktopSystemTrayIconStyle()).toStdString(),
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
          .element =
            {
              .enabled = settings.callsElementEnabled(),
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
          .browser_command        = settings.integrationsBrowserCommand().toStdString(),
          .transcription_provider = settings.integrationsTranscriptionProvider().toStdString(),
          .transcription_api_url  = settings.integrationsTranscriptionApiUrl().toStdString(),
          .transcription_model    = settings.integrationsTranscriptionModel().toStdString(),
          .transcription_language = settings.integrationsTranscriptionLanguage().toStdString(),
          .transcription_prompt   = settings.integrationsTranscriptionPrompt().toStdString(),
          .transcription_by_room  = {},
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
          .input_selection_formatting_toolbar_enabled =
            settings.composerInputSelectionFormattingToolbarEnabled(),
          .input_transcription_enabled      = settings.composerInputTranscriptionEnabled(),
          .input_spellcheck_enabled         = settings.composerInputSpellcheckEnabled(),
          .input_spellcheck_languages       = {},
          .attachments_strip_image_metadata = settings.composerAttachmentsStripImageMetadata(),
          .typing_send_global               = settings.composerTypingSendEnabled(),
          .typing_send_by_room =
            [&settings]() {
                rust::Vec<komai::rust::SettingsBoolMapEntry> entries;
                const auto byRoom = settings.composerTypingSendEnabledByRoom();
                for (auto it = byRoom.begin(); it != byRoom.end(); ++it)
                    entries.push_back({.key = it.key().toStdString(), .value = it.value()});
                return entries;
            }(),
        },
    };

    snapshot.ui.scrollbar_policy = cfg::toStorageValue(settings.uiScrollbarPolicy()).toStdString();
    snapshot.ui.default_avatar_style =
      cfg::toStorageValue(settings.uiAvatarsDefaultAvatarStyle()).toStdString();

    for (const auto &value : settings.hiddenTimelineEventTypes())
        snapshot.timeline.hidden_events.global.push_back(value.toStdString());
    for (const auto &value : settings.composerInputSpellcheckLanguages())
        snapshot.composer.input_spellcheck_languages.push_back(value.toStdString());
    for (auto it = settings.hiddenTimelineEventTypesByRoom().constBegin();
         it != settings.hiddenTimelineEventTypesByRoom().constEnd();
         ++it) {
        ::rust::Vec<::rust::String> rustValues;
        for (const auto &value : it.value())
            rustValues.push_back(value.toStdString());
        snapshot.timeline.hidden_events.by_room.push_back(
          {.key = it.key().toStdString(), .values = std::move(rustValues)});
    }

    {
        const auto byRoom = settings.integrationsTranscriptionOverridesByRoom();
        for (auto roomIt = byRoom.constBegin(); roomIt != byRoom.constEnd(); ++roomIt) {
            const auto &fields = roomIt.value();
            ::komai::rust::SettingsConfigTranscriptionByRoomEntry entry{};
            entry.key = roomIt.key().toStdString();
            const auto setIfPresent =
              [&fields](const QString &name, bool &flag, ::rust::String &value) {
                  const auto it = fields.find(name);
                  if (it == fields.end())
                      return;
                  flag  = true;
                  value = it.value().toStdString();
              };
            setIfPresent(QStringLiteral("provider"), entry.has_provider, entry.provider);
            setIfPresent(QStringLiteral("api_url"), entry.has_api_url, entry.api_url);
            setIfPresent(QStringLiteral("model"), entry.has_model, entry.model);
            setIfPresent(QStringLiteral("language"), entry.has_language, entry.language);
            setIfPresent(QStringLiteral("prompt"), entry.has_prompt, entry.prompt);
            // Drop rooms that ended up with no overrides — they would
            // otherwise emit an empty mapping in YAML.
            if (!entry.has_provider && !entry.has_api_url && !entry.has_model &&
                !entry.has_language && !entry.has_prompt) {
                continue;
            }
            snapshot.integrations.transcription_by_room.push_back(std::move(entry));
        }
    }

    ::komai::rust::settings_profile_replace_config_snapshot(profileHandle, snapshot);
}

} // namespace settings::serializer
