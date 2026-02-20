# Application Settings Inventory

Source of truth: `src/UserSettingsPage.h` and `src/UserSettingsPage.cpp`.

This inventory reflects the current persistence implementation:

- Profile layout: `~/.config/komai/profiles/<profile-id>/{config.yml,state.yml,session.yml,secrets.yml}`
- Default profile id: `default`
- Secret source:
  - `secrets.provider=secret_service` -> secure backend (QtKeychain)
  - `secrets.provider=file` -> `secrets.yml` (`auth.access_token`, `secrets`)
- Secure backend key naming:
  - `komai.<profile_hash>.settings.<key>`
  - `komai.<profile_hash>.local_crypto.<key>`
  - `komai.<profile_hash>.matrix.<key>`

## UI Settings Mapping

| Tab | Setting | C++ Constant | Stored Value Type | YAML Key | Target |
| --- | --- | --- | --- | --- | --- |
| Look & Feel | Theme | `UserSettingsModel::Theme` | enum/choice | `ui.theme.slug` | config.yml |
| Look & Feel | Font family | `UserSettingsModel::Font` | enum/choice | `ui.font.family` | config.yml |
| Look & Feel | Font size | `UserSettingsModel::FontSize` | double | `ui.font.size_pt` | config.yml |
| Look & Feel | Emoji font family | `UserSettingsModel::EmojiFont` | enum/choice | `ui.font.emoji_family` | config.yml |
| Look & Feel | Scale factor | `UserSettingsModel::ScaleFactor` | double | `ui.scale.factor` | optional/not currently persisted |
| Look & Feel | Reduce or disable animations | `UserSettingsModel::ReducedMotion` | bool | `ui.motion.reduced` | config.yml |
| Sidebars | Compact mode | `UserSettingsModel::CompactRoomList` | bool | `sidebars.room_list.compact` | config.yml |
| Sidebars | Show last message timestamp | `UserSettingsModel::ShowRoomListTime` | bool | `sidebars.room_list.show_last_message_timestamp` | config.yml |
| Sidebars | Show last message preview | `UserSettingsModel::ShowLastMessagePreview` | enum/choice | `sidebars.room_list.last_message_preview` | config.yml |
| Sidebars | Show notification counts | `UserSettingsModel::ShowCommunityNotificationCounts` | bool | `sidebars.room_list.show_community_notification_counts` | config.yml |
| Sidebars | Use circular avatars | `UserSettingsModel::UseCircularAvatars` | bool | `ui.avatars.circular` | config.yml |
| Sidebars | Use identicons | `UserSettingsModel::UseIdenticon` | bool | `ui.avatars.identicon_fallback` | config.yml |
| Sidebars | Show scrollbars | `UserSettingsModel::ScrollbarsInRoomlist` | bool | `sidebars.room_list.scrollbars_visible` | config.yml |
| Sidebars | Sorting | `UserSettingsModel::RoomSorting` | enum/choice | `sidebars.room_list.sort` | config.yml |
| Sidebars | Show communities sidebar | `UserSettingsModel::ShowCommunitiesSidebar` | bool | `sidebars.communities.visible` | config.yml |
| Look & Feel | Minimize to tray | `UserSettingsModel::Tray` | bool | `app.window.tray.enabled` | config.yml |
| Look & Feel | Start in tray | `UserSettingsModel::StartInTray` | bool | `app.startup.start_in_tray` | config.yml |
| Look & Feel | Expose room information via D-Bus | `UserSettingsModel::ExposeDBusApi` | bool | `integrations.dbus.expose_room_info` | config.yml |
| Look & Feel | Touchscreen mode | `UserSettingsModel::MobileMode` | bool | `ui.input.touchscreen_mode` | config.yml |
| Look & Feel | Enable swipe gestures | `UserSettingsModel::EnableSwipeGestures` | bool | `ui.input.swipe_gestures` | config.yml |
| Timeline | Enable message bubbles | `UserSettingsModel::Bubbles` | bool | `timeline.messages.layout.bubbles` | config.yml |
| Timeline | Use small avatars | `UserSettingsModel::SmallAvatars` | bool | `timeline.messages.layout.small_avatars` | config.yml |
| Timeline | Show own avatar in bubble layout | `UserSettingsModel::ShowOwnAvatarInBubbleLayout` | bool | `timeline.messages.layout.show_own_avatar` | config.yml |
| Timeline | Show sender username above messages | `UserSettingsModel::ShowSenderUsername` | enum/choice | `timeline.messages.sender_username` | config.yml |
| Timeline | Limit timeline width | `UserSettingsModel::MaxTimelineWidth` | int | `timeline.messages.max_width_px` | config.yml |
| Timeline | Enlarge emoji-only messages | `UserSettingsModel::EnlargeEmojiOnlyMessages` | bool | `timeline.messages.emoji_only_enlarge` | config.yml |
| Timeline | Highlight message on hover | `UserSettingsModel::MessageHoverHighlight` | bool | `timeline.messages.hover_highlight` | config.yml |
| Timeline | Show action buttons | `UserSettingsModel::ShowActionButtons` | bool | `timeline.messages.actions_visible` | config.yml |
| Timeline | Show message effects | `UserSettingsModel::FancyEffects` | bool | `timeline.media.effects_enabled` | config.yml |
| Timeline | Play animated images only on hover | `UserSettingsModel::AnimateImagesOnHover` | bool | `timeline.media.animate_on_hover` | config.yml |
| Timeline | Show images automatically | `UserSettingsModel::ShowImage` | enum/choice | `timeline.media.image_display` | config.yml |
| Timeline | Open images in external app | `UserSettingsModel::OpenImagesInExternalApp` | bool | `timeline.media.open_images_external` | config.yml |
| Timeline | Open videos in external app | `UserSettingsModel::OpenVideosInExternalApp` | bool | `timeline.media.open_videos_external` | config.yml |
| Composer | Send messages as Markdown | `UserSettingsModel::Markdown` | bool | `composer.input.markdown_enabled` | config.yml |
| Composer | Send messages with a shortcut | `UserSettingsModel::SendMessageKey` | enum/choice | `composer.input.send_key` | config.yml |
| Composer | Auto-replace text emoticons with emoji | `UserSettingsModel::AutoReplaceEmoji` | enum/choice | `composer.input.auto_replace_emoji` | config.yml |
| Composer | Typing notifications | `UserSettingsModel::TypingNotifications` | bool | `composer.feedback.typing_notifications` | config.yml |
| Composer | Read receipts | `UserSettingsModel::ReadReceipts` | bool | `composer.feedback.read_receipts` | config.yml |
| Composer | Pinned reactions | `UserSettingsModel::PinnedReactions` | text | `composer.extras.pinned_reactions` | config.yml |
| Composer | Enable stickers | `UserSettingsModel::EnableStickers` | bool | `composer.extras.stickers_enabled` | config.yml |
| Notifications | Desktop notifications | `UserSettingsModel::DesktopNotifications` | bool | `notifications.desktop.enabled` | config.yml |
| Notifications | Alert on incoming messages | `UserSettingsModel::AlertOnIncomingMessages` | bool | `notifications.desktop.alert_on_incoming` | config.yml |
| Notifications | Decrypt notifications | `UserSettingsModel::DecryptNotifications` | bool | `notifications.desktop.decrypt_messages` | config.yml |
| Calls | Enable legacy calls | `UserSettingsModel::EnableLegacyCalls` | bool | `calls.legacy_enabled` | config.yml |
| Calls | Use fallback call relay server | `UserSettingsModel::UseFallbackCallRelayServer` | bool | `calls.relay.use_fallback_server` | config.yml |
| Calls | Microphone | `UserSettingsModel::Microphone` | text | `calls.devices.microphone` | config.yml |
| Calls | Camera | `UserSettingsModel::Camera` | text | `calls.devices.camera` | config.yml |
| Calls | Camera resolution | `UserSettingsModel::CameraResolution` | text | `calls.devices.camera_resolution` | config.yml |
| Calls | Camera frame rate | `UserSettingsModel::CameraFrameRate` | text | `calls.devices.camera_frame_rate` | config.yml |
| Calls | Ringtone | `UserSettingsModel::Ringtone` | text | `calls.audio.ringtone` | config.yml |
| Privacy | Privacy screen | `UserSettingsModel::PrivacyScreen` | bool | `privacy.screen_lock.enabled` | config.yml |
| Privacy | Privacy screen timeout (seconds) | `UserSettingsModel::PrivacyScreenTimeoutSeconds` | int | `privacy.screen_lock.timeout_seconds` | config.yml |
| Privacy | Periodically delete expired events | `UserSettingsModel::ExpireEvents` | bool | `privacy.maintenance.expire_events` | config.yml |
| Privacy | Periodically update community routing information | `UserSettingsModel::UpdateSpaceVias` | bool | `privacy.maintenance.update_space_vias` | config.yml |
| Encryption | Send encrypted messages to verified users only | `UserSettingsModel::OnlyShareKeysWithVerifiedUsers` | bool | `encryption.key_sharing.only_verified_users` | config.yml |
| Encryption | Share keys with verified users and devices | `UserSettingsModel::ShareKeysWithTrustedUsers` | bool | `encryption.key_sharing.share_with_trusted` | config.yml |
| Encryption | Online key backup | `UserSettingsModel::UseOnlineKeyBackup` | bool | `encryption.backup.online.enabled` | config.yml |
| Session | User ID | `UserSettingsModel::UserId` | text | `session.account.user_id` | session.yml |
| Session | Homeserver | `UserSettingsModel::Homeserver` | text | `session.account.homeserver` | session.yml |
| Session | Device ID | `UserSettingsModel::DeviceId` | text | `session.device.id` | session.yml |
| Session | Access token | `UserSettingsModel::AccessToken` | secret text | `auth.access_token` | secure backend, file fallback in secrets.yml |

