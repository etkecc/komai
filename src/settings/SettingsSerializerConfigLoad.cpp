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

namespace cfg = settings::serializer::config;
namespace settings::serializer {

void
loadConfig(UserSettings &settings, const ::komai::rust::SettingsLoadedConfig &snapshot)
{
    // Defaults are resolved by the Rust FFI layer — C++ unconditionally trusts
    // the snapshot values.

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

    settings.setUiFontSizePt(snapshot.ui.font_size_pt);
    settings.setUiFontFamily(
      QString::fromStdString(static_cast<std::string>(snapshot.ui.font_family)));
    settings.setUiFontEmojiFamily(
      QString::fromStdString(static_cast<std::string>(snapshot.ui.font_emoji_family)));
    settings.setUiMotionAnimationsEnabled(snapshot.ui.motion_animations_enabled);
    settings.setUiScrollbarPolicy(cfg::scrollbarPolicyFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.ui.scrollbar_policy)).trimmed(),
      UserSettings::ScrollbarPolicy::WhenNeeded));
    settings.setUiAvatarsDefaultAvatarStyle(cfg::defaultAvatarStyleFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.ui.default_avatar_style)).trimmed(),
      UserSettings::DefaultAvatarStyle::BoringAvatarsBauhaus));
    settings.setUiInputMode(detail::fromStorageUiInputMode(
      QString::fromStdString(static_cast<std::string>(snapshot.ui.input_mode)).trimmed()));
    settings.setUiScaleFactor(snapshot.ui.scale_factor);
    settings.setUiInputTouchSwipeGesturesEnabled(snapshot.ui.input_touch_swipe_gestures_enabled);
    settings.setUiAvatarsCircular(snapshot.ui.avatars_circular);
    settings.setUiLayoutCompactMode(snapshot.ui.layout_compact_mode);

    settings.setNavigationRoomListShowLastMessageTime(
      snapshot.navigation.room_list.show_last_message_time);
    settings.setNavigationRoomListLastMessagePreview(cfg::lastMessagePreviewFromStorage(
      QString::fromStdString(
        static_cast<std::string>(snapshot.navigation.room_list.last_message_preview))
        .trimmed(),
      UserSettings::LastMessagePreview::Always));
    settings.setNavigationRoomListShowUnreadCounts(
      snapshot.navigation.room_list.show_unread_counts);
    settings.setNavigationCommunitiesShowUnreadCounts(
      snapshot.navigation.communities.show_unread_counts);
    settings.setNavigationRoomListSort(cfg::roomSortOrderFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.navigation.room_list.sort))
        .trimmed(),
      UserSettings::RoomSortOrder::UnreadFirst_Recent));

    settings.setNavigationCommunitiesFilterFavourites(
      snapshot.navigation.communities.filter_favourites);
    settings.setNavigationCommunitiesFilterPeople(snapshot.navigation.communities.filter_people);
    settings.setNavigationCommunitiesFilterBots(snapshot.navigation.communities.filter_bots);
    settings.setNavigationCommunitiesFilterGroups(snapshot.navigation.communities.filter_groups);
    settings.setNavigationCommunitiesFilterServerNotices(
      snapshot.navigation.communities.filter_server_notices);
    settings.setNavigationCommunitiesFilterLowPriority(
      snapshot.navigation.communities.filter_low_priority);

    settings.setNavigationRoomListOpeningPolicy(cfg::roomListOpeningPolicyFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.navigation.room_list.opening_policy))
        .trimmed(),
      UserSettings::RoomListOpeningPolicy::ReuseActiveTab));
    settings.setNavigationTabsShowPinButton(cfg::tabPinButtonVisibilityFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.navigation.tabs.show_pin_button))
        .trimmed(),
      UserSettings::TabPinButtonVisibility::Never));
    settings.setNavigationTabsPinnedTabLabel(cfg::tabLabelDisplayFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.navigation.tabs.pinned_tab_label))
        .trimmed(),
      UserSettings::TabLabelDisplay::AvatarOnly));
    settings.setNavigationTabsTabLabel(cfg::tabLabelDisplayFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.navigation.tabs.tab_label))
        .trimmed(),
      UserSettings::TabLabelDisplay::AvatarAndLabel));
    settings.setNavigationTabsPreferredWidthPx(snapshot.navigation.tabs.preferred_width_px);
    settings.setNavigationTabsMinimumWidthPx(snapshot.navigation.tabs.minimum_width_px);
    settings.setNavigationTabsMaxRecentlyClosedTimelines(
      snapshot.navigation.tabs.max_recently_closed_timelines);

    {
        QStringList hiddenEventValues;
        for (const auto &value : snapshot.timeline.hidden_events.global)
            hiddenEventValues.push_back(QString::fromStdString(static_cast<std::string>(value)));
        settings.setHiddenTimelineEventTypes(hiddenEventValues);
    }
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

    settings.setTimelineMessagesStyle(cfg::timelineMessagesStyleFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.timeline.messages.style)).trimmed(),
      UserSettings::TimelineMessagesStyle::Bubbles));
    settings.setTimelineMessagesLayoutPositioning(cfg::timelineMessagesLayoutPositioningFromStorage(
      QString::fromStdString(
        static_cast<std::string>(snapshot.timeline.messages.layout_positioning))
        .trimmed(),
      UserSettings::TimelineMessagesLayoutPositioning::OpposingBySender));
    settings.setTimelineUserColorCodingPolicy(cfg::timelineUserColorCodingPolicyFromStorage(
      QString::fromStdString(
        static_cast<std::string>(snapshot.timeline.messages.user_color_coding_policy))
        .trimmed(),
      UserSettings::TimelineUserColorCodingPolicy::AdaptiveByRoomSize));
    settings.setTimelineMessagesLayoutAvatarSize(cfg::avatarSizeFromStorage(
      QString::fromStdString(
        static_cast<std::string>(snapshot.timeline.messages.layout_avatar_size))
        .trimmed(),
      UserSettings::AvatarSize::Regular));
    settings.setTimelineMessagesLayoutShowOwnAvatar(
      snapshot.timeline.messages.layout_show_own_avatar);
    settings.setTimelineMessagesLayoutMaxWidthPercent(
      snapshot.timeline.messages.layout_max_width_percent);
    settings.setTimelineMessagesSenderUsername(cfg::showSenderUsernameFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.timeline.messages.sender_username))
        .trimmed(),
      UserSettings::ShowSenderUsername::OnlyInLargeRooms));
    settings.setTimelineMessagesEmojiOnlyEnlarge(snapshot.timeline.messages.emoji_only_enlarge);
    settings.setTimelineMessagesHoverHighlight(snapshot.timeline.messages.hover_highlight);
    settings.setTimelineThreadsCollapseReplies(snapshot.timeline.threads.collapse_replies_global);
    {
        QMap<QString, bool> byRoom;
        for (const auto &entry : snapshot.timeline.threads.collapse_replies_by_room)
            byRoom.insert(QString::fromStdString(static_cast<std::string>(entry.key)), entry.value);
        settings.setTimelineThreadsCollapseRepliesByRoom(byRoom);
    }
    settings.setTimelineFormattedCodeSyntaxHighlighting(
      snapshot.timeline.formatted.code_syntax_highlighting);
    settings.setTimelineTypingShowEnabled(snapshot.timeline.typing.show_enabled);
    settings.setTimelineReadReceiptsEnabled(snapshot.timeline.read_receipts.enabled);

    settings.setTimelineMessageActionsActivationPolicy(
      cfg::timelineMessageActionsActivationPolicyFromStorage(
        QString::fromStdString(
          static_cast<std::string>(snapshot.timeline.message_actions.activation_policy))
          .trimmed(),
        UserSettings::TimelineMessageActionsActivationPolicy::ActionsButton));
    settings.setTimelineMessageActionsPinnedReactions(
      QString::fromStdString(
        static_cast<std::string>(snapshot.timeline.message_actions.pinned_reactions))
          .trimmed()
          .isEmpty()
        ? QString::fromUtf8(settings::core::definitions::kDefaultPinnedReactions)
        : QString::fromStdString(
            static_cast<std::string>(snapshot.timeline.message_actions.pinned_reactions)));

    settings.setTimelineMediaEffectsEnabled(snapshot.timeline.media.effects_enabled);
    settings.setTimelineMediaAnimateOnHover(snapshot.timeline.media.animate_on_hover);
    settings.setTimelineMediaImageDisplay(cfg::showImageFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.timeline.media.image_display))
        .trimmed(),
      UserSettings::ShowImage::Always));
    settings.setTimelineMediaOpenImagesExternal(snapshot.timeline.media.open_images_external);
    settings.setTimelineMediaOpenVideosExternal(snapshot.timeline.media.open_videos_external);
    settings.setTimelineMediaAutoplayGifVideos(snapshot.timeline.media.autoplay_gif_videos);
    settings.setTimelineMediaOpenAudioExternal(snapshot.timeline.media.open_audio_external);
    settings.setTimelineMediaDefaultAudioPlaybackSpeed(
      snapshot.timeline.media.default_audio_playback_speed);

    settings.setDesktopWindowFocusBlurEnabled(snapshot.desktop.window_focus_blur.enabled);
    settings.setDesktopWindowFocusBlurDelaySeconds(
      snapshot.desktop.window_focus_blur.delay_seconds);
    settings.setEncryptionKeySharingOnlyVerifiedUsers(
      snapshot.network.encryption.only_verified_users);
    settings.setEncryptionKeySharingShareWithTrusted(
      snapshot.network.encryption.share_with_trusted);
    settings.setEncryptionBackupOnlineEnabledFromConfig(snapshot.network.encryption.key_backup);

    settings.setCallsLegacyEnabled(snapshot.calls.legacy.enabled);
    settings.setCallsRelayUseFallbackServer(snapshot.calls.relay.use_fallback_server);
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

    settings.setCallsScreenshareFrameRate(snapshot.calls.screenshare.frame_rate);
    settings.setCallsScreensharePictureInPicture(snapshot.calls.screenshare.picture_in_picture);
    settings.setCallsScreenshareIncludeRemoteVideo(snapshot.calls.screenshare.include_remote_video);
    settings.setCallsScreenshareShowCursor(snapshot.calls.screenshare.show_cursor);

    settings.setDesktopNotificationsEnabled(snapshot.desktop.notifications.enabled);
    settings.setDesktopNotificationsAttentionOnIncoming(
      snapshot.desktop.notifications.attention_on_incoming);
    settings.setDesktopNotificationsMessageContentPolicy(
      cfg::notificationsMessageContentPolicyFromStorage(
        QString::fromStdString(
          static_cast<std::string>(snapshot.desktop.notifications.message_content_policy))
          .trimmed(),
        UserSettings::NotificationMessageContentPolicy::WheneverAvailable));
    settings.setDesktopAttentionWindowTitleEnabled(snapshot.desktop.attention.window_title.enabled);
    settings.setDesktopAttentionAppBadgeEnabled(snapshot.desktop.attention.app_badge.enabled);

    settings.setNetworkTlsEnableCertificateValidation(
      snapshot.network.tls_enable_certificate_validation);
    settings.setNetworkMrsEnabled(snapshot.network.mrs_enabled);

    const auto loadedNetworkMrsServerName =
      QString::fromStdString(static_cast<std::string>(snapshot.network.mrs_server_name)).trimmed();
    settings.setNetworkMrsServerName(
      loadedNetworkMrsServerName.isEmpty()
        ? QString::fromLatin1(settings::core::definitions::kDefaultNetworkMrsServerName)
        : loadedNetworkMrsServerName);

    settings.setNetworkHttp3Enabled(snapshot.network.http3_enabled);

    settings.setNetworkPresenceStatusPolicy(cfg::presenceFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.network.presence_status_policy))
        .trimmed(),
      UserSettings::Presence::AutomaticPresence));

    settings.setDesktopSystemTrayEnabled(snapshot.desktop.system_tray.enabled);
    settings.setDesktopSystemTrayAutostart(snapshot.desktop.system_tray.autostart);
    settings.setIntegrationsBrowserCommand(
      QString::fromStdString(static_cast<std::string>(snapshot.integrations.browser_command)));

    settings.setIntegrationsDbusApiAccess(cfg::dbusAccessFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.integrations.dbus_api_access))
        .trimmed(),
      IntegrationsDbusAccessNone));

    settings.setComposerInputMarkdownToHtmlEnabled(
      snapshot.composer.input_markdown_to_html_enabled);
    settings.setComposerInputSendKey(cfg::sendMessageKeyFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.composer.input_send_key)).trimmed(),
      UserSettings::SendMessageKey::Enter));
    settings.setComposerInputAutoReplaceEmoji(cfg::autoReplaceEmojiFromStorage(
      QString::fromStdString(static_cast<std::string>(snapshot.composer.input_auto_replace_emoji))
        .trimmed(),
      UserSettings::AutoReplaceEmoji::Always));
    settings.setComposerInputEmojiPreferredGender(cfg::emojiPreferredGenderFromStorage(
      QString::fromStdString(
        static_cast<std::string>(snapshot.composer.input_emoji_preferred_gender))
        .trimmed(),
      UserSettings::EmojiPreferredGender::NoPreference));
    settings.setComposerInputEmojiPreferredSkinTone(cfg::emojiPreferredSkinToneFromStorage(
      QString::fromStdString(
        static_cast<std::string>(snapshot.composer.input_emoji_preferred_skin_tone))
        .trimmed(),
      UserSettings::EmojiPreferredSkinTone::NoPreference));
    settings.setComposerInputInlineEmojiPickerEnabled(
      snapshot.composer.input_inline_emoji_picker_enabled);
    settings.setComposerInputInlineRoomPickerEnabled(
      snapshot.composer.input_inline_room_picker_enabled);
    settings.setComposerInputInlineUserPickerEnabled(
      snapshot.composer.input_inline_user_picker_enabled);
    settings.setComposerTypingSendEnabled(snapshot.composer.typing_send_enabled);
    settings.setComposerExtrasStickersEnabled(snapshot.composer.extras_stickers_enabled);
}

} // namespace settings::serializer
