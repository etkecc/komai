// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pub(crate) use crate::logging::{init_logging, log_from_cpp};
pub(crate) use crate::matrix_backend::ffi::*;
pub(crate) use crate::settings::ffi::*;
pub(crate) use crate::settings::profile::SettingsProfileHandle;
pub(crate) use crate::theme::base16::parse_base16_yaml as theme_parse_base16_yaml;
pub(crate) use crate::theme::external::parse_external_theme as theme_parse_external_theme;

#[cxx::bridge(namespace = "komai::rust")]
mod bridge {
    enum MatrixFfiBlockingThreadPolicy {
        AllowUiThread,
        RequireWorkerThread,
    }

    enum MatrixFfiCallerThread {
        AppUiThread,
        WorkerThread,
    }

    struct MatrixFfiBlockingContext {
        thread_policy: MatrixFfiBlockingThreadPolicy,
        caller_thread: MatrixFfiCallerThread,
    }

    struct ResolveResult {
        base_url: String,
    }

    struct MatrixSdkPaths {
        profile_data_root: String,
        profile_cache_root: String,
        matrix_data_root: String,
        matrix_cache_root: String,
        state_store_root: String,
        cache_root: String,
        event_cache_root: String,
        media_cache_root: String,
    }

    struct MatrixRestorePreview {
        has_session: bool,
        session_source: String,
        auth_type: String,
        homeserver_url: String,
        user_id: String,
        device_id: String,
        state_store_root: String,
        cache_root: String,
    }

    struct SettingsStartupSnapshot {
        has_ui_scale_factor: bool,
        ui_scale_factor: f32,
    }

    struct SettingsConfigOverview {
        has_ui_scale_factor: bool,
        ui_scale_factor: f32,
        theme_slug: String,
        secrets_provider: String,
    }

    #[derive(Debug, PartialEq, Eq)]
    struct SettingsStringMapEntry {
        key: String,
        value: String,
    }

    #[derive(Debug, PartialEq, Eq)]
    struct SettingsStringListMapEntry {
        key: String,
        values: Vec<String>,
    }

    struct SettingsConfigUiSection {
        has_scale_factor: bool,
        scale_factor: f32,
        theme_slug: String,
        has_font_size_pt: bool,
        font_size_pt: f64,
        font_family: String,
        font_emoji_family: String,
        has_motion_animations_enabled: bool,
        motion_animations_enabled: bool,
        input_mode: String,
        has_input_touch_swipe_gestures_enabled: bool,
        input_touch_swipe_gestures_enabled: bool,
        has_layout_content_max_width_px: bool,
        layout_content_max_width_px: i32,
        has_layout_compact_mode: bool,
        layout_compact_mode: bool,
        has_avatars_circular: bool,
        avatars_circular: bool,
        scrollbar_policy: String,
        default_avatar_style: String,
    }

    struct SettingsConfigSidebarsRoomListSection {
        has_show_last_message_time: bool,
        show_last_message_time: bool,
        last_message_preview: String,
        has_show_community_counts: bool,
        show_community_counts: bool,
        sort: String,
        unread_detection_policy: String,
    }

    struct SettingsConfigSidebarsCommunitiesSection {
        has_visible: bool,
        visible: bool,
        has_filter_favourites: bool,
        filter_favourites: bool,
        has_filter_people: bool,
        filter_people: bool,
        has_filter_bots: bool,
        filter_bots: bool,
        has_filter_groups: bool,
        filter_groups: bool,
        has_filter_server_notices: bool,
        filter_server_notices: bool,
        has_filter_low_priority: bool,
        filter_low_priority: bool,
    }

    struct SettingsConfigSidebarsSection {
        room_list: SettingsConfigSidebarsRoomListSection,
        communities: SettingsConfigSidebarsCommunitiesSection,
    }

    struct SettingsConfigTimelineHiddenEventsSection {
        has_global: bool,
        global: Vec<String>,
        by_room: Vec<SettingsStringListMapEntry>,
    }

    struct SettingsConfigTimelineMessagesSection {
        style: String,
        positioning: String,
        user_color_coding_policy: String,
        has_layout_small_avatars: bool,
        layout_small_avatars: bool,
        has_layout_show_own_avatar: bool,
        layout_show_own_avatar: bool,
        sender_username: String,
        has_emoji_only_enlarge: bool,
        emoji_only_enlarge: bool,
        has_hover_highlight: bool,
        hover_highlight: bool,
    }

    struct SettingsConfigTimelineFormattedSection {
        has_code_syntax_highlighting: bool,
        code_syntax_highlighting: bool,
    }

    struct SettingsConfigTimelineTypingSection {
        has_show_enabled: bool,
        show_enabled: bool,
    }

    struct SettingsConfigTimelineReadReceiptsSection {
        has_enabled: bool,
        enabled: bool,
    }

    struct SettingsConfigTimelineMessageActionsSection {
        activation_policy: String,
        pinned_reactions: String,
    }

    struct SettingsConfigTimelineMediaSection {
        has_effects_enabled: bool,
        effects_enabled: bool,
        has_animate_on_hover: bool,
        animate_on_hover: bool,
        image_display: String,
        has_open_images_external: bool,
        open_images_external: bool,
        has_open_videos_external: bool,
        open_videos_external: bool,
        has_autoplay_gif_videos: bool,
        autoplay_gif_videos: bool,
        has_open_audio_external: bool,
        open_audio_external: bool,
        has_default_audio_playback_speed: bool,
        default_audio_playback_speed: f64,
    }

