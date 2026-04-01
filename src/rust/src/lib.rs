// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::sync::OnceLock;

use resolvematrix::server::MatrixResolver;
use tokio::runtime::Runtime;

pub mod logging;
pub mod matrix_backend;
pub mod settings;
pub mod theme;

#[cxx::bridge(namespace = "komai::rust")]
mod ffi {
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

    struct SettingsStringMapEntry {
        key: String,
        value: String,
    }

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

    struct SettingsConfigTimelineHiddenEventsSection {
        has_global: bool,
        global: Vec<String>,
        by_room: Vec<SettingsStringListMapEntry>,
    }

    struct SettingsConfigTimelineSection {
        hidden_events: SettingsConfigTimelineHiddenEventsSection,
    }

    struct SettingsConfigSecretsSection {
        provider: String,
    }

    enum SettingsConfigValueKind {
        Bool,
        Int,
        Double,
        String,
        StringList,
        StringListMap,
    }

    struct SettingsConfigValue {
        key: String,
        kind: SettingsConfigValueKind,
        bool_value: bool,
        int_value: i32,
        double_value: f64,
        string_value: String,
        string_list_value: Vec<String>,
        string_list_map_value: Vec<SettingsStringListMapEntry>,
    }

    struct SettingsConfigSnapshot {
        ui: SettingsConfigUiSection,
        timeline: SettingsConfigTimelineSection,
        secrets: SettingsConfigSecretsSection,
        values: Vec<SettingsConfigValue>,
    }

    struct SettingsLoadedConfig {
        ui: SettingsConfigUiSection,
        timeline: SettingsConfigTimelineSection,
        secrets: SettingsConfigSecretsSection,
        values: Vec<SettingsConfigValue>,
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
        fn init_logging(
            level: &str,
            log_file_path: &str,
            to_stderr: bool,
            enable_debug: bool,
        );
        fn log_from_cpp(component: &str, level: &str, message: &str);
        fn settings_load_startup_snapshot(config_text: &str) -> SettingsStartupSnapshot;
        fn settings_load_config_overview(config_text: &str) -> SettingsConfigOverview;
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
        fn settings_encode_config_yaml(snapshot: &SettingsConfigSnapshot) -> String;
        fn settings_load_config_snapshot(config_text: &str) -> SettingsLoadedConfig;
        fn settings_load_profile_snapshot(
            config_text: &str,
            session_text: &str,
            state_text: &str,
        ) -> SettingsLoadedProfile;
        fn settings_load_session_snapshot(session_text: &str) -> SettingsLoadedSession;
        fn settings_encode_session_yaml(user_id: &str, homeserver: &str, device_id: &str)
        -> String;
        fn settings_load_state_snapshot(state_text: &str) -> SettingsLoadedState;
        fn settings_encode_state_yaml(snapshot: &SettingsStateSnapshot) -> String;
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

fn init_logging(level: &str, log_file_path: &str, to_stderr: bool, enable_debug: bool) {
    logging::init_logging(level, log_file_path, to_stderr, enable_debug);
}

fn log_from_cpp(component: &str, level: &str, message: &str) {
    logging::log_from_cpp(component, level, message);
}

fn settings_load_startup_snapshot(config_text: &str) -> ffi::SettingsStartupSnapshot {
    let snapshot = settings::startup::snapshot_from_config_text(config_text);

    ffi::SettingsStartupSnapshot {
        has_ui_scale_factor: snapshot.ui_scale_factor.is_some(),
        ui_scale_factor: snapshot.ui_scale_factor.unwrap_or_default(),
    }
}

fn settings_load_config_overview(config_text: &str) -> ffi::SettingsConfigOverview {
    let config = settings::config::parse_config_text(config_text);

    ffi::SettingsConfigOverview {
        has_ui_scale_factor: config.ui.scale.factor.is_some(),
        ui_scale_factor: config.ui.scale.factor.unwrap_or_default(),
        theme_slug: config.ui.theme.slug,
        secrets_provider: config.secrets.provider,
    }
}

fn settings_encode_string_map_yaml(entries: &Vec<ffi::SettingsStringMapEntry>) -> String {
    settings::secrets::encode_string_map_yaml(entries.as_slice())
}

fn settings_decode_string_map_yaml(serialized: &str) -> Vec<ffi::SettingsStringMapEntry> {
    settings::secrets::decode_string_map_yaml(serialized)
}

fn settings_encode_named_string_map_yaml(
    root_key: &str,
    entries: &Vec<ffi::SettingsStringMapEntry>,
) -> String {
    settings::secrets::encode_named_string_map_yaml(root_key, entries.as_slice())
}

fn settings_decode_named_string_map_yaml(
    serialized: &str,
    root_key: &str,
) -> Vec<ffi::SettingsStringMapEntry> {
    settings::secrets::decode_named_string_map_yaml(serialized, root_key)
}

fn settings_encode_config_yaml(snapshot: &ffi::SettingsConfigSnapshot) -> String {
    settings::config::encode_config_yaml(snapshot)
}

fn ffi_config_ui_section(config: &settings::config::Config) -> ffi::SettingsConfigUiSection {
    ffi::SettingsConfigUiSection {
        has_scale_factor: config.ui.scale.factor.is_some(),
        scale_factor: config.ui.scale.factor.unwrap_or_default(),
        theme_slug: config.ui.theme.slug.clone(),
        has_font_size_pt: config.ui.font.size_pt.is_some(),
        font_size_pt: config.ui.font.size_pt.unwrap_or_default(),
        font_family: config.ui.font.family.clone(),
        font_emoji_family: config.ui.font.emoji_family.clone(),
        has_motion_animations_enabled: config.ui.motion.animations_enabled.is_some(),
        motion_animations_enabled: config.ui.motion.animations_enabled.unwrap_or_default(),
        input_mode: config.ui.input.mode.clone(),
        has_input_touch_swipe_gestures_enabled: config.ui.input.touch_swipe_gestures_enabled.is_some(),
        input_touch_swipe_gestures_enabled: config
            .ui
            .input
            .touch_swipe_gestures_enabled
            .unwrap_or_default(),
        has_layout_content_max_width_px: config.ui.layout.content_max_width_px.is_some(),
        layout_content_max_width_px: config.ui.layout.content_max_width_px.unwrap_or_default(),
        has_layout_compact_mode: config.ui.layout.compact_mode.is_some(),
        layout_compact_mode: config.ui.layout.compact_mode.unwrap_or_default(),
        has_avatars_circular: config.ui.avatars.circular.is_some(),
        avatars_circular: config.ui.avatars.circular.unwrap_or_default(),
        scrollbar_policy: config.ui.scrollbar_policy.clone(),
        default_avatar_style: config.ui.avatars.default_avatar_style.clone(),
    }
}

fn ffi_config_timeline_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigTimelineSection {
    let by_room = config
        .timeline
        .hidden_events
        .by_room
        .iter()
        .map(|(key, values)| ffi::SettingsStringListMapEntry {
            key: key.clone(),
            values: values.clone(),
        })
        .collect();

    ffi::SettingsConfigTimelineSection {
        hidden_events: ffi::SettingsConfigTimelineHiddenEventsSection {
            has_global: config.timeline.hidden_events.global.is_some(),
            global: config.timeline.hidden_events.global.clone().unwrap_or_default(),
            by_room,
        },
    }
}

fn ffi_config_secrets_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigSecretsSection {
    ffi::SettingsConfigSecretsSection {
        provider: config.secrets.provider.clone(),
    }
}

fn ffi_loaded_config(snapshot: settings::config::LoadedConfig) -> ffi::SettingsLoadedConfig {
    ffi::SettingsLoadedConfig {
        ui: ffi_config_ui_section(&snapshot.config),
        timeline: ffi_config_timeline_section(&snapshot.config),
        secrets: ffi_config_secrets_section(&snapshot.config),
        values: snapshot.values,
        source_version: snapshot.source_version,
        migrated_version: snapshot.migrated_version,
        had_future_version: snapshot.had_future_version,
        had_unsupported_path: snapshot.had_unsupported_path,
        should_write_back: snapshot.should_write_back,
        serialized_yaml: snapshot.serialized_yaml,
    }
}

fn ffi_loaded_session(snapshot: settings::session::LoadedSession) -> ffi::SettingsLoadedSession {
    ffi::SettingsLoadedSession {
        user_id: snapshot.user_id,
        device_id: snapshot.device_id,
        homeserver: snapshot.homeserver,
        source_version: snapshot.source_version,
        migrated_version: snapshot.migrated_version,
        had_future_version: snapshot.had_future_version,
        had_unsupported_path: snapshot.had_unsupported_path,
        should_write_back: snapshot.should_write_back,
        serialized_yaml: snapshot.serialized_yaml,
    }
}

fn ffi_loaded_state(snapshot: settings::state::LoadedState) -> ffi::SettingsLoadedState {
    ffi::SettingsLoadedState {
        window_width: snapshot.window_width,
        window_height: snapshot.window_height,
        sidebars_room_list_width_px: snapshot.sidebars_room_list_width_px,
        sidebars_communities_width_px: snapshot.sidebars_communities_width_px,
        current_filter_id: snapshot.current_filter_id,
        current_room_id: snapshot.current_room_id,
        global_excludes: snapshot.global_excludes,
        badges_hidden_filters: snapshot.badges_hidden_filters,
        hidden_pins: snapshot.hidden_pins,
        hidden_widgets: snapshot.hidden_widgets,
        collapsed_spaces: snapshot.collapsed_spaces,
        composer_drafts_by_room: snapshot.composer_drafts_by_room,
        source_version: snapshot.source_version,
        migrated_version: snapshot.migrated_version,
        had_future_version: snapshot.had_future_version,
        had_unsupported_path: snapshot.had_unsupported_path,
        should_write_back: snapshot.should_write_back,
        serialized_yaml: snapshot.serialized_yaml,
    }
}

fn settings_load_config_snapshot(config_text: &str) -> ffi::SettingsLoadedConfig {
    ffi_loaded_config(settings::config::load_config_snapshot(config_text))
}

fn settings_load_profile_snapshot(
    config_text: &str,
    session_text: &str,
    state_text: &str,
) -> ffi::SettingsLoadedProfile {
    let snapshot = settings::profile::load_profile_snapshot(config_text, session_text, state_text);

    ffi::SettingsLoadedProfile {
        config: ffi_loaded_config(snapshot.config),
        session: ffi_loaded_session(snapshot.session),
        state: ffi_loaded_state(snapshot.state),
    }
}

fn settings_load_session_snapshot(session_text: &str) -> ffi::SettingsLoadedSession {
    ffi_loaded_session(settings::session::load_session_snapshot(session_text))
}

fn settings_encode_session_yaml(user_id: &str, homeserver: &str, device_id: &str) -> String {
    settings::session::encode_session_yaml(user_id, homeserver, device_id)
}

fn settings_load_state_snapshot(state_text: &str) -> ffi::SettingsLoadedState {
    ffi_loaded_state(settings::state::load_state_snapshot(state_text))
}

fn settings_encode_state_yaml(snapshot: &ffi::SettingsStateSnapshot) -> String {
    settings::state::encode_state_yaml(snapshot)
}

fn theme_parse_external_theme(theme_text: &str) -> ffi::ThemeExternalParseResult {
    theme::external::parse_external_theme(theme_text)
}

fn theme_parse_base16_yaml(theme_text: &str) -> ffi::ThemeBase16ParseResult {
    theme::base16::parse_base16_yaml(theme_text)
}

fn runtime() -> &'static Runtime {
    static RT: OnceLock<Runtime> = OnceLock::new();
    RT.get_or_init(|| Runtime::new().expect("failed to create tokio runtime"))
}

fn resolver() -> &'static MatrixResolver {
    static RES: OnceLock<MatrixResolver> = OnceLock::new();
    RES.get_or_init(|| {
        runtime()
            .block_on(MatrixResolver::new())
            .expect("failed to create MatrixResolver")
    })
}

