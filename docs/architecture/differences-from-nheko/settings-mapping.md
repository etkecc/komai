# Settings Name Mapping (nheko -> Komai)

This reference is for porting patches from upstream nheko into Komai.

Scope:
- Map nheko-style setting names (C++ constants and flat serialization keys) to Komai names.
- Document Komai's current nested YAML keys and target files.
- Keep runtime/action-only rows where useful for patch translation context.

Not in scope:
- Backward-compatibility migration guarantees for old key names.

Source of truth:
- `src/UserSettingsPage.cpp` (`SettingKey::*` constants and load/save code).
- `src/UserSettingsPage.cpp` settings metadata table (`SettingMeta` rows).
- `src/UserSettingsPage.h` (`UserSettingsModel` constants).

Note:
- `UserSettingsModel::ScaleFactor` uses `ui.scale.factor` in profile `config.yml`; startup applies it via
  `QT_SCALE_FACTOR` only when the environment variable is not already set.

## UI Settings Mapping

| Tab | Section | Setting | C++ Constant | nheko Flat Key | Komai YAML Key | Komai Target | Persisted? |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Look & Feel | THEME | Theme | `UserSettingsModel::Theme` | `theme` | `ui.theme.slug` | config.yml | yes |
| Look & Feel | FONTS | Font family | `UserSettingsModel::Font` | `font_family` | `ui.font.family` | config.yml | yes |
| Look & Feel | FONTS | Font size | `UserSettingsModel::FontSize` | `font_size` | `ui.font.size_pt` | config.yml | yes |
| Look & Feel | FONTS | Emoji font family | `UserSettingsModel::EmojiFont` | `emoji_font_family` | `ui.font.emoji_family` | config.yml | yes |
| Look & Feel | FONTS | Scale factor | `UserSettingsModel::ScaleFactor` | `settings/scale_factor` | `ui.scale.factor` | config.yml | yes |
| Look & Feel | BEHAVIOR | Enable UI animations | `UserSettingsModel::EnableUIAnimations` | `reduced_motion` | `ui.motion.enable_animations` | config.yml | yes |
| Sidebars | ROOM LIST | Compact mode | `UserSettingsModel::CompactRoomList` | `compact_room_list` | `sidebars.room_list.compact` | config.yml | yes |
| Sidebars | ROOM LIST | Show last message timestamp | `UserSettingsModel::ShowRoomListTime` | `show_room_list_time` | `sidebars.room_list.show_last_message_timestamp` | config.yml | yes |
| Sidebars | ROOM LIST | Show last message preview | `UserSettingsModel::ShowLastMessagePreview` | `show_last_message_preview` | `sidebars.room_list.last_message_preview` | config.yml | yes |
| Sidebars | ROOM LIST | Show notification counts | `UserSettingsModel::ShowCommunityNotificationCounts` | `show_community_notification_counts` | `sidebars.room_list.show_community_notification_counts` | config.yml | yes |
| Sidebars | ROOM LIST | Use circular avatars | `UserSettingsModel::UseCircularAvatars` | `use_circular_avatars` | `ui.avatars.circular` | config.yml | yes |
| Sidebars | ROOM LIST | Use identicons | `UserSettingsModel::UseIdenticon` | `use_identicon` | `ui.avatars.identicon_fallback` | config.yml | yes |
| Sidebars | ROOM LIST | Show scrollbars | `UserSettingsModel::ScrollbarsInRoomlist` | `scrollbars_in_roomlist` | `sidebars.room_list.scrollbars.visible` | config.yml | yes |
| Sidebars | ROOM LIST | Sorting | `UserSettingsModel::RoomSorting` | `room_sort_order` | `sidebars.room_list.sort` | config.yml | yes |
| Sidebars | COMMUNITIES SIDEBAR | Show communities sidebar | `UserSettingsModel::ShowCommunitiesSidebar` | `show_communities_sidebar` | `sidebars.communities.visible` | config.yml | yes |
| Integrations | SYSTEM TRAY | Minimize to tray | `UserSettingsModel::IntegrationsTray` | `tray` | `app.window.tray.enabled` | config.yml | yes |
| Integrations | SYSTEM TRAY | Start in tray | `UserSettingsModel::IntegrationsStartInTray` | `start_in_tray` | `app.startup.start_in_tray` | config.yml | yes |
| Integrations | D-BUS | Expose room information via D-Bus | `UserSettingsModel::IntegrationsExposeDBusApi` | `expose_dbus_api` | `integrations.dbus.expose_room_info` | config.yml | yes |
| Integrations | BROWSER | Browser open command (Komai-only) | `UserSettingsModel::integrationsLinksBrowserCommand` | `-` | `integrations.browser.command` | config.yml | yes |
| Look & Feel | BEHAVIOR | Enable text selection on timeline | `UserSettingsModel::MobileMode` | `mobile_mode` | `ui.input.enable_text_selection` | config.yml | yes |
| Look & Feel | BEHAVIOR | Enable swipe gestures | `UserSettingsModel::EnableSwipeGestures` | `enable_swipe_gestures` | `ui.input.swipe_gestures` | config.yml | yes |
| Timeline | MESSAGES | Enable message bubbles | `UserSettingsModel::Bubbles` | `bubbles` | `timeline.messages.layout.bubbles` | config.yml | yes |
| Timeline | MESSAGES | Use small avatars | `UserSettingsModel::SmallAvatars` | `small_avatars` | `timeline.messages.layout.small_avatars` | config.yml | yes |
| Timeline | MESSAGES | Show your avatar next to your own messages (bubble layout) | `UserSettingsModel::ShowOwnAvatarInBubbleLayout` | `show_own_avatar_in_bubble_layout` | `timeline.messages.layout.show_own_avatar` | config.yml | yes |
| Timeline | MESSAGES | Show sender username above messages | `UserSettingsModel::ShowSenderUsername` | `show_sender_username` | `timeline.messages.sender_username` | config.yml | yes |
| Timeline | MESSAGES | Limit timeline width | `UserSettingsModel::MaxTimelineWidth` | `max_timeline_width` | `timeline.messages.max_width_px` | config.yml | yes |
| Timeline | MESSAGES | Enlarge emoji-only messages | `UserSettingsModel::EnlargeEmojiOnlyMessages` | `enlarge_emoji_only_messages` | `timeline.messages.emoji_only_enlarge` | config.yml | yes |
| Timeline | MESSAGES | Highlight message on hover | `UserSettingsModel::MessageHoverHighlight` | `message_hover_highlight` | `timeline.messages.hover_highlight` | config.yml | yes |
| Timeline | MESSAGES | Show action buttons | `UserSettingsModel::ShowActionButtons` | `show_action_buttons` | `timeline.messages.actions.enabled` | config.yml | yes |
| Timeline | MESSAGES | Pinned reactions | `UserSettingsModel::PinnedReactions` | `pinned_reactions` | `timeline.messages.actions.pinned_reactions` | config.yml | yes |
| Timeline | MEDIA | Show message effects | `UserSettingsModel::FancyEffects` | `fancy_effects` | `timeline.media.effects_enabled` | config.yml | yes |
| Timeline | MEDIA | Play animated images only on hover | `UserSettingsModel::AnimateImagesOnHover` | `animate_images_on_hover` | `timeline.media.animate_on_hover` | config.yml | yes |
| Timeline | MEDIA | Show images automatically | `UserSettingsModel::ShowImage` | `show_image` | `timeline.media.image_display` | config.yml | yes |
| Timeline | MEDIA | Open images in an external app | `UserSettingsModel::OpenImagesInExternalApp` | `open_images_in_external_app` | `timeline.media.open_images_external` | config.yml | yes |
| Timeline | MEDIA | Open videos in an external app | `UserSettingsModel::OpenVideosInExternalApp` | `open_videos_in_external_app` | `timeline.media.open_videos_external` | config.yml | yes |
| Composer | INPUT | Send messages as <a href="https://commonmark.org/help/">Markdown</a> | `UserSettingsModel::Markdown` | `markdown` | `composer.input.markdown_enabled` | config.yml | yes |
| Composer | INPUT | Send messages with a shortcut | `UserSettingsModel::SendMessageKey` | `send_message_key` | `composer.input.send_key` | config.yml | yes |
| Composer | INPUT | Auto-replace text emoticons with emoji | `UserSettingsModel::AutoReplaceEmoji` | `auto_replace_emoji` | `composer.input.auto_replace_emoji` | config.yml | yes |
| Composer | FEEDBACK | Typing notifications | `UserSettingsModel::TypingNotifications` | `typing_notifications` | `composer.feedback.typing_notifications` | config.yml | yes |
| Composer | FEEDBACK | Read receipts | `UserSettingsModel::ReadReceipts` | `read_receipts` | `composer.feedback.read_receipts` | config.yml | yes |
| Composer | EXTRAS | Enable stickers | `UserSettingsModel::EnableStickers` | `enable_stickers` | `composer.extras.stickers_enabled` | config.yml | yes |
| Notifications | DESKTOP | Desktop notifications | `UserSettingsModel::DesktopNotifications` | `desktop_notifications` | `notifications.desktop.enabled` | config.yml | yes |
| Notifications | DESKTOP | Alert on incoming messages | `UserSettingsModel::AlertOnIncomingMessages` | `alert_on_incoming_messages` | `notifications.desktop.alert_on_incoming` | config.yml | yes |
| Notifications | DESKTOP | Decrypt notifications | `UserSettingsModel::DecryptNotifications` | `decrypt_notifications` | `notifications.desktop.decrypt_messages` | config.yml | yes |
| Calls | GENERAL | Enable legacy calls | `UserSettingsModel::EnableLegacyCalls` | `enable_legacy_calls` | `calls.legacy_enabled` | config.yml | yes |
| Calls | GENERAL | Use fallback call relay server | `UserSettingsModel::UseFallbackCallRelayServer` | `use_fallback_call_relay_server` | `calls.relay.use_fallback_server` | config.yml | yes |
| Calls | DEVICES | Microphone | `UserSettingsModel::Microphone` | `microphone` | `calls.devices.microphone` | config.yml | yes |
| Calls | DEVICES | Camera | `UserSettingsModel::Camera` | `camera` | `calls.devices.camera` | config.yml | yes |
| Calls | DEVICES | Camera resolution | `UserSettingsModel::CameraResolution` | `camera_resolution` | `calls.devices.camera_resolution` | config.yml | yes |
| Calls | DEVICES | Camera frame rate | `UserSettingsModel::CameraFrameRate` | `camera_frame_rate` | `calls.devices.camera_frame_rate` | config.yml | yes |
| Calls | DEVICES | Ringtone | `UserSettingsModel::Ringtone` | `ringtone` | `calls.audio.ringtone` | config.yml | yes |
| Privacy | SCREEN LOCK | Privacy screen | `UserSettingsModel::PrivacyScreen` | `privacy_screen` | `privacy.screen_lock.enabled` | config.yml | yes |
| Privacy | SCREEN LOCK | Privacy screen timeout (seconds) | `UserSettingsModel::PrivacyScreenTimeoutSeconds` | `privacy_screen_timeout_seconds` | `privacy.screen_lock.timeout_seconds` | config.yml | yes |
| Privacy | DATA & MAINTENANCE | Periodically delete expired events | `UserSettingsModel::ExpireEvents` | `expire_events` | `privacy.maintenance.expire_events` | config.yml | yes |
| Privacy | DATA & MAINTENANCE | Hidden events | `UserSettingsModel::HiddenTimelineEvents` | `-` | `privacy.timeline.hidden_events` | runtime/UI-specific | no |
| Privacy | DATA & MAINTENANCE | Periodically update community routing information | `UserSettingsModel::UpdateSpaceVias` | `update_space_vias` | `privacy.maintenance.update_space_vias` | config.yml | yes |
| Privacy | USERS | Ignored users | `UserSettingsModel::IgnoredUsers` | `-` | `privacy.users.ignored` | runtime/UI-specific | no |
| Encryption | KEY SHARING | Send encrypted messages to verified users only | `UserSettingsModel::OnlyShareKeysWithVerifiedUsers` | `only_share_keys_with_verified_users` | `encryption.key_sharing.only_verified_users` | config.yml | yes |
| Encryption | KEY SHARING | Share keys with verified users and devices | `UserSettingsModel::ShareKeysWithTrustedUsers` | `share_keys_with_trusted_users` | `encryption.key_sharing.share_with_trusted` | config.yml | yes |
| Encryption | BACKUP | Online key backup | `UserSettingsModel::UseOnlineKeyBackup` | `use_online_key_backup` | `encryption.backup.online.enabled` | config.yml | yes |
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
| Session | DEVICE | Access token | `UserSettingsModel::AccessToken` | `access_token` | `auth.access_token` | secret backend (fallback: secrets.yml) | yes (secret backend; file fallback only) |
| Session | ACTIONS | Logout | `UserSettingsModel::Logout` | `-` | `session.actions.logout` | action only | no |
| About | APPLICATION | Name | `UserSettingsModel::AppName` | `-` | `about.application.name` | derived/runtime | no |
| About | APPLICATION | Platform | `UserSettingsModel::Platform` | `-` | `about.application.platform` | derived/runtime | no |
| About | APPLICATION | Based on | `UserSettingsModel::BasedOn` | `-` | `about.application.based_on` | derived/runtime | no |
| About | APPLICATION | Maintained by | `UserSettingsModel::MaintainedBy` | `-` | `about.application.maintained_by` | derived/runtime | no |