    struct SettingsConfigTimelineSection {
        messages: SettingsConfigTimelineMessagesSection,
        formatted: SettingsConfigTimelineFormattedSection,
        typing: SettingsConfigTimelineTypingSection,
        read_receipts: SettingsConfigTimelineReadReceiptsSection,
        message_actions: SettingsConfigTimelineMessageActionsSection,
        media: SettingsConfigTimelineMediaSection,
        hidden_events: SettingsConfigTimelineHiddenEventsSection,
    }

    struct SettingsConfigSecretsSection {
        provider: String,
    }

    struct SettingsConfigPrivacyWindowFocusBlurSection {
        has_enabled: bool,
        enabled: bool,
        has_delay_seconds: bool,
        delay_seconds: i32,
    }

    struct SettingsConfigPrivacyMaintenanceSection {
        has_expire_events: bool,
        expire_events: bool,
    }

    struct SettingsConfigPrivacySection {
        window_focus_blur: SettingsConfigPrivacyWindowFocusBlurSection,
        maintenance: SettingsConfigPrivacyMaintenanceSection,
    }

    struct SettingsConfigEncryptionKeySharingSection {
        has_only_verified_users: bool,
        only_verified_users: bool,
        has_share_with_trusted: bool,
        share_with_trusted: bool,
    }

    struct SettingsConfigEncryptionBackupOnlineSection {
        has_enabled: bool,
        enabled: bool,
    }

    struct SettingsConfigEncryptionBackupSection {
        online: SettingsConfigEncryptionBackupOnlineSection,
    }

    struct SettingsConfigEncryptionSection {
        key_sharing: SettingsConfigEncryptionKeySharingSection,
        backup: SettingsConfigEncryptionBackupSection,
    }

    struct SettingsConfigCallsLegacySection {
        has_enabled: bool,
        enabled: bool,
    }

    struct SettingsConfigCallsRelaySection {
        has_use_fallback_server: bool,
        use_fallback_server: bool,
    }

    struct SettingsConfigCallsDevicesSection {
        microphone: String,
        camera: String,
        camera_resolution: String,
        camera_frame_rate: String,
    }

    struct SettingsConfigCallsAudioSection {
        ringtone: String,
    }

    struct SettingsConfigCallsScreenshareSection {
        has_frame_rate: bool,
        frame_rate: i32,
        has_picture_in_picture: bool,
        picture_in_picture: bool,
        has_include_remote_video: bool,
        include_remote_video: bool,
        has_show_cursor: bool,
        show_cursor: bool,
    }

    struct SettingsConfigCallsSection {
        legacy: SettingsConfigCallsLegacySection,
        relay: SettingsConfigCallsRelaySection,
        devices: SettingsConfigCallsDevicesSection,
        audio: SettingsConfigCallsAudioSection,
        screenshare: SettingsConfigCallsScreenshareSection,
    }

    struct SettingsConfigNotificationsSection {
        has_enabled: bool,
        enabled: bool,
        has_attention_on_incoming: bool,
        attention_on_incoming: bool,
        message_content_policy: String,
    }

    struct SettingsConfigNetworkSection {
        presence_status_policy: String,
        has_tls_enable_certificate_validation: bool,
        tls_enable_certificate_validation: bool,
        has_mrs_enabled: bool,
        mrs_enabled: bool,
        mrs_server_name: String,
        has_http3_enabled: bool,
        http3_enabled: bool,
    }

    struct SettingsConfigIntegrationsSection {
        has_system_tray_enabled: bool,
        system_tray_enabled: bool,
        has_system_tray_autostart: bool,
        system_tray_autostart: bool,
        dbus_api_access: String,
        browser_command: String,
    }

    struct SettingsConfigComposerSection {
        has_input_markdown_to_html_enabled: bool,
        input_markdown_to_html_enabled: bool,
        input_send_key: String,
        input_auto_replace_emoji: String,
        input_emoji_preferred_gender: String,
        input_emoji_preferred_skin_tone: String,
        has_input_inline_emoji_picker_enabled: bool,
        input_inline_emoji_picker_enabled: bool,
        has_input_inline_room_picker_enabled: bool,
        input_inline_room_picker_enabled: bool,
        has_input_inline_user_picker_enabled: bool,
        input_inline_user_picker_enabled: bool,
        has_typing_send_enabled: bool,
        typing_send_enabled: bool,
        has_extras_stickers_enabled: bool,
        extras_stickers_enabled: bool,
    }

    struct SettingsConfigSnapshot {
        ui: SettingsConfigUiSection,
        sidebars: SettingsConfigSidebarsSection,
        timeline: SettingsConfigTimelineSection,
        secrets: SettingsConfigSecretsSection,
        privacy: SettingsConfigPrivacySection,
        encryption: SettingsConfigEncryptionSection,
        calls: SettingsConfigCallsSection,
        notifications: SettingsConfigNotificationsSection,
        network: SettingsConfigNetworkSection,
        integrations: SettingsConfigIntegrationsSection,
        composer: SettingsConfigComposerSection,
    }

    struct SettingsLoadedConfig {
        ui: SettingsConfigUiSection,
        sidebars: SettingsConfigSidebarsSection,
        timeline: SettingsConfigTimelineSection,
        secrets: SettingsConfigSecretsSection,
        privacy: SettingsConfigPrivacySection,
        encryption: SettingsConfigEncryptionSection,
        calls: SettingsConfigCallsSection,
        notifications: SettingsConfigNotificationsSection,
        network: SettingsConfigNetworkSection,
        integrations: SettingsConfigIntegrationsSection,
        composer: SettingsConfigComposerSection,
        source_version: i32,
        migrated_version: i32,
        had_future_version: bool,
        had_unsupported_path: bool,
        should_write_back: bool,
        serialized_yaml: String,
    }