fn ffi_block_on<F, T>(
    context: ffi::MatrixFfiBlockingContext,
    operation: &'static str,
    future: F,
) -> T
where
    F: std::future::Future<Output = T>,
{
    // Exported blocking FFI entrypoints must come through here so the C++-chosen thread policy is
    // enforced at one Rust choke point instead of letting raw runtime().block_on(...) calls spread.


    if matches!(
        context.thread_policy,
        ffi::MatrixFfiBlockingThreadPolicy::RequireWorkerThread
    ) && matches!(context.caller_thread, ffi::MatrixFfiCallerThread::AppUiThread)
    {
        panic!(
            "Blocking matrix-sdk FFI call '{}' was invoked from the app/UI thread",
            operation
        );
    }

    runtime().block_on(future)
}

fn resolve_server(
    context: ffi::MatrixFfiBlockingContext,
    server_name: &str,
) -> Result<ffi::ResolveResult, String> {
    let resolution = ffi_block_on(context, "resolve_server", resolver().resolve_server(server_name))
        .map_err(|e| format!("failed to resolve server '{}': {}", server_name, e))?;

    Ok(ffi::ResolveResult {
        base_url: resolution.base_url(),
    })
}

fn matrix_sdk_paths(profile_id: &str) -> ffi::MatrixSdkPaths {

    let paths = matrix_backend::derive_matrix_sdk_paths(
        &ffi::matrix_profile_data_root(profile_id),
        &ffi::matrix_profile_cache_root(profile_id),
    );

    ffi::MatrixSdkPaths {
        profile_data_root: paths.profile_data_root,
        profile_cache_root: paths.profile_cache_root,
        matrix_data_root: paths.matrix_data_root,
        matrix_cache_root: paths.matrix_cache_root,
        state_store_root: paths.state_store_root,
        cache_root: paths.cache_root,
        event_cache_root: paths.event_cache_root,
        media_cache_root: paths.media_cache_root,
    }
}

fn matrix_restore_session_preview(
    context: ffi::MatrixFfiBlockingContext,
    profile_id: &str,
) -> Result<ffi::MatrixRestorePreview, String> {
    let preview = ffi_block_on(
        context,
        "matrix_restore_session_preview",
        matrix_backend::bootstrap::restore_session_preview(profile_id),
    )?;

    Ok(ffi::MatrixRestorePreview {
        has_session: preview.has_session,
        session_source: preview.session_source,
        auth_type: preview.auth_type,
        homeserver_url: preview.homeserver_url,
        user_id: preview.user_id,
        device_id: preview.device_id,
        state_store_root: preview.state_store_root,
        cache_root: preview.cache_root,
    })
}

fn matrix_start_restored_backend(
    context: ffi::MatrixFfiBlockingContext,
    profile_id: &str,
) -> Result<ffi::MatrixBackendHandleInfo, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_restored_backend",
        matrix_backend::runtime::start_restored_backend(profile_id),
    )?;

    Ok(ffi::MatrixBackendHandleInfo {
        handle_id: result.handle_id,
        has_session: result.has_session,
        auth_type: result.auth_type,
        homeserver_url: result.homeserver_url,
        user_id: result.user_id,
        device_id: result.device_id,
    })
}