## Additional Persisted Keys (Not in UI Settings Table)

| nheko Flat Key | Komai YAML Key | Komai Target | Value Type | Notes |
| --- | --- | --- | --- | --- |
| `window_width` | `app.window.size.width` | state.yml | int | runtime window geometry |
| `window_height` | `app.window.size.height` | state.yml | int | runtime window geometry |
| `room_list_width` | `sidebars.room_list.width_px` | state.yml | int | runtime sidebar width |
| `community_list_width` | `sidebars.communities.width_px` | state.yml | int | runtime sidebar width |
| `current_tag_id` | `session.navigation.current_tag_id` | state.yml | text | runtime navigation state |
| `hidden_tags` | `sidebars.communities.hidden_tags` | state.yml | list(text) | runtime visibility state |
| `muted_tags` | `sidebars.communities.muted_tags` | state.yml | list(text) | runtime visibility state |
| `hidden_pins` | `timeline.pins.hidden` | state.yml | list(text) | runtime timeline state |
| `hidden_widgets` | `timeline.widgets.hidden` | state.yml | list(text) | runtime timeline state |
| `recent_reactions` | `composer.reactions.recent` | state.yml | list(text) | runtime convenience state |
| `collapsed_spaces` | `sidebars.communities.collapsed_spaces` | state.yml | list(list(text)) | runtime expansion state |
| `presence` | `network.presence.default` | config.yml | enum | account-scoped preference |
| `screen_share_frame_rate` | `calls.screenshare.frame_rate` | config.yml | int | advanced calls/screenshare pref |
| `screen_share_pip` | `calls.screenshare.picture_in_picture` | config.yml | bool | advanced calls/screenshare pref |
| `screen_share_remote_video` | `calls.screenshare.include_remote_video` | config.yml | bool | advanced calls/screenshare pref |
| `screen_share_hide_cursor` | `calls.screenshare.hide_cursor` | config.yml | bool | advanced calls/screenshare pref |
| `disable_certificate_validation` | `network.tls.disable_certificate_validation` | config.yml | bool | network/security advanced pref |
| `enable_http3` | `network.http3.enabled` | config.yml | bool | network advanced pref |
| `max_db_size` | `db.max_size_bytes` | config.yml | qulonglong | database tuning |
| `max_dbs` | `db.max_files` | config.yml | uint | database tuning |
| `run_without_secure_secrets_service` | `secrets.provider` | config.yml | bool -> enum | mapped: false=secret_service, true=file |
| `secrets` | `secrets` | secret backend (fallback: secrets.yml) | map(text->text) | secret map; do not keep in config.yml |