    struct SettingsLoadedSession {
        user_id: String,
        device_id: String,
        homeserver: String,
        source_version: i32,
        migrated_version: i32,
        had_future_version: bool,
        had_unsupported_path: bool,
        should_write_back: bool,
        serialized_yaml: String,
    }

    struct SettingsLoadedState {
        window_width: i32,
        window_height: i32,
        sidebars_room_list_width_px: i32,
        sidebars_communities_width_px: i32,
        current_filter_id: String,
        current_room_id: String,
        global_excludes: Vec<String>,
        badges_hidden_filters: Vec<String>,
        hidden_pins: Vec<String>,
        hidden_widgets: Vec<String>,
        collapsed_spaces: Vec<String>,
        composer_drafts_by_room: Vec<SettingsStringMapEntry>,
        source_version: i32,
        migrated_version: i32,
        had_future_version: bool,
        had_unsupported_path: bool,
        should_write_back: bool,
        serialized_yaml: String,
    }

    struct SettingsLoadedProfile {
        config: SettingsLoadedConfig,
        session: SettingsLoadedSession,
        state: SettingsLoadedState,
    }

    struct SettingsStateSnapshot {
        window_width: i32,
        window_height: i32,
        sidebars_room_list_width_px: i32,
        sidebars_communities_width_px: i32,
        current_filter_id: String,
        current_room_id: String,
        global_excludes: Vec<String>,
        badges_hidden_filters: Vec<String>,
        hidden_pins: Vec<String>,
        hidden_widgets: Vec<String>,
        collapsed_spaces: Vec<String>,
        composer_drafts_by_room: Vec<SettingsStringMapEntry>,
    }

    struct ThemeUserColorSlotData {
        background: String,
        text: String,
        secondary_text: String,
        link: String,
    }

    struct ThemePaletteData {
        window: String,
        window_text: String,
        base: String,
        alternate_base: String,
        text: String,
        bright_text: String,
        button: String,
        button_text: String,
        light: String,
        mid: String,
        dark: String,
        highlight: String,
        highlighted_text: String,
        link: String,
        tool_tip_base: String,
        tool_tip_text: String,
        attention: String,
        success: String,
        warning: String,
        error: String,
    }

    struct ThemeExternalDefinition {
        name: String,
        variant: String,
        palette: ThemePaletteData,
        user_color_self: ThemeUserColorSlotData,
        user_color_others: Vec<ThemeUserColorSlotData>,
    }

    struct ThemeExternalParseResult {
        has_theme: bool,
        error_message: String,
        theme: ThemeExternalDefinition,
    }

    struct ThemeBase16PaletteData {
        base00: String,
        base01: String,
        base02: String,
        base03: String,
        base04: String,
        base05: String,
        base06: String,
        base07: String,
        base08: String,
        base09: String,
        base0a: String,
        base0b: String,
        base0c: String,
        base0d: String,
        base0e: String,
        base0f: String,
    }

    struct ThemeBase16ParseResult {
        has_document: bool,
        error_message: String,
        name: String,
        author: String,
        palette: ThemeBase16PaletteData,
    }

    struct MatrixBackendHandleInfo {
        handle_id: u64,
        has_session: bool,
        auth_type: String,
        homeserver_url: String,
        user_id: String,
        device_id: String,
    }

    struct MatrixOwnProfile {
        display_name: String,
        avatar_url: String,
    }

    struct MatrixOwnPresence {
        state: String,
        status_message: String,
    }

    struct MatrixTurnServerInfo {
        username: String,
        password: String,
        uris: Vec<String>,
        ttl_seconds: u64,
    }

    struct MatrixRecoveryStatus {
        state: String,
        has_devices_to_verify_against: bool,
        own_device_is_verified: bool,
        has_unverified_own_devices: bool,
    }

    struct MatrixSetupRecoveryResult {
        recovery_key: String,
    }

    struct MatrixResetEncryptionIdentityResult {
        completed: bool,
        auth_type: String,
        approval_url: String,
    }

    struct MatrixDeviceSignOutResult {
        completed: bool,
        auth_type: String,
        approval_url: String,
    }

    struct MatrixVerificationSession {
        flow_id: String,
        user_id: String,
        device_id: String,
        state: String,
        error: String,
        sender: bool,
        is_self_verification: bool,
        is_multi_device_verification: bool,
        sas_numbers: Vec<u16>,
    }

    struct MatrixUserDevice {
        device_id: String,
        display_name: String,
        verification_state: String,
        last_seen_ip: String,
        last_seen_ts: u64,
    }

    struct MatrixUserVerificationState {
        has_master_key: bool,
        user_trust: String,
        devices: Vec<MatrixUserDevice>,
    }

    struct MatrixUserProfile {
        display_name: String,
        avatar_url: String,
    }

    struct MatrixDirectoryUser {
        display_name: String,
        user_id: String,
        avatar_url: String,
    }

    struct MatrixPublicRoomDirectoryEntry {
        room_id: String,
        room_server_name: String,
        display_name: String,
        avatar_url: String,
        topic: String,
        canonical_alias: String,
        member_count: u64,
        is_world_readable: bool,
        is_space: bool,
    }