fn matrix_logout_backend(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<(), String> {

    ffi_block_on(
        context,
        "matrix_logout_backend",
        matrix_backend::runtime::logout_backend(handle_id),
    )
}

fn matrix_stop_backend(handle_id: u64) -> Result<(), String> {

    matrix_backend::runtime::stop_backend(handle_id)
}

fn matrix_start_media_proxy(handle_id: u64) -> Result<u16, String> {
    matrix_backend::runtime::start_media_proxy(handle_id)
}

fn matrix_is_timeline_media_encrypted(handle_id: u64, item_id: &str) -> bool {
    matrix_backend::runtime::is_timeline_media_encrypted(handle_id, item_id)
}

fn matrix_register_timeline_media_proxy_url(
    handle_id: u64,
    item_id: &str,
    file_extension: &str,
) -> Result<String, String> {
    matrix_backend::runtime::register_timeline_media_proxy_url(handle_id, item_id, file_extension)
}

fn matrix_stop_media_proxy(handle_id: u64) {
    matrix_backend::runtime::stop_media_proxy(handle_id)
}

fn matrix_start_backend_sync(handle_id: u64) -> Result<(), String> {

    matrix_backend::runtime::start_sync(handle_id)
}

fn matrix_join_room(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id_or_alias: &str,
    via: &Vec<String>,
    reason: &str,
) -> ffi::MatrixJoinRoomResult {

    match ffi_block_on(
        context,
        "matrix_join_room",
        matrix_backend::runtime::join_room(handle_id, room_id_or_alias, via.as_slice(), reason),
    ) {
        Ok(room_id) => ffi::MatrixJoinRoomResult {
            ok: true,
            room_id,
            error: String::new(),
            matrix_errcode: String::new(),
        },
        Err((error, matrix_errcode)) => ffi::MatrixJoinRoomResult {
            ok: false,
            room_id: room_id_or_alias.to_owned(),
            error,
            matrix_errcode,
        },
    }
}

fn matrix_knock_room(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id_or_alias: &str,
    via: &Vec<String>,
    reason: &str,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_knock_room",
        matrix_backend::runtime::knock_room(
        handle_id,
        room_id_or_alias,
        via.as_slice(),
        reason,
        ),
    )
}

#[allow(clippy::too_many_arguments)]
fn matrix_create_room(
    context: ffi::MatrixFfiBlockingContext,
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
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_create_room",
        matrix_backend::runtime::create_room(
        handle_id,
        name,
        topic,
        room_alias_localpart,
        invite_user_ids.as_slice(),
        preset,
        is_direct,
        is_encrypted,
        is_space,
        is_public,
        ),
    )
}

fn matrix_leave_room(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    reason: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_leave_room",
        matrix_backend::runtime::leave_room(handle_id, room_id, reason),
    )
}

fn matrix_toggle_room_tag(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    tag: &str,
    enabled: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_toggle_room_tag",
        matrix_backend::runtime::toggle_room_tag(handle_id, room_id, tag, enabled),
    )
}

fn matrix_set_room_is_direct(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    is_direct: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_room_is_direct",
        matrix_backend::runtime::set_room_is_direct(handle_id, room_id, is_direct),
    )
}

fn matrix_invite_user(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_invite_user",
        matrix_backend::runtime::invite_user(handle_id, room_id, user_id, reason),
    )
}

fn matrix_kick_user(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_kick_user",
        matrix_backend::runtime::kick_user(handle_id, room_id, user_id, reason),
    )
}

fn matrix_ban_user(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_ban_user",
        matrix_backend::runtime::ban_user(handle_id, room_id, user_id, reason),
    )
}

fn matrix_unban_user(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_unban_user",
        matrix_backend::runtime::unban_user(handle_id, room_id, user_id, reason),
    )
}

fn matrix_fetch_own_profile(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixOwnProfile, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_own_profile",
        matrix_backend::runtime::fetch_own_profile(handle_id),
    )?;

    Ok(ffi::MatrixOwnProfile {
        display_name: result.display_name,
        avatar_url: result.avatar_url,
    })
}

fn matrix_fetch_own_presence(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixOwnPresence, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_own_presence",
        matrix_backend::runtime::fetch_own_presence(handle_id),
    )?;

    Ok(ffi::MatrixOwnPresence {
        state: result.state,
        status_message: result.status_message,
    })
}

fn matrix_fetch_recovery_status(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixRecoveryStatus, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_recovery_status",
        matrix_backend::runtime::fetch_recovery_status(handle_id),
    )?;

    Ok(ffi::MatrixRecoveryStatus {
        state: result.state,
        has_devices_to_verify_against: result.has_devices_to_verify_against,
        own_device_is_verified: result.own_device_is_verified,
        has_unverified_own_devices: result.has_unverified_own_devices,
    })
}

fn matrix_setup_recovery(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    use_ssss: bool,
    passphrase: &str,
    encryption_backup_online_enabled: bool,
) -> Result<ffi::MatrixSetupRecoveryResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_setup_recovery",
        matrix_backend::runtime::setup_recovery(
            handle_id,
            use_ssss,
            passphrase,
            encryption_backup_online_enabled,
        ),
    )?;

    Ok(ffi::MatrixSetupRecoveryResult {
        recovery_key: result.recovery_key,
    })
}

fn matrix_recover_encryption_secrets(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    key_or_passphrase: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_recover_encryption_secrets",
        matrix_backend::runtime::recover_encryption_secrets(handle_id, key_or_passphrase),
    )
}

fn matrix_start_reset_encryption_identity(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixResetEncryptionIdentityResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_reset_encryption_identity",
        matrix_backend::runtime::start_reset_encryption_identity(handle_id),
    )?;

    Ok(ffi::MatrixResetEncryptionIdentityResult {
        completed: result.completed,
        auth_type: result.auth_type,
        approval_url: result.approval_url,
    })
}

fn matrix_continue_reset_encryption_identity_with_password(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    password: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_continue_reset_encryption_identity_with_password",
        matrix_backend::runtime::continue_reset_encryption_identity_with_password(
            handle_id, password,
        ),
    )
}

fn matrix_continue_reset_encryption_identity_after_approval(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_continue_reset_encryption_identity_after_approval",
        matrix_backend::runtime::continue_reset_encryption_identity_after_approval(handle_id),
    )
}

fn matrix_cancel_reset_encryption_identity(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_cancel_reset_encryption_identity",
        matrix_backend::runtime::cancel_reset_encryption_identity(handle_id),
    )
}

fn matrix_start_sign_out_device(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    device_id: &str,
) -> Result<ffi::MatrixDeviceSignOutResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_sign_out_device",
        matrix_backend::runtime::start_sign_out_device(handle_id, device_id),
    )?;

    Ok(ffi::MatrixDeviceSignOutResult {
        completed: result.completed,
        auth_type: result.auth_type,
        approval_url: result.approval_url,
    })
}

fn matrix_continue_sign_out_device_with_password(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    password: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_continue_sign_out_device_with_password",
        matrix_backend::runtime::continue_sign_out_device_with_password(handle_id, password),
    )
}

fn matrix_rename_device(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    device_id: &str,
    display_name: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_rename_device",
        matrix_backend::runtime::rename_device(handle_id, device_id, display_name),
    )
}

