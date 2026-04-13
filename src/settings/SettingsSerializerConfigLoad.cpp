// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerLoad.h"

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

namespace cfg = settings::serializer::config;
namespace settings::serializer {

void
loadConfig(UserSettings &settings, const ::komai::rust::SettingsLoadedConfig &snapshot)
{
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

    const auto loadedScrollbarPolicy =
      QString::fromStdString(static_cast<std::string>(snapshot.ui.scrollbar_policy)).trimmed();
    const auto scrollbarPolicyToken =
      loadedScrollbarPolicy.isEmpty()
        ? cfg::toStorageValue(UserSettings::ScrollbarPolicy::WhenNeeded)
        : loadedScrollbarPolicy;
    settings.setUiScrollbarPolicy(cfg::scrollbarPolicyFromStorage(
      scrollbarPolicyToken, UserSettings::ScrollbarPolicy::WhenNeeded));

    const auto loadedDefaultAvatarStyle =
      QString::fromStdString(static_cast<std::string>(snapshot.ui.default_avatar_style)).trimmed();
    const auto defaultAvatarStyleToken =
      loadedDefaultAvatarStyle.isEmpty()
        ? cfg::toStorageValue(UserSettings::DefaultAvatarStyle::BoringAvatarsBauhaus)
        : loadedDefaultAvatarStyle;
    settings.setUiAvatarsDefaultAvatarStyle(cfg::defaultAvatarStyleFromStorage(
      defaultAvatarStyleToken, UserSettings::DefaultAvatarStyle::BoringAvatarsBauhaus));

    const auto loadedInputModeToken =
      QString::fromStdString(static_cast<std::string>(snapshot.ui.input_mode)).trimmed();
    const auto inputModeToken =
      loadedInputModeToken.isEmpty()
        ? detail::toStorageUiInputMode(settings::core::definitions::kDefaultUiInputMode)
        : loadedInputModeToken;
    settings.setUiInputMode(detail::fromStorageUiInputMode(inputModeToken));

    settings.setUiScaleFactor(snapshot.ui.has_scale_factor
                                ? snapshot.ui.scale_factor
                                : settings::core::definitions::kDefaultScaleFactor);
    settings.setUiInputTouchSwipeGesturesEnabled(snapshot.ui.has_input_touch_swipe_gestures_enabled
                                                   ? snapshot.ui.input_touch_swipe_gestures_enabled
                                                   : false);
    settings.setUiAvatarsCircular(snapshot.ui.has_avatars_circular ? snapshot.ui.avatars_circular
                                                                   : false);
    settings.setUiLayoutCompactMode(
      snapshot.ui.has_layout_compact_mode ? snapshot.ui.layout_compact_mode : false);

    settings.setNavigationRoomListShowLastMessageTime(
      snapshot.navigation.room_list.has_show_last_message_time
        ? snapshot.navigation.room_list.show_last_message_time
        : true);

    const auto loadedNavigationRoomListLastMessagePreview =
      QString::fromStdString(
        static_cast<std::string>(snapshot.navigation.room_list.last_message_preview))
        .trimmed();
    const auto navigationRoomListLastMessagePreviewToken =
      loadedNavigationRoomListLastMessagePreview.isEmpty()
        ? QStringLiteral("always")
        : loadedNavigationRoomListLastMessagePreview;
    settings.setNavigationRoomListLastMessagePreview(cfg::lastMessagePreviewFromStorage(
      navigationRoomListLastMessagePreviewToken, UserSettings::LastMessagePreview::Always));

    settings.setNavigationRoomListShowCommunityCounts(
      snapshot.navigation.room_list.has_show_community_counts
        ? snapshot.navigation.room_list.show_community_counts
        : true);

    const auto loadedNavigationRoomListSort =
      QString::fromStdString(static_cast<std::string>(snapshot.navigation.room_list.sort))
        .trimmed();
    const auto navigationRoomListSortToken = loadedNavigationRoomListSort.isEmpty()
                                               ? QStringLiteral("unread_first_recent")
                                               : loadedNavigationRoomListSort;
    settings.setNavigationRoomListSort(cfg::roomSortOrderFromStorage(
      navigationRoomListSortToken, UserSettings::RoomSortOrder::UnreadFirst_Recent));

    const auto loadedNavigationRoomListUnreadDetectionPolicy =
      QString::fromStdString(
        static_cast<std::string>(snapshot.navigation.room_list.unread_detection_policy))
        .trimmed();
    const auto navigationRoomListUnreadDetectionPolicyToken =
      loadedNavigationRoomListUnreadDetectionPolicy.isEmpty()
        ? QStringLiteral("any_event")
        : loadedNavigationRoomListUnreadDetectionPolicy;
    settings.setNavigationRoomListUnreadDetectionPolicy(cfg::unreadDetectionPolicyFromStorage(
      navigationRoomListUnreadDetectionPolicyToken, UserSettings::UnreadDetectionPolicy::AnyEvent));

    settings.setNavigationCommunitiesVisible(
      snapshot.navigation.communities.has_visible ? snapshot.navigation.communities.visible : true);
    settings.setNavigationCommunitiesFilterFavourites(
      snapshot.navigation.communities.has_filter_favourites
        ? snapshot.navigation.communities.filter_favourites
        : true);
    settings.setNavigationCommunitiesFilterPeople(snapshot.navigation.communities.has_filter_people
                                                    ? snapshot.navigation.communities.filter_people
                                                    : true);
    settings.setNavigationCommunitiesFilterBots(snapshot.navigation.communities.has_filter_bots
                                                  ? snapshot.navigation.communities.filter_bots
                                                  : true);
    settings.setNavigationCommunitiesFilterGroups(snapshot.navigation.communities.has_filter_groups
                                                    ? snapshot.navigation.communities.filter_groups
                                                    : true);
    settings.setNavigationCommunitiesFilterServerNotices(
      snapshot.navigation.communities.has_filter_server_notices
        ? snapshot.navigation.communities.filter_server_notices
        : true);
    settings.setNavigationCommunitiesFilterLowPriority(
      snapshot.navigation.communities.has_filter_low_priority
        ? snapshot.navigation.communities.filter_low_priority
        : true);

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

    const auto loadedTimelineMessagesStyle =
      QString::fromStdString(static_cast<std::string>(snapshot.timeline.messages.style)).trimmed();
    const auto timelineMessagesStyleToken = loadedTimelineMessagesStyle.isEmpty()
                                              ? QStringLiteral("bubbles")
                                              : loadedTimelineMessagesStyle;
    settings.setTimelineMessagesStyle(cfg::timelineMessagesStyleFromStorage(
      timelineMessagesStyleToken, UserSettings::TimelineMessagesStyle::Bubbles));

    const auto loadedTimelineMessagesLayoutPositioning =
      QString::fromStdString(
        static_cast<std::string>(snapshot.timeline.messages.layout_positioning))
        .trimmed();
    const auto timelineMessagesLayoutPositioningToken =
      loadedTimelineMessagesLayoutPositioning.isEmpty() ? QStringLiteral("opposing_by_sender")
                                                        : loadedTimelineMessagesLayoutPositioning;
    settings.setTimelineMessagesLayoutPositioning(cfg::timelineMessagesLayoutPositioningFromStorage(
      timelineMessagesLayoutPositioningToken,
      UserSettings::TimelineMessagesLayoutPositioning::OpposingBySender));

    const auto loadedTimelineUserColorCodingPolicy =
      QString::fromStdString(
        static_cast<std::string>(snapshot.timeline.messages.user_color_coding_policy))
        .trimmed();
    const auto timelineUserColorCodingPolicyToken = loadedTimelineUserColorCodingPolicy.isEmpty()
                                                      ? QStringLiteral("adaptive_by_room_size")
                                                      : loadedTimelineUserColorCodingPolicy;
    settings.setTimelineUserColorCodingPolicy(cfg::timelineUserColorCodingPolicyFromStorage(
      timelineUserColorCodingPolicyToken,
      UserSettings::TimelineUserColorCodingPolicy::AdaptiveByRoomSize));

    const auto loadedTimelineAvatarSize =
      QString::fromStdString(
        static_cast<std::string>(snapshot.timeline.messages.layout_avatar_size))
        .trimmed();
    const auto timelineAvatarSizeToken =
      loadedTimelineAvatarSize.isEmpty() ? QStringLiteral("regular") : loadedTimelineAvatarSize;
    settings.setTimelineMessagesLayoutAvatarSize(
      cfg::avatarSizeFromStorage(timelineAvatarSizeToken, UserSettings::AvatarSize::Regular));
    settings.setTimelineMessagesLayoutShowOwnAvatar(
      snapshot.timeline.messages.has_layout_show_own_avatar
        ? snapshot.timeline.messages.layout_show_own_avatar
        : true);
    settings.setTimelineMessagesLayoutMaxWidthPercent(
      snapshot.timeline.messages.has_layout_max_width_percent
        ? snapshot.timeline.messages.layout_max_width_percent
        : settings::core::definitions::kDefaultTimelineMessagesLayoutMaxWidthPercent);

    const auto loadedTimelineMessagesSenderUsername =
      QString::fromStdString(static_cast<std::string>(snapshot.timeline.messages.sender_username))
        .trimmed();
    const auto timelineMessagesSenderUsernameToken = loadedTimelineMessagesSenderUsername.isEmpty()
                                                       ? QStringLiteral("only_in_large_rooms")
                                                       : loadedTimelineMessagesSenderUsername;
    settings.setTimelineMessagesSenderUsername(cfg::showSenderUsernameFromStorage(
      timelineMessagesSenderUsernameToken, UserSettings::ShowSenderUsername::OnlyInLargeRooms));

    settings.setTimelineMessagesEmojiOnlyEnlarge(snapshot.timeline.messages.has_emoji_only_enlarge
                                                   ? snapshot.timeline.messages.emoji_only_enlarge
                                                   : true);
    settings.setTimelineMessagesHoverHighlight(snapshot.timeline.messages.has_hover_highlight
                                                 ? snapshot.timeline.messages.hover_highlight
                                                 : false);
    settings.setTimelineFormattedCodeSyntaxHighlighting(
      snapshot.timeline.formatted.has_code_syntax_highlighting
        ? snapshot.timeline.formatted.code_syntax_highlighting
        : true);
    settings.setTimelineTypingShowEnabled(
      snapshot.timeline.typing.has_show_enabled ? snapshot.timeline.typing.show_enabled : true);
    settings.setTimelineReadReceiptsEnabled(
      snapshot.timeline.read_receipts.has_enabled ? snapshot.timeline.read_receipts.enabled : true);

    const auto loadedTimelineMessageActionsActivationPolicy =
      QString::fromStdString(
        static_cast<std::string>(snapshot.timeline.message_actions.activation_policy))
        .trimmed();
    const auto timelineMessageActionsActivationPolicyToken =
      loadedTimelineMessageActionsActivationPolicy.isEmpty()
        ? QStringLiteral("on_button_click")
        : loadedTimelineMessageActionsActivationPolicy;
    settings.setTimelineMessageActionsActivationPolicy(
      cfg::timelineMessageActionsActivationPolicyFromStorage(
        timelineMessageActionsActivationPolicyToken,
        UserSettings::TimelineMessageActionsActivationPolicy::ActionsButton));

    const auto loadedTimelineMessageActionsPinnedReactions =
      QString::fromStdString(
        static_cast<std::string>(snapshot.timeline.message_actions.pinned_reactions))
        .trimmed();
    settings.setTimelineMessageActionsPinnedReactions(
      loadedTimelineMessageActionsPinnedReactions.isEmpty()
        ? QString::fromUtf8(settings::core::definitions::kDefaultPinnedReactions)
        : loadedTimelineMessageActionsPinnedReactions);

    settings.setTimelineMediaEffectsEnabled(
      snapshot.timeline.media.has_effects_enabled ? snapshot.timeline.media.effects_enabled : true);
    settings.setTimelineMediaAnimateOnHover(snapshot.timeline.media.has_animate_on_hover
                                              ? snapshot.timeline.media.animate_on_hover
                                              : false);

    const auto loadedTimelineMediaImageDisplay =
      QString::fromStdString(static_cast<std::string>(snapshot.timeline.media.image_display))
        .trimmed();
    const auto timelineMediaImageDisplayToken = loadedTimelineMediaImageDisplay.isEmpty()
                                                  ? QStringLiteral("always")
                                                  : loadedTimelineMediaImageDisplay;
    settings.setTimelineMediaImageDisplay(
      cfg::showImageFromStorage(timelineMediaImageDisplayToken, UserSettings::ShowImage::Always));

    settings.setTimelineMediaOpenImagesExternal(snapshot.timeline.media.has_open_images_external
                                                  ? snapshot.timeline.media.open_images_external
                                                  : false);
    settings.setTimelineMediaOpenVideosExternal(snapshot.timeline.media.has_open_videos_external
                                                  ? snapshot.timeline.media.open_videos_external
                                                  : false);
    settings.setTimelineMediaAutoplayGifVideos(snapshot.timeline.media.has_autoplay_gif_videos
                                                 ? snapshot.timeline.media.autoplay_gif_videos
                                                 : true);
    settings.setTimelineMediaOpenAudioExternal(snapshot.timeline.media.has_open_audio_external
                                                 ? snapshot.timeline.media.open_audio_external
                                                 : false);
    settings.setTimelineMediaDefaultAudioPlaybackSpeed(
      snapshot.timeline.media.has_default_audio_playback_speed
        ? snapshot.timeline.media.default_audio_playback_speed
        : settings::core::definitions::kDefaultTimelineMediaAudioPlaybackSpeed);

    settings.setDesktopWindowFocusBlurEnabled(snapshot.desktop.window_focus_blur.has_enabled
                                                ? snapshot.desktop.window_focus_blur.enabled
                                                : false);
    settings.setDesktopWindowFocusBlurDelaySeconds(
      snapshot.desktop.window_focus_blur.has_delay_seconds
        ? snapshot.desktop.window_focus_blur.delay_seconds
        : settings::core::definitions::kDefaultDesktopWindowFocusBlurDelaySeconds);
    settings.setEncryptionKeySharingOnlyVerifiedUsers(
      snapshot.network.encryption.has_only_verified_users
        ? snapshot.network.encryption.only_verified_users
        : false);
    settings.setEncryptionKeySharingShareWithTrusted(
      snapshot.network.encryption.has_share_with_trusted
        ? snapshot.network.encryption.share_with_trusted
        : false);
    settings.setEncryptionBackupOnlineEnabledFromConfig(
      snapshot.network.encryption.has_key_backup ? snapshot.network.encryption.key_backup : true);

    settings.setCallsLegacyEnabled(snapshot.calls.legacy.has_enabled ? snapshot.calls.legacy.enabled
                                                                     : false);
    settings.setCallsRelayUseFallbackServer(snapshot.calls.relay.has_use_fallback_server
                                              ? snapshot.calls.relay.use_fallback_server
                                              : false);
    settings.setCallsDevicesMicrophone(
      QString::fromStdString(static_cast<std::string>(snapshot.calls.devices.microphone)));
    settings.setCallsDevicesCamera(
      QString::fromStdString(static_cast<std::string>(snapshot.calls.devices.camera)));
    settings.setCallsDevicesCameraResolution(
      QString::fromStdString(static_cast<std::string>(snapshot.calls.devices.camera_resolution)));
    settings.setCallsDevicesCameraFrameRate(
      QString::fromStdString(static_cast<std::string>(snapshot.calls.devices.camera_frame_rate)));

    const auto loadedCallsAudioRingtone =
      QString::fromStdString(static_cast<std::string>(snapshot.calls.audio.ringtone)).trimmed();
    settings.setCallsAudioRingtone(
      loadedCallsAudioRingtone.isEmpty()
        ? QString::fromLatin1(settings::core::definitions::kDefaultCallsAudioRingtone)
        : loadedCallsAudioRingtone);

    settings.setCallsScreenshareFrameRate(
      snapshot.calls.screenshare.has_frame_rate
        ? snapshot.calls.screenshare.frame_rate
        : settings::core::definitions::kDefaultScreenShareFrameRate);
    settings.setCallsScreensharePictureInPicture(snapshot.calls.screenshare.has_picture_in_picture
                                                   ? snapshot.calls.screenshare.picture_in_picture
                                                   : true);
    settings.setCallsScreenshareIncludeRemoteVideo(
      snapshot.calls.screenshare.has_include_remote_video
        ? snapshot.calls.screenshare.include_remote_video
        : false);
    settings.setCallsScreenshareShowCursor(
      snapshot.calls.screenshare.has_show_cursor
        ? snapshot.calls.screenshare.show_cursor
        : settings::core::definitions::kDefaultScreenShareShowCursor);

    settings.setDesktopNotificationsEnabled(
      snapshot.desktop.notifications.has_enabled ? snapshot.desktop.notifications.enabled : true);
    settings.setDesktopNotificationsAttentionOnIncoming(
      snapshot.desktop.notifications.has_attention_on_incoming
        ? snapshot.desktop.notifications.attention_on_incoming
        : false);

    const auto loadedDesktopNotificationsMessageContentPolicy =
      QString::fromStdString(
        static_cast<std::string>(snapshot.desktop.notifications.message_content_policy))
        .trimmed();
    const auto notificationsMessageContentPolicyToken =
      loadedDesktopNotificationsMessageContentPolicy.isEmpty()
        ? QStringLiteral("whenever_available")
        : loadedDesktopNotificationsMessageContentPolicy;
    settings.setDesktopNotificationsMessageContentPolicy(
      cfg::notificationsMessageContentPolicyFromStorage(
        notificationsMessageContentPolicyToken,
        UserSettings::NotificationMessageContentPolicy::WheneverAvailable));
    settings.setDesktopAttentionWindowTitleEnabled(
      snapshot.desktop.attention.window_title.has_enabled
        ? snapshot.desktop.attention.window_title.enabled
        : settings::core::definitions::kDefaultDesktopAttentionWindowTitleEnabled);
    settings.setDesktopAttentionAppBadgeEnabled(
      snapshot.desktop.attention.app_badge.has_enabled
        ? snapshot.desktop.attention.app_badge.enabled
        : settings::core::definitions::kDefaultDesktopAttentionAppBadgeEnabled);

    settings.setNetworkTlsEnableCertificateValidation(
      snapshot.network.has_tls_enable_certificate_validation
        ? snapshot.network.tls_enable_certificate_validation
        : settings::core::definitions::kDefaultCertificateValidationEnabled);
    settings.setNetworkMrsEnabled(snapshot.network.has_mrs_enabled
                                    ? snapshot.network.mrs_enabled
                                    : settings::core::definitions::kDefaultNetworkMrsEnabled);

    const auto loadedNetworkMrsServerName =
      QString::fromStdString(static_cast<std::string>(snapshot.network.mrs_server_name)).trimmed();
    settings.setNetworkMrsServerName(
      loadedNetworkMrsServerName.isEmpty()
        ? QString::fromLatin1(settings::core::definitions::kDefaultNetworkMrsServerName)
        : loadedNetworkMrsServerName);

    settings.setNetworkHttp3Enabled(snapshot.network.has_http3_enabled
                                      ? snapshot.network.http3_enabled
                                      : settings::core::definitions::kDefaultNetworkHttp3Enabled);

    const auto loadedNetworkPresenceStatusPolicy =
      QString::fromStdString(static_cast<std::string>(snapshot.network.presence_status_policy))
        .trimmed();
    const auto networkPresenceStatusPolicyToken = loadedNetworkPresenceStatusPolicy.isEmpty()
                                                    ? QStringLiteral("automatic_presence")
                                                    : loadedNetworkPresenceStatusPolicy;
    settings.setNetworkPresenceStatusPolicy(cfg::presenceFromStorage(
      networkPresenceStatusPolicyToken, UserSettings::Presence::AutomaticPresence));

    settings.setDesktopSystemTrayEnabled(
      snapshot.desktop.system_tray.has_enabled ? snapshot.desktop.system_tray.enabled : false);
    settings.setDesktopSystemTrayAutostart(
      snapshot.desktop.system_tray.has_autostart ? snapshot.desktop.system_tray.autostart : false);
    settings.setIntegrationsBrowserCommand(
      QString::fromStdString(static_cast<std::string>(snapshot.integrations.browser_command)));

    const auto loadedIntegrationsDbusApiAccess =
      QString::fromStdString(static_cast<std::string>(snapshot.integrations.dbus_api_access))
        .trimmed();
    const auto integrationsDbusApiAccessToken = loadedIntegrationsDbusApiAccess.isEmpty()
                                                  ? QStringLiteral("none")
                                                  : loadedIntegrationsDbusApiAccess;
    settings.setIntegrationsDbusApiAccess(
      cfg::dbusAccessFromStorage(integrationsDbusApiAccessToken, IntegrationsDbusAccessNone));

    settings.setComposerInputMarkdownToHtmlEnabled(
      snapshot.composer.has_input_markdown_to_html_enabled
        ? snapshot.composer.input_markdown_to_html_enabled
        : true);

    const auto loadedComposerInputSendKey =
      QString::fromStdString(static_cast<std::string>(snapshot.composer.input_send_key)).trimmed();
    const auto composerInputSendKeyToken =
      loadedComposerInputSendKey.isEmpty() ? QStringLiteral("enter") : loadedComposerInputSendKey;
    settings.setComposerInputSendKey(cfg::sendMessageKeyFromStorage(
      composerInputSendKeyToken, UserSettings::SendMessageKey::Enter));

    const auto loadedComposerInputAutoReplaceEmoji =
      QString::fromStdString(static_cast<std::string>(snapshot.composer.input_auto_replace_emoji))
        .trimmed();
    const auto composerInputAutoReplaceEmojiToken = loadedComposerInputAutoReplaceEmoji.isEmpty()
                                                      ? QStringLiteral("always")
                                                      : loadedComposerInputAutoReplaceEmoji;
    settings.setComposerInputAutoReplaceEmoji(cfg::autoReplaceEmojiFromStorage(
      composerInputAutoReplaceEmojiToken, UserSettings::AutoReplaceEmoji::Always));

    const auto loadedComposerInputEmojiPreferredGender =
      QString::fromStdString(
        static_cast<std::string>(snapshot.composer.input_emoji_preferred_gender))
        .trimmed();
    const auto composerInputEmojiPreferredGenderToken =
      loadedComposerInputEmojiPreferredGender.isEmpty() ? QStringLiteral("no_preference")
                                                        : loadedComposerInputEmojiPreferredGender;
    settings.setComposerInputEmojiPreferredGender(cfg::emojiPreferredGenderFromStorage(
      composerInputEmojiPreferredGenderToken, UserSettings::EmojiPreferredGender::NoPreference));

    const auto loadedComposerInputEmojiPreferredSkinTone =
      QString::fromStdString(
        static_cast<std::string>(snapshot.composer.input_emoji_preferred_skin_tone))
        .trimmed();
    const auto composerInputEmojiPreferredSkinToneToken =
      loadedComposerInputEmojiPreferredSkinTone.isEmpty()
        ? QStringLiteral("no_preference")
        : loadedComposerInputEmojiPreferredSkinTone;
    settings.setComposerInputEmojiPreferredSkinTone(
      cfg::emojiPreferredSkinToneFromStorage(composerInputEmojiPreferredSkinToneToken,
                                             UserSettings::EmojiPreferredSkinTone::NoPreference));

    settings.setComposerInputInlineEmojiPickerEnabled(
      snapshot.composer.has_input_inline_emoji_picker_enabled
        ? snapshot.composer.input_inline_emoji_picker_enabled
        : true);
    settings.setComposerInputInlineRoomPickerEnabled(
      snapshot.composer.has_input_inline_room_picker_enabled
        ? snapshot.composer.input_inline_room_picker_enabled
        : true);
    settings.setComposerInputInlineUserPickerEnabled(
      snapshot.composer.has_input_inline_user_picker_enabled
        ? snapshot.composer.input_inline_user_picker_enabled
        : true);
    settings.setComposerTypingSendEnabled(
      snapshot.composer.has_typing_send_enabled ? snapshot.composer.typing_send_enabled : true);
    settings.setComposerExtrasStickersEnabled(snapshot.composer.has_extras_stickers_enabled
                                                ? snapshot.composer.extras_stickers_enabled
                                                : false);
}

} // namespace settings::serializer