    struct MatrixPublicRoomDirectoryPage {
        rooms: Vec<MatrixPublicRoomDirectoryEntry>,
        next_batch: String,
        total_room_count_estimate: i32,
    }

    struct MatrixRoomSummary {
        room_id: String,
        latest_event_id: String,
        display_name: String,
        avatar_url: String,
        topic: String,
        last_message: String,
        last_message_kind: String,
        tags: Vec<String>,
        parent_space_room_ids: Vec<String>,
        direct_chat_other_user_id: String,
        is_invite: bool,
        is_space: bool,
        is_direct: bool,
        is_bot_room: bool,
        is_encrypted: bool,
        is_public: bool,
        member_count: u64,
        unread_message_count: u64,
        notification_count: u64,
        highlight_count: u64,
        timestamp: u64,
    }

    struct MatrixNotificationRequest {
        room_id: String,
        event_id: String,
    }

    struct MatrixNotificationItem {
        room_id: String,
        event_id: String,
        replacement_event_id: String,
        room_name: String,
        avatar_url: String,
        sender_display_name: String,
        plain_body: String,
        formatted_body: String,
        media_mxc_url: String,
        is_reply: bool,
        is_emote: bool,
        is_encrypted: bool,
        contains_spoiler: bool,
        has_inline_image: bool,
        play_sound: bool,
    }

    struct MatrixImagePackImage {
        shortcode: String,
        body: String,
        url: String,
        is_emote: bool,
        is_sticker: bool,
    }

    struct MatrixImagePack {
        source_room_id: String,
        state_key: String,
        display_name: String,
        avatar_url: String,
        attribution: String,
        is_emote_pack: bool,
        is_sticker_pack: bool,
        from_space: bool,
        is_globally_enabled: bool,
        images: Vec<MatrixImagePackImage>,
    }

    struct MatrixRoomSettings {
        room_id: String,
        room_name: String,
        room_topic: String,
        room_avatar_url: String,
        room_version: String,
        member_count: u64,
        notifications: i32,
        join_rule: String,
        history_visibility: String,
        allowed_room_ids: Vec<String>,
        parent_space_room_ids: Vec<String>,
        guest_access: bool,
        is_encrypted: bool,
        can_change_name: bool,
        can_change_topic: bool,
        can_change_avatar: bool,
        can_change_join_rules: bool,
        can_change_history_visibility: bool,
    }

    struct MatrixRoomAliases {
        canonical_alias: String,
        alt_aliases: Vec<String>,
        published_aliases: Vec<String>,
    }

    struct MatrixRoomMember {
        user_id: String,
        display_name: String,
        avatar_url: String,
        power_level: i64,
    }

    struct MatrixPowerLevelEntry {
        key: String,
        level: i64,
    }

    struct MatrixRoomPowerLevels {
        room_version: String,
        creators: Vec<String>,
        events: Vec<MatrixPowerLevelEntry>,
        users: Vec<MatrixPowerLevelEntry>,
        ban: i64,
        events_default: i64,
        invite: i64,
        kick: i64,
        redact: i64,
        state_default: i64,
        users_default: i64,
    }

    struct MatrixRoomRedactionPermissions {
        can_redact_own: bool,
        can_redact_other: bool,
    }

    struct MatrixRawEventDialogData {
        pretty_json: String,
        body: String,
        formatted_body: String,
    }

    struct MatrixEventContentForForwarding {
        event_type: String,
        content_json: String,
    }

    struct MatrixCallSessionDescription {
        sdp: String,
        /// "offer" or "answer"
        sdp_type: String,
    }

    struct MatrixCallIceCandidate {
        sdp_mid: String,
        sdp_m_line_index: u16,
        candidate: String,
    }