fn matrix_start_self_verification(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixVerificationSession, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_self_verification",
        matrix_backend::runtime::start_self_verification(handle_id),
    )?;

    Ok(ffi::MatrixVerificationSession {
        flow_id: result.flow_id,
        user_id: result.user_id,
        device_id: result.device_id,
        state: result.state,
        error: result.error,
        sender: result.sender,
        is_self_verification: result.is_self_verification,
        is_multi_device_verification: result.is_multi_device_verification,
        sas_numbers: result.sas_numbers,
    })
}

fn matrix_start_user_verification(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
) -> Result<ffi::MatrixVerificationSession, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_user_verification",
        matrix_backend::runtime::start_user_verification(handle_id, user_id),
    )?;

    Ok(ffi::MatrixVerificationSession {
        flow_id: result.flow_id,
        user_id: result.user_id,
        device_id: result.device_id,
        state: result.state,
        error: result.error,
        sender: result.sender,
        is_self_verification: result.is_self_verification,
        is_multi_device_verification: result.is_multi_device_verification,
        sas_numbers: result.sas_numbers,
    })
}

fn matrix_start_device_verification(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
    device_id: &str,
) -> Result<ffi::MatrixVerificationSession, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_device_verification",
        matrix_backend::runtime::start_device_verification(handle_id, user_id, device_id),
    )?;

    Ok(ffi::MatrixVerificationSession {
        flow_id: result.flow_id,
        user_id: result.user_id,
        device_id: result.device_id,
        state: result.state,
        error: result.error,
        sender: result.sender,
        is_self_verification: result.is_self_verification,
        is_multi_device_verification: result.is_multi_device_verification,
        sas_numbers: result.sas_numbers,
    })
}

fn matrix_unverify_device(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
    device_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_unverify_device",
        matrix_backend::runtime::unverify_device(handle_id, user_id, device_id),
    )
}

fn matrix_block_device(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
    device_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_block_device",
        matrix_backend::runtime::block_device(handle_id, user_id, device_id),
    )
}

fn matrix_unblock_device(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
    device_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_unblock_device",
        matrix_backend::runtime::unblock_device(handle_id, user_id, device_id),
    )
}

fn matrix_fetch_user_verification_state(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
) -> Result<ffi::MatrixUserVerificationState, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_user_verification_state",
        matrix_backend::runtime::fetch_user_verification_state(handle_id, user_id),
    )?;

    Ok(ffi::MatrixUserVerificationState {
        has_master_key: result.has_master_key,
        user_trust: result.user_trust,
        devices: result
            .devices
            .into_iter()
            .map(|device| ffi::MatrixUserDevice {
                device_id: device.device_id,
                display_name: device.display_name,
                verification_state: device.verification_state,
                last_seen_ip: device.last_seen_ip,
                last_seen_ts: device.last_seen_ts,
            })
            .collect(),
    })
}

fn matrix_take_pending_verification_flow_ids(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<Vec<String>, String> {
    Ok(ffi_block_on(
        context,
        "matrix_take_pending_verification_flow_ids",
        async move { matrix_backend::runtime::take_pending_verification_flow_ids(handle_id) },
    )?)
}

fn matrix_fetch_verification_session(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    flow_id: &str,
) -> Result<ffi::MatrixVerificationSession, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_verification_session",
        matrix_backend::runtime::fetch_verification_session(handle_id, flow_id),
    )?;

    Ok(ffi::MatrixVerificationSession {
        flow_id: result.flow_id,
        user_id: result.user_id,
        device_id: result.device_id,
        state: result.state,
        error: result.error,
        sender: result.sender,
        is_self_verification: result.is_self_verification,
        is_multi_device_verification: result.is_multi_device_verification,
        sas_numbers: result.sas_numbers,
    })
}

fn matrix_clear_verification_session(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    flow_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_clear_verification_session",
        matrix_backend::runtime::clear_verification_session(handle_id, flow_id),
    )
}

fn matrix_advance_verification_session(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    flow_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_advance_verification_session",
        matrix_backend::runtime::advance_verification_session(handle_id, flow_id),
    )
}

fn matrix_cancel_verification_session(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    flow_id: &str,
    mismatch: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_cancel_verification_session",
        matrix_backend::runtime::cancel_verification_session(handle_id, flow_id, mismatch),
    )
}

fn matrix_fetch_user_profile(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
) -> Result<ffi::MatrixUserProfile, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_user_profile",
        matrix_backend::runtime::fetch_user_profile(handle_id, user_id),
    )?;

    Ok(ffi::MatrixUserProfile {
        display_name: result.display_name,
        avatar_url: result.avatar_url,
    })
}

fn matrix_fetch_room_member_profile(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    user_id: &str,
) -> Result<ffi::MatrixUserProfile, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_member_profile",
        matrix_backend::runtime::fetch_room_member_profile(handle_id, room_id, user_id),
    )?;

    Ok(ffi::MatrixUserProfile {
        display_name: result.display_name,
        avatar_url: result.avatar_url,
    })
}

fn matrix_search_users(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    search_term: &str,
    limit: u64,
) -> Result<Vec<ffi::MatrixDirectoryUser>, String> {
    ffi_block_on(
        context,
        "matrix_search_users",
        matrix_backend::runtime::search_users(handle_id, search_term, limit),
    )
        .map(|users| {
            users
                .into_iter()
                .map(|user| ffi::MatrixDirectoryUser {
                    display_name: user.display_name,
                    user_id: user.user_id,
                    avatar_url: user.avatar_url,
                })
                .collect()
        })
}

fn matrix_fetch_public_room_directory_page(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    search_term: &str,
    limit: u64,
    since: &str,
    server: &str,
) -> Result<ffi::MatrixPublicRoomDirectoryPage, String> {
    ffi_block_on(
        context,
        "matrix_fetch_public_room_directory_page",
        matrix_backend::runtime::fetch_public_room_directory_page(
            handle_id,
            search_term,
            limit,
            since,
            server,
        ),
    )
        .map(|page| ffi::MatrixPublicRoomDirectoryPage {
            rooms: page
                .rooms
                .into_iter()
                .map(|room| ffi::MatrixPublicRoomDirectoryEntry {
                    room_id: room.room_id,
                    room_server_name: room.room_server_name,
                    display_name: room.display_name,
                    avatar_url: room.avatar_url,
                    topic: room.topic,
                    canonical_alias: room.canonical_alias,
                    member_count: room.member_count,
                    is_world_readable: room.is_world_readable,
                    is_space: room.is_space,
                })
                .collect(),
            next_batch: page.next_batch,
            total_room_count_estimate: page.total_room_count_estimate,
        })
}

fn matrix_set_own_display_name(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    display_name: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_own_display_name",
        matrix_backend::runtime::set_own_display_name(handle_id, display_name),
    )
}

fn matrix_set_own_presence(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    presence_state: &str,
    status_message: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_own_presence",
        matrix_backend::runtime::set_own_presence(handle_id, presence_state, status_message),
    )
}

fn matrix_set_own_room_display_name(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    display_name: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_own_room_display_name",
        matrix_backend::runtime::set_own_room_display_name(handle_id, room_id, display_name),
    )
}

fn matrix_upload_own_avatar(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    file_path: &str,
    mime_type: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_upload_own_avatar",
        matrix_backend::runtime::upload_own_avatar(handle_id, file_path, mime_type),
    )
}

fn matrix_remove_own_avatar(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_remove_own_avatar",
        matrix_backend::runtime::remove_own_avatar(handle_id),
    )
}

fn matrix_upload_own_room_avatar(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    file_path: &str,
    mime_type: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_upload_own_room_avatar",
        matrix_backend::runtime::upload_own_room_avatar(handle_id, room_id, file_path, mime_type),
    )
}

