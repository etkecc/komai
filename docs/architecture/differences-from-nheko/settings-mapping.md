# Settings Name Mapping (nheko -> Komai)

This reference is for porting changes between upstream nheko and Komai.
It is intentionally focused on naming and key mapping, not behavior changes:
- `Scope` maps old nheko-style names (C++ constants and flat keys) to Komai symbols/keys.
- A `-` in the nheko column means “no direct upstream nheko equivalent”.

Scope:
- Map nheko-style setting names (C++ constants and flat serialization keys) to Komai symbols/keys.
- Document Komai's current nested YAML keys and target files.
- Keep runtime/action-only rows where useful for patch translation context.

Not in scope:
- Backward-compatibility migration guarantees for old key names.

Source of truth:
- `src/settings/SettingKeys.h` (`SettingKey::*` constants).
- `src/settings/core/SettingsDefinitions.h` (`settings::core::SettingId` <-> `SettingKey` mapping).
- `src/settings/SettingsSerializerConfigSchema.cpp` (config load/save descriptors).
- `src/settings/ui/rows/*.inc` (`UserSettingsModel` row metadata).
- `src/settings/ui/facade/UserSettingsPage.h` (runtime settings API exposed to QML).
- `src/settings/ui/facade/UserSettingsCoreStoreBridgeEntries.inc` (runtime getter mapping to `SettingId`).

Note:
- `settings::core::SettingId::UiScaleFactor` uses `ui.scale.factor` in profile `config.yml`; startup applies it via
  `QT_SCALE_FACTOR` only when the environment variable is not already set.

## UI Settings Mapping