    struct MatrixCallInviteEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        version: String,
        lifetime: u32,
        invitee: String,
        offer: MatrixCallSessionDescription,
    }

    struct MatrixCallCandidatesEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        version: String,
        candidates: Vec<MatrixCallIceCandidate>,
    }

    struct MatrixCallAnswerEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        version: String,
        answer: MatrixCallSessionDescription,
    }

    struct MatrixCallHangUpEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        version: String,
        /// One of: "ice_failed", "invite_timeout", "ice_timeout",
        /// "user_hangup", "user_media_failed", "user_busy",
        /// "unknown_error", "user", or "" for default (UserHangUp)
        reason: String,
    }

    struct MatrixCallSelectAnswerEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        version: String,
        selected_party_id: String,
    }

    struct MatrixCallRejectEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        version: String,
    }

    struct MatrixCallNegotiateEvent {
        room_id: String,
        sender_id: String,
        event_id: String,
        call_id: String,
        party_id: String,
        lifetime: u32,
        description: MatrixCallSessionDescription,
    }

    struct MatrixReadReceiptEntry {
        user_id: String,
        display_name: String,
        avatar_url: String,
        timestamp: u64,
    }

    struct MatrixTimelineItem {
        item_id: String,
        event_id: String,
        delivery_state: String,
        thread_id: String,
        sender_id: String,
        sender_display_name: String,
        sender_avatar_url: String,
        body: String,
        formatted_body: String,
        reply_event_id: String,
        reply_sender_id: String,
        reply_sender_display_name: String,
        reply_item_kind: String,
        reply_matrix_event_type: String,
        reply_body: String,
        reply_formatted_body: String,
        reply_media_url: String,
        reply_thumbnail_url: String,
        reply_file_name: String,
        reply_mime_type: String,
        reply_media_width: u64,
        reply_media_height: u64,
        reply_media_duration_ms: u64,
        reply_media_size_bytes: u64,
        reactions: Vec<MatrixReactionSummary>,
        reactions_summary: String,
        special_effect_names: Vec<String>,
        item_kind: String,
        matrix_event_type: String,
        is_edited: bool,
        media_url: String,
        thumbnail_url: String,
        file_name: String,
        mime_type: String,
        media_width: u64,
        media_height: u64,
        media_duration_ms: u64,
        media_size_bytes: u64,
        media_is_encrypted: bool,
        thumbnail_is_encrypted: bool,
        timestamp: u64,
        is_own: bool,
    }

    struct MatrixReactionSummary {
        key: String,
        users: String,
        self_reacted_event: String,
        count: u64,
    }

    struct MatrixJoinRoomResult {
        ok: bool,
        room_id: String,
        error: String,
        matrix_errcode: String,
    }

    struct MatrixLoginResult {
        user_id: String,
        access_token: String,
        device_id: String,
        homeserver_url: String,
    }

    struct MatrixLoginIdentityProvider {
        id: String,
        name: String,
        icon: String,
        brand: String,
    }

    struct MatrixLoginFlows {
        homeserver_url: String,
        password_supported: bool,
        sso_supported: bool,
        oauth_supported: bool,
        identity_providers: Vec<MatrixLoginIdentityProvider>,
    }

    struct MatrixSsoCallbackServer {
        listener_id: u64,
        callback_url: String,
    }

    struct MatrixSsoCallbackStatus {
        ready: bool,
        success: bool,
        login_token: String,
        callback_query: String,
    }

    struct MatrixOauthLoginStartResult {
        login_id: u64,
        login_url: String,
    }

    struct MatrixPersistedSessionSecrets {
        store_passphrase: String,
        homeserver_url: String,
        serialized_session: String,
    }

    unsafe extern "C++" {
        include!("matrix/backend/MatrixBackendBridge.h");

        #[namespace = "komai::rust_bridge"]
        fn matrix_profile_data_root(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn matrix_profile_cache_root(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn settings_profile_config_path(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn settings_profile_state_path(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn settings_profile_session_path(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn settings_read_text_file(path: &str, label: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn settings_write_text_file(
            path: &str,
            content: &str,
            owner_read_write_only: bool,
        ) -> bool;
        #[namespace = "komai::rust_bridge"]
        fn matrix_load_session_secrets(profile_id: &str) -> MatrixPersistedSessionSecrets;
        #[namespace = "komai::rust_bridge"]
        fn matrix_save_session_secrets(
            profile_id: &str,
            store_passphrase: &str,
            homeserver_url: &str,
            serialized_session: &str,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_clear_session_secrets(profile_id: &str);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_room_list_snapshot_updated(
            handle_id: u64,
            room_list: Vec<MatrixRoomSummary>,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_ignored_user_list_updated(handle_id: u64, user_ids: Vec<String>);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_initial_sync_ready(handle_id: u64);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_room_timeline_snapshot_updated(handle_id: u64, room_id: &str);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_notification_received(handle_id: u64, room_id: &str, event_id: &str);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_notification_item_received(handle_id: u64, item: MatrixNotificationItem);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_invite_received(handle_id: u64, event: MatrixCallInviteEvent);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_candidates_received(
            handle_id: u64,
            event: MatrixCallCandidatesEvent,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_answer_received(handle_id: u64, event: MatrixCallAnswerEvent);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_hangup_received(handle_id: u64, event: MatrixCallHangUpEvent);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_select_answer_received(
            handle_id: u64,
            event: MatrixCallSelectAnswerEvent,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_reject_received(handle_id: u64, event: MatrixCallRejectEvent);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_call_negotiate_received(
            handle_id: u64,
            event: MatrixCallNegotiateEvent,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_sync_stopped(handle_id: u64, reason: &str, is_auth_error: bool);
    }

    extern "Rust" {
        type SettingsProfileHandle;

        fn init_logging(
            level: &str,
            log_file_path: &str,
            to_stderr: bool,
            enable_debug: bool,
        );
        fn log_from_cpp(component: &str, level: &str, message: &str);
        fn settings_load_startup_snapshot_from_path(config_path: &str) -> SettingsStartupSnapshot;
        fn settings_load_config_overview(config_text: &str) -> SettingsConfigOverview;
        fn settings_load_config_overview_for_profile(profile_id: &str) -> SettingsConfigOverview;
        fn settings_encode_string_map_yaml(entries: &Vec<SettingsStringMapEntry>) -> String;
        fn settings_decode_string_map_yaml(serialized: &str) -> Vec<SettingsStringMapEntry>;
        fn settings_encode_named_string_map_yaml(
            root_key: &str,
            entries: &Vec<SettingsStringMapEntry>,
        ) -> String;
        fn settings_decode_named_string_map_yaml(
            serialized: &str,
            root_key: &str,
        ) -> Vec<SettingsStringMapEntry>;
        fn settings_load_named_string_map_from_path(
            path: &str,
            label: &str,
            root_key: &str,
        ) -> Vec<SettingsStringMapEntry>;
        fn settings_write_named_string_map_to_path(
            path: &str,
            root_key: &str,
            entries: &Vec<SettingsStringMapEntry>,
            owner_read_write_only: bool,
        ) -> bool;
        fn settings_write_config_snapshot_to_path(
            config_path: &str,
            snapshot: &SettingsConfigSnapshot,
        ) -> bool;
        fn settings_load_config_snapshot(config_text: &str) -> SettingsLoadedConfig;
        fn settings_open_profile_handle_for_profile(
            profile_id: &str,
            include_session: bool,
        ) -> Box<SettingsProfileHandle>;
        fn settings_profile_snapshot(handle: &SettingsProfileHandle) -> SettingsLoadedProfile;
        fn settings_profile_set_config_secrets_provider(
            handle: Pin<&mut SettingsProfileHandle>,
            provider: &str,
        );
        fn settings_profile_replace_config_snapshot(
            handle: Pin<&mut SettingsProfileHandle>,
            snapshot: &SettingsConfigSnapshot,
        );
        fn settings_profile_replace_session_identity(
            handle: Pin<&mut SettingsProfileHandle>,
            user_id: &str,
            homeserver: &str,
            device_id: &str,
        );
        fn settings_profile_replace_state_snapshot(
            handle: Pin<&mut SettingsProfileHandle>,
            snapshot: &SettingsStateSnapshot,
        );
        fn settings_profile_write_config(handle: &SettingsProfileHandle) -> bool;
        fn settings_profile_write_session(handle: &SettingsProfileHandle) -> bool;
        fn settings_profile_write_state(handle: &SettingsProfileHandle) -> bool;
        fn settings_load_session_snapshot(session_text: &str) -> SettingsLoadedSession;
        fn settings_load_session_snapshot_for_profile(profile_id: &str) -> SettingsLoadedSession;
        fn settings_write_session_snapshot_to_path(
            session_path: &str,
            user_id: &str,
            homeserver: &str,
            device_id: &str,
        ) -> bool;
        fn settings_load_state_snapshot(state_text: &str) -> SettingsLoadedState;
        fn settings_write_state_snapshot_to_path(
            state_path: &str,
            snapshot: &SettingsStateSnapshot,
        ) -> bool;
        fn theme_parse_external_theme(theme_text: &str) -> ThemeExternalParseResult;
        fn theme_parse_base16_yaml(theme_text: &str) -> ThemeBase16ParseResult;

        fn resolve_server(
            context: MatrixFfiBlockingContext,
            server_name: &str,
        ) -> Result<ResolveResult>;
        fn matrix_sdk_paths(profile_id: &str) -> MatrixSdkPaths;
        fn matrix_restore_session_preview(
            context: MatrixFfiBlockingContext,
            profile_id: &str,
        ) -> Result<MatrixRestorePreview>;
        fn matrix_start_restored_backend(
            context: MatrixFfiBlockingContext,
            profile_id: &str,
        ) -> Result<MatrixBackendHandleInfo>;
        fn matrix_logout_backend(context: MatrixFfiBlockingContext, handle_id: u64) -> Result<()>;
        fn matrix_stop_backend(handle_id: u64) -> Result<()>;
        fn matrix_start_media_proxy(handle_id: u64) -> Result<u16>;
        fn matrix_is_timeline_media_encrypted(handle_id: u64, item_id: &str) -> bool;
        fn matrix_register_timeline_media_proxy_url(
            handle_id: u64,
            item_id: &str,
            file_extension: &str,
        ) -> Result<String>;
        fn matrix_stop_media_proxy(handle_id: u64);
        fn matrix_start_backend_sync(handle_id: u64) -> Result<()>;
        fn matrix_join_room(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id_or_alias: &str,
            via: &Vec<String>,
            reason: &str,
        ) -> MatrixJoinRoomResult;
        fn matrix_knock_room(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id_or_alias: &str,
            via: &Vec<String>,
            reason: &str,
        ) -> Result<String>;
        fn matrix_create_room(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            name: &str,
            topic: &str,
            room_alias_localpart: &str,
            invite_user_ids: &Vec<String>,
            preset: &str,
            is_direct: bool,
            is_encrypted: bool,
            is_space: bool,
            is_public: bool,
        ) -> Result<String>;
        fn matrix_leave_room(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_toggle_room_tag(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            tag: &str,
            enabled: bool,
        ) -> Result<()>;
        fn matrix_set_room_is_direct(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            is_direct: bool,
        ) -> Result<()>;
        fn matrix_invite_user(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_kick_user(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_ban_user(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_unban_user(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_fetch_own_profile(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixOwnProfile>;
        fn matrix_fetch_own_presence(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixOwnPresence>;
        fn matrix_fetch_recovery_status(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixRecoveryStatus>;
        fn matrix_setup_recovery(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            use_ssss: bool,
            passphrase: &str,
            encryption_backup_online_enabled: bool,
        ) -> Result<MatrixSetupRecoveryResult>;
        fn matrix_recover_encryption_secrets(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            key_or_passphrase: &str,
        ) -> Result<()>;
        fn matrix_start_reset_encryption_identity(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixResetEncryptionIdentityResult>;
        fn matrix_continue_reset_encryption_identity_with_password(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            password: &str,
        ) -> Result<()>;
        fn matrix_continue_reset_encryption_identity_after_approval(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<()>;
        fn matrix_cancel_reset_encryption_identity(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<()>;
        fn matrix_start_sign_out_device(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            device_id: &str,
        ) -> Result<MatrixDeviceSignOutResult>;
        fn matrix_continue_sign_out_device_with_password(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            password: &str,
        ) -> Result<()>;
        fn matrix_rename_device(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            device_id: &str,
            display_name: &str,
        ) -> Result<()>;
        fn matrix_start_self_verification(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixVerificationSession>;
        fn matrix_start_user_verification(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
        ) -> Result<MatrixVerificationSession>;
        fn matrix_start_device_verification(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
            device_id: &str,
        ) -> Result<MatrixVerificationSession>;
        fn matrix_unverify_device(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
            device_id: &str,
        ) -> Result<()>;
        fn matrix_block_device(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
            device_id: &str,
        ) -> Result<()>;
        fn matrix_unblock_device(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
            device_id: &str,
        ) -> Result<()>;
        fn matrix_fetch_user_verification_state(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
        ) -> Result<MatrixUserVerificationState>;
        fn matrix_take_pending_verification_flow_ids(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<Vec<String>>;
        fn matrix_fetch_verification_session(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            flow_id: &str,
        ) -> Result<MatrixVerificationSession>;
        fn matrix_clear_verification_session(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            flow_id: &str,
        ) -> Result<()>;
        fn matrix_advance_verification_session(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            flow_id: &str,
        ) -> Result<()>;
        fn matrix_cancel_verification_session(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            flow_id: &str,
            mismatch: bool,
        ) -> Result<()>;
        fn matrix_fetch_user_profile(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
        ) -> Result<MatrixUserProfile>;
        fn matrix_fetch_room_member_profile(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            user_id: &str,
        ) -> Result<MatrixUserProfile>;
        fn matrix_search_users(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            search_term: &str,
            limit: u64,
        ) -> Result<Vec<MatrixDirectoryUser>>;
        fn matrix_fetch_public_room_directory_page(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            search_term: &str,
            limit: u64,
            since: &str,
            server: &str,
        ) -> Result<MatrixPublicRoomDirectoryPage>;
        fn matrix_set_own_display_name(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            display_name: &str,
        ) -> Result<()>;
        fn matrix_set_own_presence(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            presence_state: &str,
            status_message: &str,
        ) -> Result<()>;
        fn matrix_set_own_room_display_name(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            display_name: &str,
        ) -> Result<()>;
        fn matrix_upload_own_avatar(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            file_path: &str,
            mime_type: &str,
        ) -> Result<()>;
        fn matrix_remove_own_avatar(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<()>;
        fn matrix_upload_own_room_avatar(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            file_path: &str,
            mime_type: &str,
        ) -> Result<()>;
        fn matrix_remove_own_room_avatar(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<()>;
        fn matrix_ignore_user(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
        ) -> Result<()>;
        fn matrix_unignore_user(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            user_id: &str,
        ) -> Result<()>;
        fn matrix_set_invite_permission(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            target: &str,
            block: bool,
        ) -> Result<()>;
        fn matrix_fetch_room_list(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<Vec<MatrixRoomSummary>>;
        fn matrix_fetch_notification_items(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            requests: &Vec<MatrixNotificationRequest>,
        ) -> Result<Vec<MatrixNotificationItem>>;
        fn matrix_fetch_account_notifications_enabled(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<bool>;
        fn matrix_fetch_turn_server_info(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<MatrixTurnServerInfo>;
        fn matrix_set_account_notifications_enabled(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            enabled: bool,
        ) -> Result<()>;
        fn matrix_fetch_image_packs(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<Vec<MatrixImagePack>>;
        fn matrix_save_image_pack(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            state_key: &str,
            previous_state_key: &str,
            has_previous_state_key: bool,
            pack: MatrixImagePack,
        ) -> Result<()>;
        fn matrix_remove_image_pack(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            state_key: &str,
        ) -> Result<()>;
        fn matrix_set_image_pack_globally_enabled(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            state_key: &str,
            enabled: bool,
        ) -> Result<()>;
        fn matrix_fetch_room_settings(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<MatrixRoomSettings>;
        fn matrix_fetch_room_aliases(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<MatrixRoomAliases>;
        fn matrix_apply_room_aliases(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            aliases: MatrixRoomAliases,
        ) -> Result<()>;
        fn matrix_fetch_room_members(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<Vec<MatrixRoomMember>>;
        fn matrix_fetch_room_power_levels(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<MatrixRoomPowerLevels>;
        fn matrix_apply_room_power_levels(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            power_levels: MatrixRoomPowerLevels,
        ) -> Result<()>;
        fn matrix_fetch_media_content(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            mxc_uri: &str,
            width: i32,
            height: i32,
            crop: bool,
        ) -> Result<Vec<u8>>;
        fn matrix_set_room_notification_mode(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            mode: i32,
        ) -> Result<()>;
        fn matrix_set_room_name(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            name: &str,
        ) -> Result<()>;
        fn matrix_set_room_topic(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            topic: &str,
        ) -> Result<()>;
        fn matrix_upload_room_avatar(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            file_path: &str,
            mime_type: &str,
            width: i32,
            height: i32,
        ) -> Result<()>;
        fn matrix_remove_room_avatar(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<()>;
        fn matrix_enable_room_encryption(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<()>;
        fn matrix_set_room_history_visibility(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            history_visibility: &str,
        ) -> Result<()>;
        fn matrix_set_room_access_rules(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            join_rule_kind: &str,
            guest_access: bool,
            allowed_room_ids: &Vec<String>,
        ) -> Result<()>;
        fn matrix_set_active_room_timeline_initial_page_size(
            handle_id: u64,
            page_size: u16,
        ) -> Result<()>;
        fn matrix_select_active_room_timeline(handle_id: u64, room_id: &str) -> Result<()>;
        fn matrix_fetch_active_room_timeline(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
        ) -> Result<Vec<MatrixTimelineItem>>;
        fn matrix_paginate_active_room_timeline_backwards(
            handle_id: u64,
            page_size: u16,
        ) -> Result<()>;
        fn matrix_fetch_active_room_timeline_media_content(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            item_id: &str,
            width: i32,
            height: i32,
            crop: bool,
        ) -> Result<Vec<u8>>;
        fn matrix_send_room_message(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            body: &str,
            formatted_html: &str,
            message_kind: &str,
        ) -> Result<()>;
        fn matrix_send_room_message_like_event_json(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_type: &str,
            content_json: &str,
        ) -> Result<()>;
        fn matrix_send_call_invite(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            version: &str,
            lifetime: u32,
            invitee: &str,
            offer_sdp: &str,
            offer_type: &str,
        ) -> Result<()>;
        fn matrix_send_call_candidates(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            version: &str,
            candidates: Vec<MatrixCallIceCandidate>,
        ) -> Result<()>;
        fn matrix_send_call_answer(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            version: &str,
            answer_sdp: &str,
            answer_type: &str,
        ) -> Result<()>;
        fn matrix_send_call_hangup(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            version: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_send_call_select_answer(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            version: &str,
            selected_party_id: &str,
        ) -> Result<()>;
        fn matrix_send_call_reject(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            version: &str,
        ) -> Result<()>;
        fn matrix_send_call_negotiate(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            call_id: &str,
            party_id: &str,
            lifetime: u32,
            description_sdp: &str,
            description_type: &str,
        ) -> Result<()>;
        fn matrix_send_room_reply_message(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            replied_to_event_id: &str,
            body: &str,
            formatted_html: &str,
            message_kind: &str,
        ) -> Result<()>;
        fn matrix_send_room_edit_message(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            target_event_id: &str,
            body: &str,
            formatted_html: &str,
            message_kind: &str,
        ) -> Result<()>;
        fn matrix_toggle_room_reaction(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
            reaction_key: &str,
        ) -> Result<()>;
        fn matrix_redact_room_event(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_mark_room_event_as_read(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<()>;
        fn matrix_report_room_event(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
            reason: &str,
            score: i32,
        ) -> Result<()>;
        fn matrix_fetch_room_pinned_event_ids(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<Vec<String>>;
        fn matrix_pin_room_event(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<()>;
        fn matrix_unpin_room_event(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<()>;
        fn matrix_fetch_active_room_raw_event_dialog_data(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<MatrixRawEventDialogData>;
        fn matrix_fetch_active_room_event_content_for_forwarding(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<MatrixEventContentForForwarding>;
        fn matrix_fetch_room_read_receipts(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<Vec<MatrixReadReceiptEntry>>;
        fn matrix_fetch_room_redaction_permissions(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
        ) -> Result<MatrixRoomRedactionPermissions>;
        fn matrix_send_room_attachment(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            file_path: &str,
            filename: &str,
            caption: &str,
            reply_event_id: &str,
            mime_type: &str,
        ) -> Result<()>;
        fn matrix_upload_media(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            file_path: &str,
            mime_type: &str,
        ) -> Result<String>;
        fn matrix_send_room_image(
            context: MatrixFfiBlockingContext,
            handle_id: u64,
            room_id: &str,
            mxc_uri: &str,
            body: &str,
            filename: &str,
            info_json: &str,
        ) -> Result<()>;
        fn matrix_discover_login_flows(
            context: MatrixFfiBlockingContext,
            server_name_or_url: &str,
            verify_certificates: bool,
        ) -> Result<MatrixLoginFlows>;
        fn matrix_get_sso_login_url(
            context: MatrixFfiBlockingContext,
            homeserver_url: &str,
            redirect_url: &str,
            identity_provider_id: &str,
            verify_certificates: bool,
        ) -> Result<String>;
        fn matrix_start_sso_callback_server(
            success_html: &str,
            failure_html: &str,
            timeout_ms: u32,
        ) -> Result<MatrixSsoCallbackServer>;
        fn matrix_poll_sso_callback_server(listener_id: u64) -> Result<MatrixSsoCallbackStatus>;
        fn matrix_stop_sso_callback_server(listener_id: u64) -> Result<()>;
        fn matrix_start_oauth_login(
            context: MatrixFfiBlockingContext,
            profile_id: &str,
            homeserver_url: &str,
            redirect_url: &str,
            user_id_hint: &str,
            device_id: &str,
            initial_device_display_name: &str,
            verify_certificates: bool,
        ) -> Result<MatrixOauthLoginStartResult>;
        fn matrix_finish_oauth_login(
            context: MatrixFfiBlockingContext,
            login_id: u64,
            callback_query: &str,
        ) -> Result<MatrixLoginResult>;
        fn matrix_cancel_oauth_login(login_id: u64) -> Result<()>;
        fn matrix_login_password(
            context: MatrixFfiBlockingContext,
            profile_id: &str,
            homeserver_url: &str,
            user_id: &str,
            password: &str,
            device_id: &str,
            initial_device_display_name: &str,
            verify_certificates: bool,
        ) -> Result<MatrixLoginResult>;
        fn matrix_login_token(
            context: MatrixFfiBlockingContext,
            profile_id: &str,
            homeserver_url: &str,
            login_token: &str,
            device_id: &str,
            initial_device_display_name: &str,
            verify_certificates: bool,
        ) -> Result<MatrixLoginResult>;
    }
}

pub(crate) use bridge::*;