fn matrix_remove_own_room_avatar(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_remove_own_room_avatar",
        matrix_backend::runtime::remove_own_room_avatar(handle_id, room_id),
    )
}

fn matrix_ignore_user(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_ignore_user",
        matrix_backend::runtime::ignore_user(handle_id, user_id),
    )
}

fn matrix_unignore_user(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_unignore_user",
        matrix_backend::runtime::unignore_user(handle_id, user_id),
    )
}

fn matrix_set_invite_permission(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    target: &str,
    block: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_invite_permission",
        matrix_backend::runtime::set_invite_permission(handle_id, target, block),
    )
}

pub(crate) fn into_ffi_matrix_room_summary(
    room: matrix_backend::runtime::MatrixRoomSummary,
) -> ffi::MatrixRoomSummary {
    ffi::MatrixRoomSummary {
        room_id: room.room_id,
        latest_event_id: room.latest_event_id,
        display_name: room.display_name,
        avatar_url: room.avatar_url,
        topic: room.topic,
        last_message: room.last_message,
        last_message_kind: room.last_message_kind,
        tags: room.tags,
        parent_space_room_ids: room.parent_space_room_ids,
        direct_chat_other_user_id: room.direct_chat_other_user_id,
        is_invite: room.is_invite,
        is_space: room.is_space,
        is_direct: room.is_direct,
        is_bot_room: room.is_bot_room,
        is_encrypted: room.is_encrypted,
        is_public: room.is_public,
        member_count: room.member_count,
        unread_message_count: room.unread_message_count,
        notification_count: room.notification_count,
        highlight_count: room.highlight_count,
        timestamp: room.timestamp,
    }
}

fn into_ffi_matrix_notification_item(
    item: matrix_backend::runtime::MatrixNotificationItem,
) -> ffi::MatrixNotificationItem {
    ffi::MatrixNotificationItem {
        room_id: item.room_id,
        event_id: item.event_id,
        replacement_event_id: item.replacement_event_id,
        room_name: item.room_name,
        avatar_url: item.avatar_url,
        sender_display_name: item.sender_display_name,
        plain_body: item.plain_body,
        formatted_body: item.formatted_body,
        media_mxc_url: item.media_mxc_url,
        is_reply: item.is_reply,
        is_emote: item.is_emote,
        is_encrypted: item.is_encrypted,
        contains_spoiler: item.contains_spoiler,
        has_inline_image: item.has_inline_image,
        play_sound: item.play_sound,
    }
}

fn into_ffi_matrix_turn_server_info(
    info: matrix_backend::runtime::MatrixTurnServerInfo,
) -> ffi::MatrixTurnServerInfo {
    ffi::MatrixTurnServerInfo {
        username: info.username,
        password: info.password,
        uris: info.uris,
        ttl_seconds: info.ttl_seconds,
    }
}

fn into_ffi_matrix_image_pack_image(
    image: matrix_backend::runtime::MatrixImagePackImage,
) -> ffi::MatrixImagePackImage {
    ffi::MatrixImagePackImage {
        shortcode: image.shortcode,
        body: image.body,
        url: image.url,
        is_emote: image.is_emote,
        is_sticker: image.is_sticker,
    }
}

fn into_ffi_matrix_image_pack(
    pack: matrix_backend::runtime::MatrixImagePack,
) -> ffi::MatrixImagePack {
    ffi::MatrixImagePack {
        source_room_id: pack.source_room_id,
        state_key: pack.state_key,
        display_name: pack.display_name,
        avatar_url: pack.avatar_url,
        attribution: pack.attribution,
        is_emote_pack: pack.is_emote_pack,
        is_sticker_pack: pack.is_sticker_pack,
        from_space: pack.from_space,
        is_globally_enabled: pack.is_globally_enabled,
        images: pack
            .images
            .into_iter()
            .map(into_ffi_matrix_image_pack_image)
            .collect(),
    }
}

fn from_ffi_matrix_image_pack_image(
    image: ffi::MatrixImagePackImage,
) -> matrix_backend::runtime::MatrixImagePackImage {
    matrix_backend::runtime::MatrixImagePackImage {
        shortcode: image.shortcode,
        body: image.body,
        url: image.url,
        is_emote: image.is_emote,
        is_sticker: image.is_sticker,
    }
}

fn from_ffi_matrix_image_pack(pack: ffi::MatrixImagePack) -> matrix_backend::runtime::MatrixImagePack {
    matrix_backend::runtime::MatrixImagePack {
        source_room_id: pack.source_room_id,
        state_key: pack.state_key,
        display_name: pack.display_name,
        avatar_url: pack.avatar_url,
        attribution: pack.attribution,
        is_emote_pack: pack.is_emote_pack,
        is_sticker_pack: pack.is_sticker_pack,
        from_space: pack.from_space,
        is_globally_enabled: pack.is_globally_enabled,
        images: pack
            .images
            .into_iter()
            .map(from_ffi_matrix_image_pack_image)
            .collect(),
    }
}

fn matrix_fetch_room_list(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<Vec<ffi::MatrixRoomSummary>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_room_list",
        matrix_backend::runtime::fetch_room_list(handle_id),
    )
        .map(|rooms| rooms.into_iter().map(into_ffi_matrix_room_summary).collect())
}

fn matrix_fetch_notification_items(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    requests: &Vec<ffi::MatrixNotificationRequest>,
) -> Result<Vec<ffi::MatrixNotificationItem>, String> {
    let requests = requests
        .iter()
        .map(|request| matrix_backend::runtime::MatrixNotificationRequest {
            room_id: request.room_id.clone(),
            event_id: request.event_id.clone(),
        })
        .collect::<Vec<_>>();

    ffi_block_on(
        context,
        "matrix_fetch_notification_items",
        matrix_backend::runtime::fetch_notification_items(handle_id, &requests),
    )
    .map(|items| items.into_iter().map(into_ffi_matrix_notification_item).collect())
}

fn matrix_fetch_account_notifications_enabled(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<bool, String> {
    ffi_block_on(
        context,
        "matrix_fetch_account_notifications_enabled",
        matrix_backend::runtime::fetch_account_notifications_enabled(handle_id),
    )
}

fn matrix_fetch_turn_server_info(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixTurnServerInfo, String> {
    ffi_block_on(
        context,
        "matrix_fetch_turn_server_info",
        matrix_backend::runtime::fetch_turn_server_info(handle_id),
    )
    .map(into_ffi_matrix_turn_server_info)
}

fn matrix_set_account_notifications_enabled(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    enabled: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_account_notifications_enabled",
        matrix_backend::runtime::set_account_notifications_enabled(handle_id, enabled),
    )
}

fn matrix_fetch_image_packs(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<ffi::MatrixImagePack>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_image_packs",
        matrix_backend::runtime::fetch_image_packs(handle_id, room_id),
    )
    .map(|packs| packs.into_iter().map(into_ffi_matrix_image_pack).collect())
}

fn matrix_save_image_pack(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    state_key: &str,
    previous_state_key: &str,
    has_previous_state_key: bool,
    pack: ffi::MatrixImagePack,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_save_image_pack",
        matrix_backend::runtime::save_image_pack(
            handle_id,
            room_id,
            state_key,
            previous_state_key,
            has_previous_state_key,
            from_ffi_matrix_image_pack(pack),
        ),
    )
}

fn matrix_remove_image_pack(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    state_key: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_remove_image_pack",
        matrix_backend::runtime::remove_image_pack(handle_id, room_id, state_key),
    )
}

fn matrix_set_image_pack_globally_enabled(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    state_key: &str,
    enabled: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_image_pack_globally_enabled",
        matrix_backend::runtime::set_image_pack_globally_enabled(
            handle_id, room_id, state_key, enabled,
        ),
    )
}