## Additional Persisted Variables (Outside UI Table)

| C++ Member | Key | Target | Value Type |
| --- | --- | --- | --- |
| `windowWidth_` | `app.window.size.width` | state.yml | int |
| `windowHeight_` | `app.window.size.height` | state.yml | int |
| `roomListWidth_` | `sidebars.room_list.width_px` | state.yml | int |
| `communityListWidth_` | `sidebars.communities.width_px` | state.yml | int |
| `currentTagId_` | `session.navigation.current_tag_id` | state.yml | text |
| `hiddenTags_` | `sidebars.communities.hidden_tags` | state.yml | list(text) |
| `mutedTags_` | `sidebars.communities.muted_tags` | state.yml | list(text) |
| `collapsedSpaces_` | `sidebars.communities.collapsed_spaces` | state.yml | list(list(text)) |
| `hiddenPins_` | `timeline.pins.hidden` | state.yml | list(text) |
| `hiddenWidgets_` | `timeline.widgets.hidden` | state.yml | list(text) |
| `recentReactions_` | `composer.reactions.recent` | state.yml | list(text) |
| `presence_` | `session.presence.default` | session.yml | enum |
| `screenShareFrameRate_` | `calls.screenshare.frame_rate` | config.yml | int |
| `screenSharePiP_` | `calls.screenshare.picture_in_picture` | config.yml | bool |
| `screenShareRemoteVideo_` | `calls.screenshare.include_remote_video` | config.yml | bool |
| `screenShareHideCursor_` | `calls.screenshare.hide_cursor` | config.yml | bool |
| `disableCertificateValidation_` | `network.tls.disable_certificate_validation` | config.yml | bool |
| `enableHttp3_` | `network.http3.enabled` | config.yml | bool |
| `maxDbSize_` | `db.max_size_bytes` | config.yml | qulonglong |
| `maxDbs_` | `db.max_files` | config.yml | uint |
| `runWithoutSecureSecretsService_` | `secrets.provider` | config.yml | mapped bool (`false=secret_service`, `true=file`) |
| `secrets_` | `secrets` | secure backend, file fallback in secrets.yml | map(text->text) |