| Tab | Section | Setting | Komai Symbol | nheko Flat Key | Komai YAML Key | Komai Target | Persisted? |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Look & Feel | APPEARANCE | Theme | `settings::core::SettingId::UiThemeSlug` | `theme` | `ui.theme.slug` | config.yml | yes |
| Look & Feel | FONTS & SCALING | Font family | `settings::core::SettingId::UiFontFamily` | `font_family` | `ui.font.family` | config.yml | yes |
| Look & Feel | FONTS & SCALING | Font size | `settings::core::SettingId::UiFontSizePt` | `font_size` | `ui.font.size_pt` | config.yml | yes |
| Look & Feel | FONTS & SCALING | Emoji font family | `settings::core::SettingId::UiFontEmojiFamily` | `emoji_font_family` | `ui.font.emoji_family` | config.yml | yes |
| Look & Feel | FONTS & SCALING | Scale factor | `settings::core::SettingId::UiScaleFactor` | `settings/scale_factor` | `ui.scale.factor` | config.yml | yes |
| Look & Feel | APPEARANCE | Enable UI animations | `settings::core::SettingId::UiMotionAnimationsEnabled` | `reduced_motion` | `ui.motion.enable_animations` | config.yml | yes |
| Timeline | PRESENTATION | Maximum message width | `settings::core::SettingId::TimelineMessagesLayoutMaxWidthPercent` | *(new)* | `timeline.messages.layout.max_width_percent` | config.yml | yes |
| Look & Feel | AVATARS | Use circular avatars | `settings::core::SettingId::UiAvatarsCircular` | `use_circular_avatars` | `ui.avatars.circular` | config.yml | yes |
| Look & Feel | AVATARS | Default avatar style | `settings::core::SettingId::UiAvatarsDefaultAvatarStyle` | _(removed)_ | `ui.avatars.default_avatar_style` | config.yml | yes |
| Look & Feel | APPEARANCE | Density | `settings::core::SettingId::UiLayoutDensity` | `compact_room_list` | `ui.layout.density` | config.yml | yes |
| Navigation | ROOM LIST | Show last message timestamp | `settings::core::SettingId::NavigationRoomListShowLastMessageTime` | `show_room_list_time` | `navigation.room_list.show_last_message_timestamp` | config.yml | yes |
| Navigation | ROOM LIST | Show last message preview | `settings::core::SettingId::NavigationRoomListLastMessagePreview` | `show_last_message_preview` | `navigation.room_list.last_message_preview` | config.yml | yes |
| Navigation | ROOM LIST | Show unread indicators | `settings::core::SettingId::NavigationRoomListShowUnreadIndicators` | `-` | `navigation.room_list.show_unread_indicators` | config.yml | yes |
| Navigation | COMMUNITIES SIDEBAR | Show unread indicators | `settings::core::SettingId::NavigationCommunitiesShowUnreadIndicators` | `-` | `navigation.communities.show_unread_indicators` | config.yml | yes |
| Look & Feel | APPEARANCE | Scrollbar visibility | `settings::core::SettingId::UiScrollbarPolicy` | `scrollbars_in_roomlist` | `ui.scrollbar_policy` | config.yml | yes |
| Navigation | ROOM LIST | Sorting | `settings::core::SettingId::NavigationRoomListSort` | `room_sort_order` | `navigation.room_list.sort` | config.yml | yes |
| Navigation | COMMUNITIES SIDEBAR | Show Direct Chats filter | `settings::core::SettingId::NavigationCommunitiesFilterDirectChats` | `-` | `navigation.communities.filters.direct_chats` | config.yml | yes |
| Navigation | COMMUNITIES SIDEBAR | Show Favourites filter | `settings::core::SettingId::NavigationCommunitiesFilterFavourites` | `-` | `navigation.communities.filters.favourites` | config.yml | yes |
| Navigation | COMMUNITIES SIDEBAR | Show Low Priority filter | `settings::core::SettingId::NavigationCommunitiesFilterLowPriority` | `-` | `navigation.communities.filters.low_priority` | config.yml | yes |
| Desktop | SYSTEM TRAY | Close to tray | `settings::core::SettingId::DesktopSystemTrayEnabled` | `tray` | `desktop.system_tray.enabled` | config.yml | yes |
| Desktop | SYSTEM TRAY | Start in tray | `settings::core::SettingId::DesktopSystemTrayAutostart` | `start_in_tray` | `desktop.system_tray.autostart` | config.yml | yes |
| Integrations | D-BUS | D-Bus access | `settings::core::SettingId::IntegrationsDbusApiAccess` | `-` | `integrations.dbus.access` | config.yml | yes |
| Integrations | BROWSER | Browser open command (Komai-only) | `settings::core::SettingId::IntegrationsBrowserCommand` | `-` | `integrations.browser.command` | config.yml | yes |
| - | REMOVED | Interaction mode | - | `mobile_mode` | - | - | no |
| - | REMOVED | Enable swipe gestures | - | `enable_swipe_gestures` | - | - | no |
| Timeline | PRESENTATION | Style | `settings::core::SettingId::TimelineMessagesStyle` | `bubbles` | `timeline.messages.style` | config.yml | yes |
| Timeline | PRESENTATION | Avatar size | `settings::core::SettingId::TimelineMessagesLayoutAvatarSize` | `small_avatars` | `timeline.messages.layout.avatar_size` | config.yml | yes |
| Timeline | PRESENTATION | Show avatar next to own message bubbles | `settings::core::SettingId::TimelineMessagesLayoutShowOwnAvatar` | `show_own_avatar_in_bubble_layout` | `timeline.messages.layout.show_own_avatar` | config.yml | yes |
| Timeline | PRESENTATION | Show sender username above messages | `settings::core::SettingId::TimelineMessagesSenderUsername` | `show_sender_username` | `timeline.messages.sender_username` | config.yml | yes |
| Timeline | PRESENTATION | Enlarge emoji-only messages | `settings::core::SettingId::TimelineMessagesEmojiOnlyEnlarge` | `enlarge_emoji_only_messages` | `timeline.messages.emoji_only_enlarge` | config.yml | yes |
| Timeline | PRESENTATION | Highlight message on hover | `settings::core::SettingId::TimelineMessagesHoverHighlight` | `message_hover_highlight` | `timeline.messages.hover_highlight` | config.yml | yes |
| Timeline | PRESENTATION | Syntax highlight formatted code blocks | `settings::core::SettingId::TimelineFormattedCodeSyntaxHighlighting` | `-` | `timeline.messages.formatted.code_syntax_highlighting` | config.yml | yes |
| Timeline | PRESENTATION | Show message effects | `settings::core::SettingId::TimelineMediaEffectsEnabled` | `fancy_effects` | `timeline.media.effects.enabled` | config.yml | yes |
| Timeline | ACTIONS | Actions activation policy | `settings::core::SettingId::TimelineMessageActionsActivationPolicy` | `show_action_buttons` | `timeline.messages.actions.activation_policy` | config.yml | yes |
| Timeline | ACTIONS | Pinned reactions | `settings::core::SettingId::TimelineMessageActionsPinnedReactions` | `pinned_reactions` | `timeline.messages.actions.pinned_reactions` | config.yml | yes |
| Timeline | MEDIA HANDLING | Play animated images only on hover | `settings::core::SettingId::TimelineMediaAnimateOnHover` | `animate_images_on_hover` | `timeline.media.animate_on_hover` | config.yml | yes |
| Timeline | MEDIA HANDLING | Show images automatically | `settings::core::SettingId::TimelineMediaImageDisplay` | `show_image` | `timeline.media.image_display` | config.yml | yes |
| Timeline | MEDIA HANDLING | Open in an external viewer | `settings::core::SettingId::TimelineMediaOpenImagesExternal` | `open_images_in_external_app` | `timeline.media.open_images_external` | config.yml | yes |
| Timeline | MEDIA HANDLING | Open in an external player | `settings::core::SettingId::TimelineMediaOpenVideosExternal` | `open_videos_in_external_app` | `timeline.media.open_videos_external` | config.yml | yes |
| Timeline | MEDIA HANDLING | Open in an external player | `settings::core::SettingId::TimelineMediaOpenAudioExternal` | *(new)* | `timeline.media.open_audio_external` | config.yml | yes |
| Timeline | MEDIA HANDLING | Default playback speed | `settings::core::SettingId::TimelineMediaDefaultAudioPlaybackSpeed` | *(new)* | `timeline.media.default_audio_playback_speed` | config.yml | yes |
| Composer | INPUT | Auto-convert Markdown to HTML | `settings::core::SettingId::ComposerInputMarkdownToHtmlEnabled` | `markdown` | `composer.input.markdown_to_html.enabled` | config.yml | yes |
| Composer | INPUT | Send key | `settings::core::SettingId::ComposerInputSendKey` | `send_message_key` | `composer.input.send_key` | config.yml | yes |
| Composer | INPUT | Auto-replace emoticons with emoji | `settings::core::SettingId::ComposerInputAutoReplaceEmoji` | `auto_replace_emoji` | `composer.input.auto_replace_emoji` | config.yml | yes |
| Composer | INPUT | Inline emoji picker | `settings::core::SettingId::ComposerInputInlineEmojiPickerEnabled` | *(new)* | `composer.input.inline_emoji_picker.enabled` | config.yml | yes |
| Composer | INPUT | Inline room picker | `settings::core::SettingId::ComposerInputInlineRoomPickerEnabled` | *(new)* | `composer.input.inline_room_picker.enabled` | config.yml | yes |
| Composer | INPUT | Inline user picker | `settings::core::SettingId::ComposerInputInlineUserPickerEnabled` | *(new)* | `composer.input.inline_user_picker.enabled` | config.yml | yes |
| Composer | FEEDBACK | Show others when I'm typing (global default; per-room override at `composer.typing.send.by_room`) | `settings::core::SettingId::ComposerTypingSendGlobal` | `typing_notifications` | `composer.typing.send.global` | config.yml | yes |
| Composer | -- | Enable stickers | *(removed -- not yet supported in Komai; see [Stickers](../../user-guide/features/stickers.md))* | `enable_stickers` | -- | -- | -- |
| Timeline | FEEDBACK | Show when others are typing | `settings::core::SettingId::TimelineTypingShowEnabled` | `typing_notifications` | `timeline.typing.show.enabled` | config.yml | yes |
| Timeline | FEEDBACK | Read receipts | `settings::core::SettingId::TimelineReadReceiptsEnabled` | `read_receipts` | `timeline.read_receipts.enabled` | config.yml | yes |
| Desktop | SYSTEM NOTIFICATIONS | Enable system notifications | `settings::core::SettingId::DesktopNotificationsEnabled` | `desktop_notifications` | `desktop.notifications.enabled` | config.yml | yes |
| Desktop | SYSTEM NOTIFICATIONS | Flash app window/taskbar on incoming messages | `settings::core::SettingId::DesktopNotificationsAttentionOnIncoming` | `alert_on_incoming_messages` | `desktop.notifications.attention_on_incoming` | config.yml | yes |
| Desktop | SYSTEM NOTIFICATIONS | Message content in notifications | `settings::core::SettingId::DesktopNotificationsMessageContentPolicy` | `decrypt_notifications` | `desktop.notifications.message_content_policy` | config.yml | yes |
| Desktop | ATTENTION INDICATORS | Show attention count in window title | `settings::core::SettingId::DesktopAttentionWindowTitleEnabled` | `-` | `desktop.attention.window_title.enabled` | config.yml | yes |
| Desktop | ATTENTION INDICATORS | Show attention count on app icon/taskbar badge | `settings::core::SettingId::DesktopAttentionAppBadgeEnabled` | `-` | `desktop.attention.app_badge.enabled` | config.yml | yes |
| Calls | GENERAL | Enable legacy calls | `settings::core::SettingId::CallsLegacyEnabled` | `enable_legacy_calls` | `calls.legacy.enabled` | config.yml | yes |
| Calls | GENERAL | Use turn.matrix.org as fallback relay | `settings::core::SettingId::CallsRelayUseFallbackServer` | `use_fallback_call_relay_server` | `calls.relay.use_fallback_server` | config.yml | yes |
| Calls | DEVICES | Microphone | `settings::core::SettingId::CallsDevicesMicrophone` | `microphone` | `calls.devices.microphone` | config.yml | yes |
| Calls | DEVICES | Camera | `settings::core::SettingId::CallsDevicesCamera` | `camera` | `calls.devices.camera` | config.yml | yes |
| Calls | DEVICES | Camera resolution | `settings::core::SettingId::CallsDevicesCameraResolution` | `camera_resolution` | `calls.devices.camera_resolution` | config.yml | yes |
| Calls | DEVICES | Camera frame rate | `settings::core::SettingId::CallsDevicesCameraFrameRate` | `camera_frame_rate` | `calls.devices.camera_frame_rate` | config.yml | yes |
| Calls | DEVICES | Ringtone | `settings::core::SettingId::CallsAudioRingtone` | `ringtone` | `calls.audio.ringtone` | config.yml | yes |
| Calls | SCREEN SHARING | Screen share frame rate | `settings::core::SettingId::CallsScreenshareFrameRate` | `screen_share_frame_rate` | `calls.screenshare.frame_rate` | config.yml | yes |
| Calls | SCREEN SHARING | Include camera picture-in-picture | `settings::core::SettingId::CallsScreensharePictureInPicture` | `screen_share_pip` | `calls.screenshare.picture_in_picture` | config.yml | yes |
| Calls | SCREEN SHARING | Show participant camera while screen sharing | `settings::core::SettingId::CallsScreenshareIncludeRemoteVideo` | `screen_share_remote_video` | `calls.screenshare.include_remote_video` | config.yml | yes |
| Calls | SCREEN SHARING | Show mouse cursor | `settings::core::SettingId::CallsScreenshareShowCursor` | `screen_share_hide_cursor` | `calls.screenshare.show_cursor` | config.yml | yes |
| Desktop | WINDOW BLUR | Blur on focus loss | `settings::core::SettingId::DesktopWindowFocusBlurEnabled` | `privacy_screen` | `desktop.window_focus_blur.enabled` | config.yml | yes |
| Desktop | WINDOW BLUR | Blur delay (seconds) | `settings::core::SettingId::DesktopWindowFocusBlurDelaySeconds` | `privacy_screen_timeout_seconds` | `desktop.window_focus_blur.delay_seconds` | config.yml | yes |
| Timeline | DATA & MAINTENANCE | Hidden events | `UserSettingsModel::HiddenTimelineEvents` | `-` | `timeline.hidden_events.global`, `timeline.hidden_events.by_room` | config.yml | no |
| Account | USERS | Ignored users | `UserSettingsModel::IgnoredUsers` | `-` | `account.users.ignored` | runtime/UI-specific | no |
| Network | ENCRYPTION | Send encrypted messages to verified users only | `settings::core::SettingId::EncryptionKeySharingOnlyVerifiedUsers` | `only_share_keys_with_verified_users` | `network.encryption.only_verified_users` | config.yml | yes |
| Network | ENCRYPTION | Share keys with verified users and devices | `settings::core::SettingId::EncryptionKeySharingShareWithTrusted` | `share_keys_with_trusted_users` | `network.encryption.share_with_trusted` | config.yml | yes |
| Network | ENCRYPTION | Enable online key backup | `settings::core::SettingId::EncryptionBackupOnlineEnabled` | `use_online_key_backup` | `network.encryption.key_backup` | config.yml | yes |
| Session | ACCOUNT | User ID | `UserSettingsModel::UserId` | `user_id` | `session.account.user_id` | session.yml | yes |
| Session | ACCOUNT | Homeserver | `UserSettingsModel::Homeserver` | `homeserver` | `session.account.homeserver` | session.yml | yes |
| Session | ACCOUNT | Profile | `UserSettingsModel::Profile` | `-` | `session.profile.name` | runtime only | no |
| Session | DEVICE | Device ID | `UserSettingsModel::DeviceId` | `device_id` | `session.device.id` | session.yml | yes |
| Session | DEVICE | Device fingerprint | `UserSettingsModel::DeviceFingerprint` | `-` | `session.device.fingerprint` | derived/runtime | no |
| Session | DEVICE | Access token | `UserSettingsModel::AccessToken` | `access_token` | `secrets.__session.access_token` | secret backend (fallback: secrets.yml) | yes (stored inside `session.secrets` payload) |
| Session | ACTIONS | Log out | `UserSettingsModel::Logout` | `-` | `session.actions.logout` | action only | no |
| About | APPLICATION | Name | `UserSettingsModel::AppName` | `-` | `about.application.name` | derived/runtime | no |
| About | APPLICATION | Platform | `UserSettingsModel::Platform` | `-` | `about.application.platform` | derived/runtime | no |
| About | APPLICATION | Based on | `UserSettingsModel::BasedOn` | `-` | `about.application.based_on` | derived/runtime | no |
| About | APPLICATION | Created by | `UserSettingsModel::MaintainedBy` | `-` | `about.application.maintained_by` | derived/runtime | no |