fn matrix_fetch_room_settings(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<ffi::MatrixRoomSettings, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_settings",
        matrix_backend::runtime::fetch_room_settings(handle_id, room_id),
    )?;

    Ok(ffi::MatrixRoomSettings {
        room_id: result.room_id,
        room_name: result.room_name,
        room_topic: result.room_topic,
        room_avatar_url: result.room_avatar_url,
        room_version: result.room_version,
        member_count: result.member_count,
        notifications: result.notifications,
        join_rule: result.join_rule,
        history_visibility: result.history_visibility,
        allowed_room_ids: result.allowed_room_ids,
        parent_space_room_ids: result.parent_space_room_ids,
        guest_access: result.guest_access,
        is_encrypted: result.is_encrypted,
        can_change_name: result.can_change_name,
        can_change_topic: result.can_change_topic,
        can_change_avatar: result.can_change_avatar,
        can_change_join_rules: result.can_change_join_rules,
        can_change_history_visibility: result.can_change_history_visibility,
    })
}

fn matrix_fetch_room_aliases(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<ffi::MatrixRoomAliases, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_aliases",
        matrix_backend::runtime::fetch_room_aliases(handle_id, room_id),
    )?;

    Ok(ffi::MatrixRoomAliases {
        canonical_alias: result.canonical_alias,
        alt_aliases: result.alt_aliases,
        published_aliases: result.published_aliases,
    })
}

fn matrix_apply_room_aliases(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    aliases: ffi::MatrixRoomAliases,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_apply_room_aliases",
        matrix_backend::runtime::apply_room_aliases(
            handle_id,
            room_id,
            matrix_backend::runtime::MatrixRoomAliases {
                canonical_alias: aliases.canonical_alias,
                alt_aliases: aliases.alt_aliases,
                published_aliases: aliases.published_aliases,
            },
        ),
    )
}

fn matrix_fetch_room_members(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<ffi::MatrixRoomMember>, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_members",
        matrix_backend::runtime::fetch_room_members(handle_id, room_id),
    )?;
    Ok(result
        .into_iter()
        .map(|member| ffi::MatrixRoomMember {
            user_id: member.user_id,
            display_name: member.display_name,
            avatar_url: member.avatar_url,
            power_level: member.power_level,
        })
        .collect())
}

fn matrix_fetch_room_power_levels(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<ffi::MatrixRoomPowerLevels, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_power_levels",
        matrix_backend::runtime::fetch_room_power_levels(handle_id, room_id),
    )?;

    Ok(ffi::MatrixRoomPowerLevels {
        room_version: result.room_version,
        creators: result.creators,
        events: result
            .events
            .into_iter()
            .map(|entry| ffi::MatrixPowerLevelEntry { key: entry.key, level: entry.level })
            .collect(),
        users: result
            .users
            .into_iter()
            .map(|entry| ffi::MatrixPowerLevelEntry { key: entry.key, level: entry.level })
            .collect(),
        ban: result.ban,
        events_default: result.events_default,
        invite: result.invite,
        kick: result.kick,
        redact: result.redact,
        state_default: result.state_default,
        users_default: result.users_default,
    })
}

fn matrix_apply_room_power_levels(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    power_levels: ffi::MatrixRoomPowerLevels,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_apply_room_power_levels",
        matrix_backend::runtime::apply_room_power_levels(
            handle_id,
            room_id,
            matrix_backend::runtime::MatrixRoomPowerLevels {
                room_version: power_levels.room_version,
                creators: power_levels.creators,
                events: power_levels
                    .events
                    .into_iter()
                    .map(|entry| matrix_backend::runtime::MatrixPowerLevelEntry {
                        key: entry.key,
                        level: entry.level,
                    })
                    .collect(),
                users: power_levels
                    .users
                    .into_iter()
                    .map(|entry| matrix_backend::runtime::MatrixPowerLevelEntry {
                        key: entry.key,
                        level: entry.level,
                    })
                    .collect(),
                ban: power_levels.ban,
                events_default: power_levels.events_default,
                invite: power_levels.invite,
                kick: power_levels.kick,
                redact: power_levels.redact,
                state_default: power_levels.state_default,
                users_default: power_levels.users_default,
            },
        ),
    )
}

fn matrix_fetch_media_content(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    mxc_uri: &str,
    width: i32,
    height: i32,
    crop: bool,
) -> Result<Vec<u8>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_media_content",
        matrix_backend::runtime::fetch_media_content(handle_id, mxc_uri, width, height, crop),
    )
}

fn matrix_set_room_notification_mode(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    mode: i32,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_room_notification_mode",
        matrix_backend::runtime::set_room_notification_mode(handle_id, room_id, mode),
    )
}

fn matrix_set_room_name(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    name: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_room_name",
        matrix_backend::runtime::set_room_name(handle_id, room_id, name),
    )
}

fn matrix_set_room_topic(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    topic: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_room_topic",
        matrix_backend::runtime::set_room_topic(handle_id, room_id, topic),
    )
}

fn matrix_upload_room_avatar(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    file_path: &str,
    mime_type: &str,
    width: i32,
    height: i32,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_upload_room_avatar",
        matrix_backend::runtime::upload_room_avatar(
            handle_id,
            room_id,
            file_path,
            mime_type,
            width,
            height,
        ),
    )
}

fn matrix_remove_room_avatar(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_remove_room_avatar",
        matrix_backend::runtime::remove_room_avatar(handle_id, room_id),
    )
}

fn matrix_enable_room_encryption(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_enable_room_encryption",
        matrix_backend::runtime::enable_room_encryption(handle_id, room_id),
    )
}

fn matrix_set_room_history_visibility(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    history_visibility: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_room_history_visibility",
        matrix_backend::runtime::set_room_history_visibility(handle_id, room_id, history_visibility),
    )
}

fn matrix_set_room_access_rules(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    join_rule_kind: &str,
    guest_access: bool,
    allowed_room_ids: &Vec<String>,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_room_access_rules",
        matrix_backend::runtime::set_room_access_rules(
            handle_id,
            room_id,
            join_rule_kind,
            guest_access,
            allowed_room_ids,
        ),
    )
}

fn matrix_select_active_room_timeline(handle_id: u64, room_id: &str) -> Result<(), String> {

    matrix_backend::runtime::select_active_room_timeline(handle_id, room_id)
}

fn matrix_set_active_room_timeline_initial_page_size(
    handle_id: u64,
    page_size: u16,
) -> Result<(), String> {

    matrix_backend::runtime::set_active_room_timeline_initial_page_size(handle_id, page_size)
}

