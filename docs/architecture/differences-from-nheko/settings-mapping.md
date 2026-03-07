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
| Look & Feel | THEME | Theme | `settings::core::SettingId::UiThemeSlug` | `theme` | `ui.theme.slug` | config.yml | yes |
| Look & Feel | FONTS | Font family | `settings::core::SettingId::UiFontFamily` | `font_family` | `ui.font.family` | config.yml | yes |
| Look & Feel | FONTS | Font size | `settings::core::SettingId::UiFontSizePt` | `font_size` | `ui.font.size_pt` | config.yml | yes |
| Look & Feel | FONTS | Emoji font family | `settings::core::SettingId::UiFontEmojiFamily` | `emoji_font_family` | `ui.font.emoji_family` | config.yml | yes |
| Look & Feel | FONTS | Scale factor | `settings::core::SettingId::UiScaleFactor` | `settings/scale_factor` | `ui.scale.factor` | config.yml | yes |
| Look & Feel | BEHAVIOR | Enable UI animations | `settings::core::SettingId::UiMotionAnimationsEnabled` | `reduced_motion` | `ui.motion.enable_animations` | config.yml | yes |
| Look & Feel | LAYOUT | Maximum content width | `settings::core::SettingId::UiLayoutContentMaxWidthPx` | `max_timeline_width` | `ui.layout.content.max_width_px` | config.yml | yes |
| Look & Feel | AVATARS | Use circular avatars | `settings::core::SettingId::UiAvatarsCircular` | `use_circular_avatars` | `ui.avatars.circular` | config.yml | yes |
| Look & Feel | AVATARS | Default avatar style | `settings::core::SettingId::UiAvatarsDefaultAvatarStyle` | _(removed)_ | `ui.avatars.default_avatar_style` | config.yml | yes |
| Look & Feel | LAYOUT | Compact mode | `settings::core::SettingId::UiLayoutCompactMode` | `compact_room_list` | `ui.layout.compact_mode` | config.yml | yes |
| Sidebars | ROOM LIST | Show last message timestamp | `settings::core::SettingId::SidebarsRoomListShowLastMessageTime` | `show_room_list_time` | `sidebars.room_list.show_last_message_timestamp` | config.yml | yes |
| Sidebars | ROOM LIST | Show last message preview | `settings::core::SettingId::SidebarsRoomListLastMessagePreview` | `show_last_message_preview` | `sidebars.room_list.last_message_preview` | config.yml | yes |
| Sidebars | ROOM LIST | Show notification counts | `settings::core::SettingId::SidebarsRoomListShowCommunityCounts` | `show_community_notification_counts` | `sidebars.room_list.show_community_notification_counts` | config.yml | yes |
| Sidebars | ROOM LIST | Show scrollbars | `settings::core::SettingId::SidebarsRoomListScrollbarsEnabled` | `scrollbars_in_roomlist` | `sidebars.room_list.scrollbars.visible` | config.yml | yes |
| Sidebars | ROOM LIST | Sorting | `settings::core::SettingId::SidebarsRoomListSort` | `room_sort_order` | `sidebars.room_list.sort` | config.yml | yes |
| Sidebars | COMMUNITIES SIDEBAR | Show communities sidebar | `settings::core::SettingId::SidebarsCommunitiesVisible` | `show_communities_sidebar` | `sidebars.communities.visible` | config.yml | yes |
| Sidebars | COMMUNITIES SIDEBAR | Show Direct Chats filter | `settings::core::SettingId::SidebarsCommunitiesFilterDirectChats` | `-` | `sidebars.communities.filters.direct_chats` | config.yml | yes |
| Sidebars | COMMUNITIES SIDEBAR | Show Favourites filter | `settings::core::SettingId::SidebarsCommunitiesFilterFavourites` | `-` | `sidebars.communities.filters.favourites` | config.yml | yes |
| Sidebars | COMMUNITIES SIDEBAR | Show Low Priority filter | `settings::core::SettingId::SidebarsCommunitiesFilterLowPriority` | `-` | `sidebars.communities.filters.low_priority` | config.yml | yes |
| Integrations | SYSTEM TRAY | Minimize to tray | `settings::core::SettingId::IntegrationsSystemTrayEnabled` | `tray` | `integrations.system_tray.enabled` | config.yml | yes |
| Integrations | SYSTEM TRAY | Start in tray | `settings::core::SettingId::IntegrationsSystemTrayAutostart` | `start_in_tray` | `integrations.system_tray.autostart` | config.yml | yes |
| Integrations | D-BUS | D-Bus access | `settings::core::SettingId::IntegrationsDbusApiAccess` | `-` | `integrations.dbus.access` | config.yml | yes |
| Integrations | BROWSER | Browser open command (Komai-only) | `settings::core::SettingId::IntegrationsBrowserCommand` | `-` | `integrations.browser.command` | config.yml | yes |
| Look & Feel | BEHAVIOR | Interaction mode | `settings::core::SettingId::UiInputMode` | `mobile_mode` | `ui.input.mode` | config.yml | yes |
| Look & Feel | BEHAVIOR | Enable swipe gestures | `settings::core::SettingId::UiInputTouchSwipeGesturesEnabled` | `enable_swipe_gestures` | `ui.input.touch.swipe_gestures.enabled` | config.yml | yes |
| Timeline | PRESENTATION | Style | `settings::core::SettingId::TimelineMessagesStyle` | `bubbles` | `timeline.messages.style` | config.yml | yes |
| Timeline | PRESENTATION | Use small avatars | `settings::core::SettingId::TimelineMessagesLayoutSmallAvatars` | `small_avatars` | `timeline.messages.layout.small_avatars` | config.yml | yes |
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
| Timeline | MEDIA HANDLING | Open images in an external app | `settings::core::SettingId::TimelineMediaOpenImagesExternal` | `open_images_in_external_app` | `timeline.media.open_images_external` | config.yml | yes |
| Timeline | MEDIA HANDLING | Open videos in an external app | `settings::core::SettingId::TimelineMediaOpenVideosExternal` | `open_videos_in_external_app` | `timeline.media.open_videos_external` | config.yml | yes |
| Composer | INPUT | Auto-convert Markdown to HTML | `settings::core::SettingId::ComposerInputMarkdownToHtmlEnabled` | `markdown` | `composer.input.markdown_to_html.enabled` | config.yml | yes |
| Composer | INPUT | Send key | `settings::core::SettingId::ComposerInputSendKey` | `send_message_key` | `composer.input.send_key` | config.yml | yes |
| Composer | INPUT | Auto-replace emoticons with emoji | `settings::core::SettingId::ComposerInputAutoReplaceEmoji` | `auto_replace_emoji` | `composer.input.auto_replace_emoji` | config.yml | yes |
| Composer | INPUT | Inline emoji picker | `settings::core::SettingId::ComposerInputInlineEmojiPickerEnabled` | *(new)* | `composer.input.inline_emoji_picker.enabled` | config.yml | yes |
| Composer | INPUT | Inline room picker | `settings::core::SettingId::ComposerInputInlineRoomPickerEnabled` | *(new)* | `composer.input.inline_room_picker.enabled` | config.yml | yes |
| Composer | INPUT | Inline user picker | `settings::core::SettingId::ComposerInputInlineUserPickerEnabled` | *(new)* | `composer.input.inline_user_picker.enabled` | config.yml | yes |
| Composer | FEEDBACK | Show others when I'm typing | `settings::core::SettingId::ComposerTypingSendEnabled` | `typing_notifications` | `composer.typing.send.enabled` | config.yml | yes |
| Composer | EXTRAS | Enable stickers | `settings::core::SettingId::ComposerExtrasStickersEnabled` | `enable_stickers` | `composer.extras.stickers.enabled` | config.yml | yes |
| Timeline | FEEDBACK | Show when others are typing | `settings::core::SettingId::TimelineTypingShowEnabled` | `typing_notifications` | `timeline.typing.show.enabled` | config.yml | yes |
| Timeline | FEEDBACK | Read receipts | `settings::core::SettingId::TimelineReadReceiptsEnabled` | `read_receipts` | `timeline.read_receipts.enabled` | config.yml | yes |
| Notifications | SYSTEM NOTIFICATIONS | Enable system notifications | `settings::core::SettingId::NotificationsEnabled` | `desktop_notifications` | `notifications.enabled` | config.yml | yes |
| Notifications | SYSTEM NOTIFICATIONS | Flash app window/taskbar on incoming messages | `settings::core::SettingId::NotificationsAttentionOnIncoming` | `alert_on_incoming_messages` | `notifications.attention_on_incoming` | config.yml | yes |
| Notifications | SYSTEM NOTIFICATIONS | Message content in notifications | `settings::core::SettingId::NotificationsMessageContentPolicy` | `decrypt_notifications` | `notifications.message_content_policy` | config.yml | yes |
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
| Privacy | WINDOW BLUR | Blur on focus loss | `settings::core::SettingId::PrivacyWindowFocusBlurEnabled` | `privacy_screen` | `privacy.window_focus_blur.enabled` | config.yml | yes |
| Privacy | WINDOW BLUR | Blur delay (seconds) | `settings::core::SettingId::PrivacyWindowFocusBlurDelaySeconds` | `privacy_screen_timeout_seconds` | `privacy.window_focus_blur.delay_seconds` | config.yml | yes |
| Privacy | DATA & MAINTENANCE | Delete expired events periodically | `settings::core::SettingId::PrivacyMaintenanceExpireEvents` | `expire_events` | `privacy.maintenance.expire_events` | config.yml | yes |
| Privacy | DATA & MAINTENANCE | Hidden events | `UserSettingsModel::HiddenTimelineEvents` | `-` | `privacy.timeline.hidden_events` | runtime/UI-specific | no |
| Privacy | DATA & MAINTENANCE | Update community routing info periodically | `settings::core::SettingId::PrivacyMaintenanceUpdateSpaceVias` | `update_space_vias` | `privacy.maintenance.update_space_vias` | config.yml | yes |
| Privacy | USERS | Ignored users | `UserSettingsModel::IgnoredUsers` | `-` | `privacy.users.ignored` | runtime/UI-specific | no |
| Encryption | KEY SHARING | Send encrypted messages to verified users only | `settings::core::SettingId::EncryptionKeySharingOnlyVerifiedUsers` | `only_share_keys_with_verified_users` | `encryption.key_sharing.only_verified_users` | config.yml | yes |
| Encryption | KEY SHARING | Share keys with verified users and devices | `settings::core::SettingId::EncryptionKeySharingShareWithTrusted` | `share_keys_with_trusted_users` | `encryption.key_sharing.share_with_trusted` | config.yml | yes |
| Encryption | BACKUP | Enable online key backup | `settings::core::SettingId::EncryptionBackupOnlineEnabled` | `use_online_key_backup` | `encryption.backup.online.enabled` | config.yml | yes |
| Encryption | BACKUP | Session keys | `UserSettingsModel::SessionKeys` | `-` | `encryption.backup.session_keys` | action only | no |
| Encryption | CROSS-SIGNING | Online backup key | `UserSettingsModel::OnlineBackupKey` | `-` | `encryption.cross_signing.online_backup_key_cached` | derived/runtime | optional |
| Encryption | CROSS-SIGNING | Self signing key | `UserSettingsModel::SelfSigningKey` | `-` | `encryption.cross_signing.self_signing_key_cached` | derived/runtime | optional |
| Encryption | CROSS-SIGNING | User signing key | `UserSettingsModel::UserSigningKey` | `-` | `encryption.cross_signing.user_signing_key_cached` | derived/runtime | optional |
| Encryption | CROSS-SIGNING | Master signing key | `UserSettingsModel::MasterKey` | `-` | `encryption.cross_signing.master_key_cached` | derived/runtime | optional |
| Encryption | CROSS-SIGNING | Cross-signing secrets | `UserSettingsModel::CrossSigningSecrets` | `-` | `encryption.cross_signing.secrets` | action only | no |
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
| About | APPLICATION | Maintained by | `UserSettingsModel::MaintainedBy` | `-` | `about.application.maintained_by` | derived/runtime | no |