## Additional Persisted Keys (Not in UI Settings Table)

| nheko Flat Key | Komai YAML Key | Komai Target | Value Type | Notes |
| --- | --- | --- | --- | --- |
| `window_width` | `ui.window.width_px` | state.yml | int | runtime window geometry |
| `window_height` | `ui.window.height_px` | state.yml | int | runtime window geometry |
| `room_list_width` | `navigation.room_list.width_px` | state.yml | int | runtime sidebar width |
| `community_list_width` | `navigation.communities.width_px` | state.yml | int | runtime sidebar width |
| `current_tag_id` | `navigation.communities.filtering.current` | state.yml | text | runtime community sidebar state |
| `-` | `navigation.room_list.current_room_id` | state.yml | text | last open room restored on restart |
| `hidden_tags` | `navigation.communities.filtering.global_excludes` | state.yml | list(text) | filters excluded from "All rooms" |
| `muted_tags` | `navigation.communities.filtering.unread_indicators_hidden` | state.yml | list(text) | filters with unread indicators hidden |
| `hidden_pins` | `timeline.pins.hidden` | state.yml | list(text) | runtime timeline state |
| `hidden_widgets` | `timeline.widgets.hidden` | state.yml | list(text) | runtime timeline state |
| `recent_reactions` | `composer.reactions.recent` | state.yml | list(text) | runtime convenience state |
| `room_drafts` | `composer.drafts.by_room` | state.yml | map(text->text) | unsent composer drafts per room, restored across restarts |
| `collapsed_spaces` | `navigation.communities.filtering.collapsed_spaces` | state.yml | list(text) | runtime expansion state |
| `presence` | `network.presence.status_policy` | config.yml | enum | account-scoped preference |
| `screen_share_frame_rate` | `calls.screenshare.frame_rate` | config.yml | int | advanced calls/screenshare pref |
| `screen_share_pip` | `calls.screenshare.picture_in_picture` | config.yml | bool | advanced calls/screenshare pref |
| `screen_share_remote_video` | `calls.screenshare.include_remote_video` | config.yml | bool | advanced calls/screenshare pref |
| `screen_share_hide_cursor` | `calls.screenshare.show_cursor` | config.yml | bool | key semantics flipped to positive naming in Komai |
| `disable_certificate_validation` | `network.tls.enable_certificate_validation` | config.yml | bool | inverted from upstream key semantics; positive in Komai |
| `enable_http3` | `network.http3.enabled` | config.yml | bool | network advanced pref |
| `run_without_secure_secrets_service` | `secrets.provider` | config.yml | bool -> enum | mapped: false=secret_service, true=file; no direct runtime toggle in Komai |
| `secrets` | `secrets` | secret backend (fallback: secrets.yml) | map(text->text) | secret map; do not keep in config.yml |