fn matrix_fetch_active_room_timeline(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<Vec<ffi::MatrixTimelineItem>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_active_room_timeline",
        matrix_backend::runtime::fetch_active_room_timeline(handle_id),
    )
        .map(|items| {
            items.into_iter()
                .map(|item| ffi::MatrixTimelineItem {
                    item_id: item.item_id,
                    event_id: item.event_id,
                    delivery_state: item.delivery_state,
                    thread_id: item.thread_id,
                    sender_id: item.sender_id,
                    sender_display_name: item.sender_display_name,
                    sender_avatar_url: item.sender_avatar_url,
                    body: item.body,
                    formatted_body: item.formatted_body,
                    reply_event_id: item.reply_event_id,
                    reply_sender_id: item.reply_sender_id,
                    reply_sender_display_name: item.reply_sender_display_name,
                    reply_item_kind: item.reply_item_kind,
                    reply_matrix_event_type: item.reply_matrix_event_type,
                    reply_body: item.reply_body,
                    reply_formatted_body: item.reply_formatted_body,
                    reply_media_url: item.reply_media_url,
                    reply_thumbnail_url: item.reply_thumbnail_url,
                    reply_file_name: item.reply_file_name,
                    reply_mime_type: item.reply_mime_type,
                    reply_media_width: item.reply_media_width,
                    reply_media_height: item.reply_media_height,
                    reply_media_duration_ms: item.reply_media_duration_ms,
                    reply_media_size_bytes: item.reply_media_size_bytes,
                    reactions: item
                        .reactions
                        .into_iter()
                        .map(|reaction| ffi::MatrixReactionSummary {
                            key: reaction.key,
                            users: reaction.users,
                            self_reacted_event: reaction.self_reacted_event,
                            count: reaction.count,
                        })
                        .collect(),
                    reactions_summary: item.reactions_summary,
                    special_effect_names: item.special_effect_names,
                    item_kind: item.item_kind,
                    matrix_event_type: item.matrix_event_type,
                    is_edited: item.is_edited,
                    media_url: item.media_url,
                    thumbnail_url: item.thumbnail_url,
                    file_name: item.file_name,
                    mime_type: item.mime_type,
                    media_width: item.media_width,
                    media_height: item.media_height,
                    media_duration_ms: item.media_duration_ms,
                    media_size_bytes: item.media_size_bytes,
                    media_is_encrypted: item.media_is_encrypted,
                    thumbnail_is_encrypted: item.thumbnail_is_encrypted,
                    timestamp: item.timestamp,
                    is_own: item.is_own,
                })
                .collect()
        })
}

fn matrix_paginate_active_room_timeline_backwards(
    handle_id: u64,
    page_size: u16,
) -> Result<(), String> {

    matrix_backend::runtime::paginate_active_room_timeline_backwards(handle_id, page_size)
}

fn matrix_fetch_active_room_timeline_media_content(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    item_id: &str,
    width: i32,
    height: i32,
    crop: bool,
) -> Result<Vec<u8>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_active_room_timeline_media_content",
        matrix_backend::runtime::fetch_active_room_timeline_media_content(
            handle_id, item_id, width, height, crop,
        ),
    )
}

fn matrix_send_room_message(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    body: &str,
    formatted_html: &str,
    message_kind: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_send_room_message",
        matrix_backend::runtime::send_room_message(
            handle_id,
            room_id,
            body,
            formatted_html,
            message_kind,
        ),
    )
}

fn matrix_send_room_message_like_event_json(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_type: &str,
    content_json: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_send_room_message_like_event_json",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            event_type,
            content_json,
        ),
    )
}

fn matrix_send_call_invite(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    version: &str,
    lifetime: u32,
    invitee: &str,
    offer_sdp: &str,
    offer_type: &str,
) -> Result<(), String> {
    let content_json = matrix_backend::runtime::serialize_call_invite(
        call_id, party_id, version, lifetime, invitee, offer_sdp, offer_type,
    )?;
    ffi_block_on(
        context,
        "matrix_send_call_invite",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.invite",
            &content_json,
        ),
    )
}

fn matrix_send_call_candidates(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    version: &str,
    candidates: Vec<ffi::MatrixCallIceCandidate>,
) -> Result<(), String> {
    let content_json =
        matrix_backend::runtime::serialize_call_candidates(call_id, party_id, version, &candidates)?;
    ffi_block_on(
        context,
        "matrix_send_call_candidates",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.candidates",
            &content_json,
        ),
    )
}

fn matrix_send_call_answer(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    version: &str,
    answer_sdp: &str,
    answer_type: &str,
) -> Result<(), String> {
    let content_json = matrix_backend::runtime::serialize_call_answer(
        call_id, party_id, version, answer_sdp, answer_type,
    )?;
    ffi_block_on(
        context,
        "matrix_send_call_answer",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.answer",
            &content_json,
        ),
    )
}

fn matrix_send_call_hangup(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    version: &str,
    reason: &str,
) -> Result<(), String> {
    let content_json =
        matrix_backend::runtime::serialize_call_hangup(call_id, party_id, version, reason)?;
    ffi_block_on(
        context,
        "matrix_send_call_hangup",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.hangup",
            &content_json,
        ),
    )
}

fn matrix_send_call_select_answer(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    version: &str,
    selected_party_id: &str,
) -> Result<(), String> {
    let content_json = matrix_backend::runtime::serialize_call_select_answer(
        call_id, party_id, version, selected_party_id,
    )?;
    ffi_block_on(
        context,
        "matrix_send_call_select_answer",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.select_answer",
            &content_json,
        ),
    )
}

fn matrix_send_call_reject(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    version: &str,
) -> Result<(), String> {
    let content_json =
        matrix_backend::runtime::serialize_call_reject(call_id, party_id, version)?;
    ffi_block_on(
        context,
        "matrix_send_call_reject",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.reject",
            &content_json,
        ),
    )
}

fn matrix_send_call_negotiate(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    lifetime: u32,
    description_sdp: &str,
    description_type: &str,
) -> Result<(), String> {
    let content_json = matrix_backend::runtime::serialize_call_negotiate(
        call_id, party_id, lifetime, description_sdp, description_type,
    )?;
    ffi_block_on(
        context,
        "matrix_send_call_negotiate",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.negotiate",
            &content_json,
        ),
    )
}

fn matrix_send_room_reply_message(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    replied_to_event_id: &str,
    body: &str,
    formatted_html: &str,
    message_kind: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_send_room_reply_message",
        matrix_backend::runtime::send_room_reply_message(
            handle_id,
            room_id,
            replied_to_event_id,
            body,
            formatted_html,
            message_kind,
        ),
    )
}

fn matrix_send_room_edit_message(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    target_event_id: &str,
    body: &str,
    formatted_html: &str,
    message_kind: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_send_room_edit_message",
        matrix_backend::runtime::send_room_edit_message(
            handle_id,
            room_id,
            target_event_id,
            body,
            formatted_html,
            message_kind,
        ),
    )
}

fn matrix_toggle_room_reaction(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reaction_key: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_toggle_room_reaction",
        matrix_backend::runtime::toggle_room_reaction(
            handle_id,
            room_id,
            event_id,
            reaction_key,
        ),
    )
}

fn matrix_redact_room_event(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reason: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_redact_room_event",
        matrix_backend::runtime::redact_room_event(handle_id, room_id, event_id, reason),
    )
}

fn matrix_mark_room_event_as_read(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_mark_room_event_as_read",
        matrix_backend::runtime::mark_room_event_as_read(handle_id, room_id, event_id),
    )
}

fn matrix_report_room_event(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reason: &str,
    score: i32,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_report_room_event",
        matrix_backend::runtime::report_room_event(handle_id, room_id, event_id, reason, score),
    )
}

fn matrix_fetch_room_pinned_event_ids(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<String>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_room_pinned_event_ids",
        matrix_backend::runtime::fetch_room_pinned_event_ids(handle_id, room_id),
    )
}

fn matrix_pin_room_event(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_pin_room_event",
        matrix_backend::runtime::pin_room_event(handle_id, room_id, event_id),
    )
}

fn matrix_unpin_room_event(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_unpin_room_event",
        matrix_backend::runtime::unpin_room_event(handle_id, room_id, event_id),
    )
}

fn matrix_fetch_active_room_raw_event_dialog_data(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<ffi::MatrixRawEventDialogData, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_active_room_raw_event_dialog_data",
        matrix_backend::runtime::fetch_active_room_raw_event_dialog_data(
            handle_id, room_id, event_id,
        ),
    )?;

    Ok(ffi::MatrixRawEventDialogData {
        pretty_json: result.pretty_json,
        body: result.body,
        formatted_body: result.formatted_body,
    })
}