## Additional Persisted Keys (Not in UI Settings Table)

| nheko Flat Key | Komai YAML Key | Komai Target | Value Type | Notes |
| --- | --- | --- | --- | --- |
| `window_width` | `ui.window.width_px` | state.yml | int | runtime window geometry |
| `window_height` | `ui.window.height_px` | state.yml | int | runtime window geometry |
| `room_list_width` | `sidebars.room_list.width_px` | state.yml | int | runtime sidebar width |
| `community_list_width` | `sidebars.communities.width_px` | state.yml | int | runtime sidebar width |
| `current_tag_id` | `sidebars.communities.filtering.current` | state.yml | text | runtime community sidebar state |
| `-` | `sidebars.room_list.current_room_id` | state.yml | text | last open room restored on restart |
| `hidden_tags` | `sidebars.communities.filtering.global_excludes` | state.yml | list(text) | filters excluded from "All rooms" |
| `muted_tags` | `sidebars.communities.filtering.badges_hidden` | state.yml | list(text) | filters with attention badges hidden |
| `hidden_pins` | `timeline.pins.hidden` | state.yml | list(text) | runtime timeline state |
| `hidden_widgets` | `timeline.widgets.hidden` | state.yml | list(text) | runtime timeline state |
| `recent_reactions` | `composer.reactions.recent` | state.yml | list(text) | runtime convenience state |
| `room_drafts` | `composer.drafts.by_room` | state.yml | map(text->text) | unsent composer drafts per room, restored across restarts |
| `collapsed_spaces` | `sidebars.communities.filtering.collapsed_spaces` | state.yml | list(text) | runtime expansion state |
| `presence` | `network.presence.status_policy` | config.yml | enum | account-scoped preference |
| `screen_share_frame_rate` | `calls.screenshare.frame_rate` | config.yml | int | advanced calls/screenshare pref |
| `screen_share_pip` | `calls.screenshare.picture_in_picture` | config.yml | bool | advanced calls/screenshare pref |
| `screen_share_remote_video` | `calls.screenshare.include_remote_video` | config.yml | bool | advanced calls/screenshare pref |
| `screen_share_hide_cursor` | `calls.screenshare.show_cursor` | config.yml | bool | key semantics flipped to positive naming in Komai |
| `disable_certificate_validation` | `network.tls.enable_certificate_validation` | config.yml | bool | inverted from upstream key semantics; positive in Komai |
| `enable_http3` | `network.http3.enabled` | config.yml | bool | network advanced pref |
| `max_db_size` | `db.max_size_bytes` | config.yml | qulonglong | database tuning |
| `max_dbs` | `db.max_stores` | config.yml | uint | database tuning |
| `run_without_secure_secrets_service` | `secrets.provider` | config.yml | bool -> enum | mapped: false=secret_service, true=file; no direct runtime toggle in Komai |
| `secrets` | `secrets` | secret backend (fallback: secrets.yml) | map(text->text) | secret map; do not keep in config.yml |