## Explicit Security Invariants

- `access_token` and `secrets` are never written to `config.yml`.
- `access_token` and `secrets` are never written to `state.yml`.
- `access_token` and `secrets` are never written to `session.yml`.
- In `secret_service` mode, token and secret map are stored in secure backend.
- In `file` mode, token and secret map are stored in `secrets.yml`.
- `secrets.yml` is written with owner read/write permissions only.

## Secure Backend Namespaces

| Namespace | Pattern | Producer | Example Keys |
| --- | --- | --- | --- |
| Settings secrets | `komai.<profile_hash>.settings.<key>` | `UserSettings` | `session.auth.access_token`, `session.secrets` |
| Local crypto secrets | `komai.<profile_hash>.local_crypto.<key>` | `Cache` | `pickle_secret` |
| Matrix secrets | `komai.<profile_hash>.matrix.<key>` | `Cache` | Matrix secret names (cross-signing/backup paths) |

Notes:
- `profile_hash = hex(sha256(normalized_profile_id))`
- `normalized_profile_id`: empty/default -> `default`, otherwise profile id
- default profile hash convenience value: `37a8eec1ce19687d132fe29051dca629d164e2c4958ba141d5f4133a33f0688f`
- Legacy base64-profile-hash secret IDs are intentionally not used.

## File Provider Storage Layout

When `secrets.provider=file`, `secrets.yml` is the fallback secret store:

- `auth.access_token`: plain token value.
- `secrets`: map of secret-store IDs -> secret value.
  - IDs use the same namespaced patterns as secure backend mode:
    - `komai.<profile_hash>.settings.<key>`
    - `komai.<profile_hash>.local_crypto.<key>`
    - `komai.<profile_hash>.matrix.<key>`