fn matrix_fetch_active_room_event_content_for_forwarding(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<ffi::MatrixEventContentForForwarding, String> {
    let (event_type, content_json) = ffi_block_on(
        context,
        "matrix_fetch_active_room_event_content_for_forwarding",
        matrix_backend::runtime::fetch_active_room_event_content_for_forwarding(
            handle_id, room_id, event_id,
        ),
    )?;

    Ok(ffi::MatrixEventContentForForwarding {
        event_type,
        content_json,
    })
}

fn matrix_fetch_room_read_receipts(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<Vec<ffi::MatrixReadReceiptEntry>, String> {
    Ok(ffi_block_on(
        context,
        "matrix_fetch_room_read_receipts",
        matrix_backend::runtime::fetch_room_read_receipts(handle_id, room_id, event_id),
    )?
        .into_iter()
        .map(|entry| ffi::MatrixReadReceiptEntry {
            user_id: entry.user_id,
            display_name: entry.display_name,
            avatar_url: entry.avatar_url,
            timestamp: entry.timestamp,
        })
        .collect())
}

fn matrix_fetch_room_redaction_permissions(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<ffi::MatrixRoomRedactionPermissions, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_redaction_permissions",
        matrix_backend::runtime::fetch_room_redaction_permissions(handle_id, room_id),
    )?;

    Ok(ffi::MatrixRoomRedactionPermissions {
        can_redact_own: result.can_redact_own,
        can_redact_other: result.can_redact_other,
    })
}

fn matrix_send_room_attachment(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    file_path: &str,
    filename: &str,
    caption: &str,
    reply_event_id: &str,
    mime_type: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_send_room_attachment",
        matrix_backend::runtime::send_room_attachment(
            handle_id,
            room_id,
            file_path,
            filename,
            caption,
            reply_event_id,
            mime_type,
        ),
    )
}

fn matrix_upload_media(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    file_path: &str,
    mime_type: &str,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_upload_media",
        matrix_backend::runtime::upload_media(handle_id, file_path, mime_type),
    )
}

fn matrix_send_room_image(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    mxc_uri: &str,
    body: &str,
    filename: &str,
    info_json: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_send_room_image",
        matrix_backend::runtime::send_room_image(handle_id, room_id, mxc_uri, body, filename, info_json),
    )
}

fn matrix_discover_login_flows(
    context: ffi::MatrixFfiBlockingContext,
    server_name_or_url: &str,
    verify_certificates: bool,
) -> Result<ffi::MatrixLoginFlows, String> {
    let result = ffi_block_on(
        context,
        "matrix_discover_login_flows",
        matrix_backend::auth::discover_login_flows(server_name_or_url, verify_certificates),
    )?;

    Ok(ffi::MatrixLoginFlows {
        homeserver_url: result.homeserver_url,
        password_supported: result.password_supported,
        sso_supported: result.sso_supported,
        oauth_supported: result.oauth_supported,
        identity_providers: result
            .identity_providers
            .into_iter()
            .map(|provider| ffi::MatrixLoginIdentityProvider {
                id: provider.id,
                name: provider.name,
                icon: provider.icon,
                brand: provider.brand,
            })
            .collect(),
    })
}

fn matrix_get_sso_login_url(
    context: ffi::MatrixFfiBlockingContext,
    homeserver_url: &str,
    redirect_url: &str,
    identity_provider_id: &str,
    verify_certificates: bool,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_get_sso_login_url",
        matrix_backend::auth::get_sso_login_url(
            homeserver_url,
            redirect_url,
            identity_provider_id,
            verify_certificates,
        ),
    )
}

fn matrix_start_sso_callback_server(
    success_html: &str,
    failure_html: &str,
    timeout_ms: u32,
) -> Result<ffi::MatrixSsoCallbackServer, String> {

    let result =
        matrix_backend::auth::start_sso_callback_server(success_html, failure_html, timeout_ms)?;

    Ok(ffi::MatrixSsoCallbackServer {
        listener_id: result.listener_id,
        callback_url: result.callback_url,
    })
}

fn matrix_poll_sso_callback_server(
    listener_id: u64,
) -> Result<ffi::MatrixSsoCallbackStatus, String> {

    let result = matrix_backend::auth::poll_sso_callback_server(listener_id)?;

    Ok(ffi::MatrixSsoCallbackStatus {
        ready: result.ready,
        success: result.success,
        login_token: result.login_token,
        callback_query: result.callback_query,
    })
}

fn matrix_stop_sso_callback_server(listener_id: u64) -> Result<(), String> {

    matrix_backend::auth::stop_sso_callback_server(listener_id)
}

fn matrix_start_oauth_login(
    context: ffi::MatrixFfiBlockingContext,
    profile_id: &str,
    homeserver_url: &str,
    redirect_url: &str,
    user_id_hint: &str,
    device_id: &str,
    initial_device_display_name: &str,
    verify_certificates: bool,
) -> Result<ffi::MatrixOauthLoginStartResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_oauth_login",
        matrix_backend::auth::start_oauth_login(
            profile_id,
            homeserver_url,
            redirect_url,
            user_id_hint,
            device_id,
            initial_device_display_name,
            verify_certificates,
        ),
    )?;

    Ok(ffi::MatrixOauthLoginStartResult {
        login_id: result.login_id,
        login_url: result.login_url,
    })
}

fn matrix_finish_oauth_login(
    context: ffi::MatrixFfiBlockingContext,
    login_id: u64,
    callback_query: &str,
) -> Result<ffi::MatrixLoginResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_finish_oauth_login",
        matrix_backend::auth::finish_oauth_login(login_id, callback_query),
    )?;

    Ok(ffi::MatrixLoginResult {
        user_id: result.user_id,
        access_token: result.access_token,
        device_id: result.device_id,
        homeserver_url: result.homeserver_url,
    })
}

fn matrix_cancel_oauth_login(login_id: u64) -> Result<(), String> {
    matrix_backend::auth::cancel_oauth_login(login_id)
}

fn matrix_login_password(
    context: ffi::MatrixFfiBlockingContext,
    profile_id: &str,
    homeserver_url: &str,
    user_id: &str,
    password: &str,
    device_id: &str,
    initial_device_display_name: &str,
    verify_certificates: bool,
) -> Result<ffi::MatrixLoginResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_login_password",
        matrix_backend::auth::login_password(
            profile_id,
            homeserver_url,
            user_id,
            password,
            device_id,
            initial_device_display_name,
            verify_certificates,
        ),
    )?;

    Ok(ffi::MatrixLoginResult {
        user_id: result.user_id,
        access_token: result.access_token,
        device_id: result.device_id,
        homeserver_url: result.homeserver_url,
    })
}

fn matrix_login_token(
    context: ffi::MatrixFfiBlockingContext,
    profile_id: &str,
    homeserver_url: &str,
    login_token: &str,
    device_id: &str,
    initial_device_display_name: &str,
    verify_certificates: bool,
) -> Result<ffi::MatrixLoginResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_login_token",
        matrix_backend::auth::login_token(
            profile_id,
            homeserver_url,
            login_token,
            device_id,
            initial_device_display_name,
            verify_certificates,
        ),
    )?;

    Ok(ffi::MatrixLoginResult {
        user_id: result.user_id,
        access_token: result.access_token,
        device_id: result.device_id,
        homeserver_url: result.homeserver_url,
    })
}
