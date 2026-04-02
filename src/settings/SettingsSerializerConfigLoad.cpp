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
        if (adapter.id == settings::core::SettingId::NotificationsMessageContentPolicy)
            continue;
        if (adapter.id == settings::core::SettingId::NetworkPresenceStatusPolicy)
            continue;
        if (adapter.id == settings::core::SettingId::IntegrationsDbusApiAccess)
            continue;
        if (adapter.id == settings::core::SettingId::ComposerInputSendKey)
            continue;
        if (adapter.id == settings::core::SettingId::ComposerInputAutoReplaceEmoji)
            continue;
        if (adapter.id == settings::core::SettingId::ComposerInputEmojiPreferredGender)
            continue;
        if (adapter.id == settings::core::SettingId::ComposerInputEmojiPreferredSkinTone)
            continue;
        if (adapter.id == settings::core::SettingId::SidebarsRoomListLastMessagePreview)
            continue;
        if (adapter.id == settings::core::SettingId::SidebarsRoomListSort)
            continue;
        if (adapter.id == settings::core::SettingId::SidebarsRoomListUnreadDetectionPolicy)
            continue;
        if (adapter.id == settings::core::SettingId::TimelineMessagesStyle)
            continue;
        if (adapter.id == settings::core::SettingId::TimelineMessagesPositioning)
            continue;
        if (adapter.id == settings::core::SettingId::TimelineUserColorCodingPolicy)
            continue;
        if (adapter.id == settings::core::SettingId::TimelineMessagesSenderUsername)
            continue;
        if (adapter.id == settings::core::SettingId::TimelineMessageActionsActivationPolicy)
            continue;
        if (adapter.id == settings::core::SettingId::TimelineMediaImageDisplay)
            continue;

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

    settings.setSidebarsRoomListShowLastMessageTime(
      snapshot.sidebars.room_list.has_show_last_message_time
        ? snapshot.sidebars.room_list.show_last_message_time
        : true);

    const auto loadedSidebarsRoomListLastMessagePreview =
      QString::fromStdString(
        static_cast<std::string>(snapshot.sidebars.room_list.last_message_preview))
        .trimmed();
    const auto sidebarsRoomListLastMessagePreviewToken =
      loadedSidebarsRoomListLastMessagePreview.isEmpty() ? QStringLiteral("always")
                                                         : loadedSidebarsRoomListLastMessagePreview;
    settings.setSidebarsRoomListLastMessagePreview(cfg::lastMessagePreviewFromStorage(
      sidebarsRoomListLastMessagePreviewToken, UserSettings::LastMessagePreview::Always));
    if (sidebarsRoomListLastMessagePreviewToken !=
        cfg::toStorageValue(settings.sidebarsRoomListLastMessagePreview())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          sidebarsRoomListLastMessagePreviewToken.toStdString(),
          SettingKey::SidebarsRoomListLastMessagePreview,
          cfg::toStorageValue(settings.sidebarsRoomListLastMessagePreview()).toStdString());
    }

    settings.setSidebarsRoomListShowCommunityCounts(
      snapshot.sidebars.room_list.has_show_community_counts
        ? snapshot.sidebars.room_list.show_community_counts
        : true);

    const auto loadedSidebarsRoomListSort =
      QString::fromStdString(static_cast<std::string>(snapshot.sidebars.room_list.sort)).trimmed();
    const auto sidebarsRoomListSortToken = loadedSidebarsRoomListSort.isEmpty()
                                             ? QStringLiteral("unread_first_recent")
                                             : loadedSidebarsRoomListSort;
    settings.setSidebarsRoomListSort(cfg::roomSortOrderFromStorage(
      sidebarsRoomListSortToken, UserSettings::RoomSortOrder::UnreadFirst_Recent));
    if (sidebarsRoomListSortToken != cfg::toStorageValue(settings.sidebarsRoomListSort())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          sidebarsRoomListSortToken.toStdString(),
          SettingKey::SidebarsRoomListSort,
          cfg::toStorageValue(settings.sidebarsRoomListSort()).toStdString());
    }

    const auto loadedSidebarsRoomListUnreadDetectionPolicy =
      QString::fromStdString(
        static_cast<std::string>(snapshot.sidebars.room_list.unread_detection_policy))
        .trimmed();
    const auto sidebarsRoomListUnreadDetectionPolicyToken =
      loadedSidebarsRoomListUnreadDetectionPolicy.isEmpty()
        ? QStringLiteral("any_event")
        : loadedSidebarsRoomListUnreadDetectionPolicy;
    settings.setSidebarsRoomListUnreadDetectionPolicy(cfg::unreadDetectionPolicyFromStorage(
      sidebarsRoomListUnreadDetectionPolicyToken, UserSettings::UnreadDetectionPolicy::AnyEvent));
    if (sidebarsRoomListUnreadDetectionPolicyToken !=
        cfg::toStorageValue(settings.sidebarsRoomListUnreadDetectionPolicy())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          sidebarsRoomListUnreadDetectionPolicyToken.toStdString(),
          SettingKey::SidebarsRoomListUnreadDetectionPolicy,
          cfg::toStorageValue(settings.sidebarsRoomListUnreadDetectionPolicy()).toStdString());
    }

    settings.setSidebarsCommunitiesVisible(
      snapshot.sidebars.communities.has_visible ? snapshot.sidebars.communities.visible : true);
    settings.setSidebarsCommunitiesFilterFavourites(
      snapshot.sidebars.communities.has_filter_favourites
        ? snapshot.sidebars.communities.filter_favourites
        : true);
    settings.setSidebarsCommunitiesFilterPeople(snapshot.sidebars.communities.has_filter_people
                                                  ? snapshot.sidebars.communities.filter_people
                                                  : true);
    settings.setSidebarsCommunitiesFilterBots(snapshot.sidebars.communities.has_filter_bots
                                                ? snapshot.sidebars.communities.filter_bots
                                                : true);
    settings.setSidebarsCommunitiesFilterGroups(snapshot.sidebars.communities.has_filter_groups
                                                  ? snapshot.sidebars.communities.filter_groups
                                                  : true);
    settings.setSidebarsCommunitiesFilterServerNotices(
      snapshot.sidebars.communities.has_filter_server_notices
        ? snapshot.sidebars.communities.filter_server_notices
        : true);
    settings.setSidebarsCommunitiesFilterLowPriority(
      snapshot.sidebars.communities.has_filter_low_priority
        ? snapshot.sidebars.communities.filter_low_priority
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
    if (timelineMessagesStyleToken != cfg::toStorageValue(settings.timelineMessagesStyle())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          timelineMessagesStyleToken.toStdString(),
          SettingKey::TimelineMessagesStyle,
          cfg::toStorageValue(settings.timelineMessagesStyle()).toStdString());
    }

    const auto loadedTimelineMessagesPositioning =
      QString::fromStdString(static_cast<std::string>(snapshot.timeline.messages.positioning))
        .trimmed();
    const auto timelineMessagesPositioningToken = loadedTimelineMessagesPositioning.isEmpty()
                                                    ? QStringLiteral("opposing_by_sender")
                                                    : loadedTimelineMessagesPositioning;
    settings.setTimelineMessagesPositioning(cfg::timelineMessagesPositioningFromStorage(
      timelineMessagesPositioningToken,
      UserSettings::TimelineMessagesPositioning::OpposingBySender));
    if (timelineMessagesPositioningToken !=
        cfg::toStorageValue(settings.timelineMessagesPositioning())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          timelineMessagesPositioningToken.toStdString(),
          SettingKey::TimelineMessagesPositioning,
          cfg::toStorageValue(settings.timelineMessagesPositioning()).toStdString());
    }

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
    if (timelineUserColorCodingPolicyToken !=
        cfg::toStorageValue(settings.timelineUserColorCodingPolicy())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          timelineUserColorCodingPolicyToken.toStdString(),
          SettingKey::TimelineUserColorCodingPolicy,
          cfg::toStorageValue(settings.timelineUserColorCodingPolicy()).toStdString());
    }

    settings.setTimelineMessagesLayoutSmallAvatars(
      snapshot.timeline.messages.has_layout_small_avatars
        ? snapshot.timeline.messages.layout_small_avatars
        : false);
    settings.setTimelineMessagesLayoutShowOwnAvatar(
      snapshot.timeline.messages.has_layout_show_own_avatar
        ? snapshot.timeline.messages.layout_show_own_avatar
        : true);

    const auto loadedTimelineMessagesSenderUsername =
      QString::fromStdString(static_cast<std::string>(snapshot.timeline.messages.sender_username))
        .trimmed();
    const auto timelineMessagesSenderUsernameToken = loadedTimelineMessagesSenderUsername.isEmpty()
                                                       ? QStringLiteral("only_in_large_rooms")
                                                       : loadedTimelineMessagesSenderUsername;
    settings.setTimelineMessagesSenderUsername(cfg::showSenderUsernameFromStorage(
      timelineMessagesSenderUsernameToken, UserSettings::ShowSenderUsername::OnlyInLargeRooms));
    if (timelineMessagesSenderUsernameToken !=
        cfg::toStorageValue(settings.timelineMessagesSenderUsername())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          timelineMessagesSenderUsernameToken.toStdString(),
          SettingKey::TimelineMessagesSenderUsername,
          cfg::toStorageValue(settings.timelineMessagesSenderUsername()).toStdString());
    }

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
    if (timelineMessageActionsActivationPolicyToken !=
        cfg::toStorageValue(settings.timelineMessageActionsActivationPolicy())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          timelineMessageActionsActivationPolicyToken.toStdString(),
          SettingKey::TimelineMessageActionsActivationPolicy,
          cfg::toStorageValue(settings.timelineMessageActionsActivationPolicy()).toStdString());
    }

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
    if (timelineMediaImageDisplayToken !=
        cfg::toStorageValue(settings.timelineMediaImageDisplay())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          timelineMediaImageDisplayToken.toStdString(),
          SettingKey::TimelineMediaImageDisplay,
          cfg::toStorageValue(settings.timelineMediaImageDisplay()).toStdString());
    }

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

    settings.setPrivacyWindowFocusBlurEnabled(snapshot.privacy.window_focus_blur.has_enabled
                                                ? snapshot.privacy.window_focus_blur.enabled
                                                : false);
    settings.setPrivacyWindowFocusBlurDelaySeconds(
      snapshot.privacy.window_focus_blur.has_delay_seconds
        ? snapshot.privacy.window_focus_blur.delay_seconds
        : settings::core::definitions::kDefaultPrivacyWindowFocusBlurDelaySeconds);
    settings.setPrivacyMaintenanceExpireEvents(snapshot.privacy.maintenance.has_expire_events
                                                 ? snapshot.privacy.maintenance.expire_events
                                                 : false);

    settings.setEncryptionKeySharingOnlyVerifiedUsers(
      snapshot.encryption.key_sharing.has_only_verified_users
        ? snapshot.encryption.key_sharing.only_verified_users
        : false);
    settings.setEncryptionKeySharingShareWithTrusted(
      snapshot.encryption.key_sharing.has_share_with_trusted
        ? snapshot.encryption.key_sharing.share_with_trusted
        : false);
    settings.setEncryptionBackupOnlineEnabledFromConfig(
      snapshot.encryption.backup.online.has_enabled ? snapshot.encryption.backup.online.enabled
                                                    : true);

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

    settings.setNotificationsEnabled(
      snapshot.notifications.has_enabled ? snapshot.notifications.enabled : true);
    settings.setNotificationsAttentionOnIncoming(snapshot.notifications.has_attention_on_incoming
                                                   ? snapshot.notifications.attention_on_incoming
                                                   : false);

    const auto loadedNotificationsMessageContentPolicy =
      QString::fromStdString(
        static_cast<std::string>(snapshot.notifications.message_content_policy))
        .trimmed();
    const auto notificationsMessageContentPolicyToken =
      loadedNotificationsMessageContentPolicy.isEmpty() ? QStringLiteral("whenever_available")
                                                        : loadedNotificationsMessageContentPolicy;
    settings.setNotificationsMessageContentPolicy(cfg::notificationsMessageContentPolicyFromStorage(
      notificationsMessageContentPolicyToken,
      UserSettings::NotificationMessageContentPolicy::WheneverAvailable));
    if (notificationsMessageContentPolicyToken !=
        cfg::toStorageValue(settings.notificationsMessageContentPolicy())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          notificationsMessageContentPolicyToken.toStdString(),
          SettingKey::NotificationsMessageContentPolicy,
          cfg::toStorageValue(settings.notificationsMessageContentPolicy()).toStdString());
    }

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
    if (networkPresenceStatusPolicyToken !=
        cfg::toStorageValue(settings.networkPresenceStatusPolicy())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          networkPresenceStatusPolicyToken.toStdString(),
          SettingKey::NetworkPresenceStatusPolicy,
          cfg::toStorageValue(settings.networkPresenceStatusPolicy()).toStdString());
    }

    settings.setIntegrationsSystemTrayEnabled(snapshot.integrations.has_system_tray_enabled
                                                ? snapshot.integrations.system_tray_enabled
                                                : false);
    settings.setIntegrationsSystemTrayAutostart(snapshot.integrations.has_system_tray_autostart
                                                  ? snapshot.integrations.system_tray_autostart
                                                  : false);
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
    if (integrationsDbusApiAccessToken !=
        cfg::dbusAccessToStorage(settings.integrationsDbusApiAccess())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          integrationsDbusApiAccessToken.toStdString(),
          SettingKey::IntegrationsDbusApiAccess,
          cfg::dbusAccessToStorage(settings.integrationsDbusApiAccess()).toStdString());
    }

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
    if (composerInputSendKeyToken != cfg::toStorageValue(settings.composerInputSendKey())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          composerInputSendKeyToken.toStdString(),
          SettingKey::ComposerInputSendKey,
          cfg::toStorageValue(settings.composerInputSendKey()).toStdString());
    }

    const auto loadedComposerInputAutoReplaceEmoji =
      QString::fromStdString(static_cast<std::string>(snapshot.composer.input_auto_replace_emoji))
        .trimmed();
    const auto composerInputAutoReplaceEmojiToken = loadedComposerInputAutoReplaceEmoji.isEmpty()
                                                      ? QStringLiteral("always")
                                                      : loadedComposerInputAutoReplaceEmoji;
    settings.setComposerInputAutoReplaceEmoji(cfg::autoReplaceEmojiFromStorage(
      composerInputAutoReplaceEmojiToken, UserSettings::AutoReplaceEmoji::Always));
    if (composerInputAutoReplaceEmojiToken !=
        cfg::toStorageValue(settings.composerInputAutoReplaceEmoji())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          composerInputAutoReplaceEmojiToken.toStdString(),
          SettingKey::ComposerInputAutoReplaceEmoji,
          cfg::toStorageValue(settings.composerInputAutoReplaceEmoji()).toStdString());
    }

    const auto loadedComposerInputEmojiPreferredGender =
      QString::fromStdString(
        static_cast<std::string>(snapshot.composer.input_emoji_preferred_gender))
        .trimmed();
    const auto composerInputEmojiPreferredGenderToken =
      loadedComposerInputEmojiPreferredGender.isEmpty() ? QStringLiteral("no_preference")
                                                        : loadedComposerInputEmojiPreferredGender;
    settings.setComposerInputEmojiPreferredGender(cfg::emojiPreferredGenderFromStorage(
      composerInputEmojiPreferredGenderToken, UserSettings::EmojiPreferredGender::NoPreference));
    if (composerInputEmojiPreferredGenderToken !=
        cfg::toStorageValue(settings.composerInputEmojiPreferredGender())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          composerInputEmojiPreferredGenderToken.toStdString(),
          SettingKey::ComposerInputEmojiPreferredGender,
          cfg::toStorageValue(settings.composerInputEmojiPreferredGender()).toStdString());
    }

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
    if (composerInputEmojiPreferredSkinToneToken !=
        cfg::toStorageValue(settings.composerInputEmojiPreferredSkinTone())) {
        activeLoggers().ui->warn(
          "Invalid value '{}' for '{}'; using '{}'",
          composerInputEmojiPreferredSkinToneToken.toStdString(),
          SettingKey::ComposerInputEmojiPreferredSkinTone,
          cfg::toStorageValue(settings.composerInputEmojiPreferredSkinTone()).toStdString());
    }

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
