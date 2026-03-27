// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::sync::OnceLock;

use resolvematrix::server::MatrixResolver;
use tokio::runtime::Runtime;

pub mod ipc;
pub mod logging;
pub mod matrix_backend;
pub mod mcp;

#[cxx::bridge(namespace = "komai::rust")]
mod ffi {
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
        display_name: String,
        avatar_url: String,
        topic: String,
        last_message: String,
        last_message_kind: String,
        direct_chat_other_user_id: String,
        is_invite: bool,
        is_space: bool,
        is_direct: bool,
        is_bot_room: bool,
        is_encrypted: bool,
        is_public: bool,
        unread_message_count: u64,
        notification_count: u64,
        highlight_count: u64,
        timestamp: u64,
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

    struct MatrixRoomRedactionPermissions {
        can_redact_own: bool,
        can_redact_other: bool,
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
        thread_id: String,
        sender_id: String,
        sender_display_name: String,
        sender_avatar_url: String,
        body: String,
        reply_event_id: String,
        reply_sender_id: String,
        reply_sender_display_name: String,
        reply_body: String,
        reactions: Vec<MatrixReactionSummary>,
        reactions_summary: String,
        item_kind: String,
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

    unsafe extern "C++" {
        include!("matrix/backend/MatrixBackendBridge.h");

        #[namespace = "komai::rust_bridge"]
        fn matrix_profile_data_root(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn matrix_profile_cache_root(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn matrix_store_passphrase(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn matrix_homeserver_url(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn matrix_serialized_session(profile_id: &str) -> String;
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
        fn matrix_log_event(
            level: &str,
            target: &str,
            module_path: &str,
            file: &str,
            line: u32,
            message: &str,
        );
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_room_list_snapshot_updated(handle_id: u64);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_initial_sync_ready(handle_id: u64);
        #[namespace = "komai::rust_bridge"]
        fn matrix_notify_room_timeline_snapshot_updated(handle_id: u64, room_id: &str);
    }

    extern "Rust" {
        fn resolve_server(server_name: &str) -> Result<ResolveResult>;
        fn matrix_sdk_paths(profile_id: &str) -> MatrixSdkPaths;
        fn matrix_restore_session_preview(profile_id: &str) -> Result<MatrixRestorePreview>;
        fn matrix_start_restored_backend(profile_id: &str) -> Result<MatrixBackendHandleInfo>;
        fn matrix_logout_backend(handle_id: u64) -> Result<()>;
        fn matrix_stop_backend(handle_id: u64) -> Result<()>;
        fn matrix_start_backend_sync(handle_id: u64) -> Result<()>;
        fn matrix_join_room(
            handle_id: u64,
            room_id_or_alias: &str,
            via: &Vec<String>,
            reason: &str,
        ) -> MatrixJoinRoomResult;
        fn matrix_knock_room(
            handle_id: u64,
            room_id_or_alias: &str,
            via: &Vec<String>,
            reason: &str,
        ) -> Result<String>;
        fn matrix_create_room(
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
        fn matrix_leave_room(handle_id: u64, room_id: &str, reason: &str) -> Result<()>;
        fn matrix_invite_user(
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_kick_user(
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_ban_user(
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_unban_user(
            handle_id: u64,
            room_id: &str,
            user_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_fetch_own_profile(handle_id: u64) -> Result<MatrixOwnProfile>;
        fn matrix_fetch_recovery_status(handle_id: u64) -> Result<MatrixRecoveryStatus>;
        fn matrix_setup_recovery(
            handle_id: u64,
            use_ssss: bool,
            passphrase: &str,
            encryption_backup_online_enabled: bool,
        ) -> Result<MatrixSetupRecoveryResult>;
        fn matrix_recover_encryption_secrets(
            handle_id: u64,
            key_or_passphrase: &str,
        ) -> Result<()>;
        fn matrix_start_reset_encryption_identity(
            handle_id: u64,
        ) -> Result<MatrixResetEncryptionIdentityResult>;
        fn matrix_continue_reset_encryption_identity_with_password(
            handle_id: u64,
            password: &str,
        ) -> Result<()>;
        fn matrix_continue_reset_encryption_identity_after_approval(
            handle_id: u64,
        ) -> Result<()>;
        fn matrix_cancel_reset_encryption_identity(handle_id: u64) -> Result<()>;
        fn matrix_start_sign_out_device(
            handle_id: u64,
            device_id: &str,
        ) -> Result<MatrixDeviceSignOutResult>;
        fn matrix_continue_sign_out_device_with_password(
            handle_id: u64,
            password: &str,
        ) -> Result<()>;
        fn matrix_rename_device(
            handle_id: u64,
            device_id: &str,
            display_name: &str,
        ) -> Result<()>;
        fn matrix_start_self_verification(handle_id: u64) -> Result<MatrixVerificationSession>;
        fn matrix_start_user_verification(
            handle_id: u64,
            user_id: &str,
        ) -> Result<MatrixVerificationSession>;
        fn matrix_start_device_verification(
            handle_id: u64,
            user_id: &str,
            device_id: &str,
        ) -> Result<MatrixVerificationSession>;
        fn matrix_unverify_device(handle_id: u64, user_id: &str, device_id: &str) -> Result<()>;
        fn matrix_block_device(handle_id: u64, user_id: &str, device_id: &str) -> Result<()>;
        fn matrix_unblock_device(handle_id: u64, user_id: &str, device_id: &str) -> Result<()>;
        fn matrix_fetch_user_verification_state(
            handle_id: u64,
            user_id: &str,
        ) -> Result<MatrixUserVerificationState>;
        fn matrix_take_pending_verification_flow_ids(handle_id: u64) -> Result<Vec<String>>;
        fn matrix_fetch_verification_session(
            handle_id: u64,
            flow_id: &str,
        ) -> Result<MatrixVerificationSession>;
        fn matrix_clear_verification_session(handle_id: u64, flow_id: &str) -> Result<()>;
        fn matrix_advance_verification_session(handle_id: u64, flow_id: &str) -> Result<()>;
        fn matrix_cancel_verification_session(
            handle_id: u64,
            flow_id: &str,
            mismatch: bool,
        ) -> Result<()>;
        fn matrix_fetch_user_profile(handle_id: u64, user_id: &str) -> Result<MatrixUserProfile>;
        fn matrix_search_users(
            handle_id: u64,
            search_term: &str,
            limit: u64,
        ) -> Result<Vec<MatrixDirectoryUser>>;
        fn matrix_fetch_public_room_directory_page(
            handle_id: u64,
            search_term: &str,
            limit: u64,
            since: &str,
            server: &str,
        ) -> Result<MatrixPublicRoomDirectoryPage>;
        fn matrix_set_own_display_name(handle_id: u64, display_name: &str) -> Result<()>;
        fn matrix_upload_own_avatar(
            handle_id: u64,
            file_path: &str,
            mime_type: &str,
        ) -> Result<()>;
        fn matrix_remove_own_avatar(handle_id: u64) -> Result<()>;
        fn matrix_ignore_user(handle_id: u64, user_id: &str) -> Result<()>;
        fn matrix_unignore_user(handle_id: u64, user_id: &str) -> Result<()>;
        fn matrix_fetch_room_list(handle_id: u64) -> Result<Vec<MatrixRoomSummary>>;
        fn matrix_fetch_room_settings(handle_id: u64, room_id: &str) -> Result<MatrixRoomSettings>;
        fn matrix_fetch_media_content(
            handle_id: u64,
            mxc_uri: &str,
            width: i32,
            height: i32,
            crop: bool,
        ) -> Result<Vec<u8>>;
        fn matrix_set_room_notification_mode(
            handle_id: u64,
            room_id: &str,
            mode: i32,
        ) -> Result<()>;
        fn matrix_set_room_name(handle_id: u64, room_id: &str, name: &str) -> Result<()>;
        fn matrix_set_room_topic(handle_id: u64, room_id: &str, topic: &str) -> Result<()>;
        fn matrix_upload_room_avatar(
            handle_id: u64,
            room_id: &str,
            file_path: &str,
            mime_type: &str,
            width: i32,
            height: i32,
        ) -> Result<()>;
        fn matrix_remove_room_avatar(handle_id: u64, room_id: &str) -> Result<()>;
        fn matrix_enable_room_encryption(handle_id: u64, room_id: &str) -> Result<()>;
        fn matrix_set_room_history_visibility(
            handle_id: u64,
            room_id: &str,
            history_visibility: &str,
        ) -> Result<()>;
        fn matrix_set_room_access_rules(
            handle_id: u64,
            room_id: &str,
            join_rule_kind: &str,
            guest_access: bool,
            allowed_room_ids: &Vec<String>,
        ) -> Result<()>;
        fn matrix_select_active_room_timeline(handle_id: u64, room_id: &str) -> Result<()>;
        fn matrix_fetch_active_room_timeline(handle_id: u64) -> Result<Vec<MatrixTimelineItem>>;
        fn matrix_paginate_active_room_timeline_backwards(
            handle_id: u64,
            page_size: u16,
        ) -> Result<()>;
        fn matrix_fetch_active_room_timeline_media_content(
            handle_id: u64,
            item_id: &str,
            width: i32,
            height: i32,
            crop: bool,
        ) -> Result<Vec<u8>>;
        fn matrix_send_room_message(
            handle_id: u64,
            room_id: &str,
            body: &str,
            formatted_html: &str,
            message_kind: &str,
        ) -> Result<()>;
        fn matrix_send_room_reply_message(
            handle_id: u64,
            room_id: &str,
            replied_to_event_id: &str,
            body: &str,
            formatted_html: &str,
            message_kind: &str,
        ) -> Result<()>;
        fn matrix_send_room_edit_message(
            handle_id: u64,
            room_id: &str,
            target_event_id: &str,
            body: &str,
            formatted_html: &str,
            message_kind: &str,
        ) -> Result<()>;
        fn matrix_toggle_room_reaction(
            handle_id: u64,
            room_id: &str,
            event_id: &str,
            reaction_key: &str,
        ) -> Result<()>;
        fn matrix_redact_room_event(
            handle_id: u64,
            room_id: &str,
            event_id: &str,
            reason: &str,
        ) -> Result<()>;
        fn matrix_mark_room_event_as_read(
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<()>;
        fn matrix_report_room_event(
            handle_id: u64,
            room_id: &str,
            event_id: &str,
            reason: &str,
            score: i32,
        ) -> Result<()>;
        fn matrix_fetch_room_pinned_event_ids(
            handle_id: u64,
            room_id: &str,
        ) -> Result<Vec<String>>;
        fn matrix_pin_room_event(handle_id: u64, room_id: &str, event_id: &str) -> Result<()>;
        fn matrix_unpin_room_event(handle_id: u64, room_id: &str, event_id: &str) -> Result<()>;
        fn matrix_fetch_active_room_raw_event_json(
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<String>;
        fn matrix_fetch_room_read_receipts(
            handle_id: u64,
            room_id: &str,
            event_id: &str,
        ) -> Result<Vec<MatrixReadReceiptEntry>>;
        fn matrix_fetch_room_redaction_permissions(
            handle_id: u64,
            room_id: &str,
        ) -> Result<MatrixRoomRedactionPermissions>;
        fn matrix_send_room_attachment(
            handle_id: u64,
            room_id: &str,
            file_path: &str,
            filename: &str,
            caption: &str,
            reply_event_id: &str,
            mime_type: &str,
        ) -> Result<()>;
        fn matrix_discover_login_flows(
            server_name_or_url: &str,
            verify_certificates: bool,
        ) -> Result<MatrixLoginFlows>;
        fn matrix_get_sso_login_url(
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
            profile_id: &str,
            homeserver_url: &str,
            redirect_url: &str,
            user_id_hint: &str,
            device_id: &str,
            initial_device_display_name: &str,
            verify_certificates: bool,
        ) -> Result<MatrixOauthLoginStartResult>;
        fn matrix_finish_oauth_login(
            login_id: u64,
            callback_query: &str,
        ) -> Result<MatrixLoginResult>;
        fn matrix_cancel_oauth_login(login_id: u64) -> Result<()>;
        fn matrix_login_password(
            profile_id: &str,
            homeserver_url: &str,
            user_id: &str,
            password: &str,
            device_id: &str,
            initial_device_display_name: &str,
            verify_certificates: bool,
        ) -> Result<MatrixLoginResult>;
        fn matrix_login_token(
            profile_id: &str,
            homeserver_url: &str,
            login_token: &str,
            device_id: &str,
            initial_device_display_name: &str,
            verify_certificates: bool,
        ) -> Result<MatrixLoginResult>;
    }
}

fn runtime() -> &'static Runtime {
    static RT: OnceLock<Runtime> = OnceLock::new();
    logging::ensure_initialized();
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

fn resolve_server(server_name: &str) -> Result<ffi::ResolveResult, String> {
    let resolution = runtime()
        .block_on(resolver().resolve_server(server_name))
        .map_err(|e| format!("failed to resolve server '{}': {}", server_name, e))?;

    Ok(ffi::ResolveResult {
        base_url: resolution.base_url(),
    })
}

fn matrix_sdk_paths(profile_id: &str) -> ffi::MatrixSdkPaths {
    logging::ensure_initialized();
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

fn matrix_restore_session_preview(profile_id: &str) -> Result<ffi::MatrixRestorePreview, String> {
    let preview = runtime().block_on(matrix_backend::bootstrap::restore_session_preview(
        profile_id,
    ))?;

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

fn matrix_start_restored_backend(profile_id: &str) -> Result<ffi::MatrixBackendHandleInfo, String> {
    let result = runtime().block_on(matrix_backend::runtime::start_restored_backend(profile_id))?;

    Ok(ffi::MatrixBackendHandleInfo {
        handle_id: result.handle_id,
        has_session: result.has_session,
        auth_type: result.auth_type,
        homeserver_url: result.homeserver_url,
        user_id: result.user_id,
        device_id: result.device_id,
    })
}

fn matrix_logout_backend(handle_id: u64) -> Result<(), String> {
    logging::ensure_initialized();
    runtime().block_on(matrix_backend::runtime::logout_backend(handle_id))
}

fn matrix_stop_backend(handle_id: u64) -> Result<(), String> {
    logging::ensure_initialized();
    matrix_backend::runtime::stop_backend(handle_id)
}

fn matrix_start_backend_sync(handle_id: u64) -> Result<(), String> {
    logging::ensure_initialized();
    matrix_backend::runtime::start_sync(handle_id)
}

fn matrix_join_room(
    handle_id: u64,
    room_id_or_alias: &str,
    via: &Vec<String>,
    reason: &str,
) -> ffi::MatrixJoinRoomResult {
    logging::ensure_initialized();
    match runtime().block_on(matrix_backend::runtime::join_room(
        handle_id,
        room_id_or_alias,
        via.as_slice(),
        reason,
    )) {
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
    handle_id: u64,
    room_id_or_alias: &str,
    via: &Vec<String>,
    reason: &str,
) -> Result<String, String> {
    runtime().block_on(matrix_backend::runtime::knock_room(
        handle_id,
        room_id_or_alias,
        via.as_slice(),
        reason,
    ))
}

#[allow(clippy::too_many_arguments)]
fn matrix_create_room(
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
    runtime().block_on(matrix_backend::runtime::create_room(
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
    ))
}

fn matrix_leave_room(handle_id: u64, room_id: &str, reason: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::leave_room(handle_id, room_id, reason))
}

fn matrix_invite_user(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::invite_user(
        handle_id, room_id, user_id, reason,
    ))
}

fn matrix_kick_user(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::kick_user(
        handle_id, room_id, user_id, reason,
    ))
}

fn matrix_ban_user(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::ban_user(
        handle_id, room_id, user_id, reason,
    ))
}

fn matrix_unban_user(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::unban_user(
        handle_id, room_id, user_id, reason,
    ))
}

fn matrix_fetch_own_profile(handle_id: u64) -> Result<ffi::MatrixOwnProfile, String> {
    let result = runtime().block_on(matrix_backend::runtime::fetch_own_profile(handle_id))?;

    Ok(ffi::MatrixOwnProfile {
        display_name: result.display_name,
        avatar_url: result.avatar_url,
    })
}

fn matrix_fetch_recovery_status(handle_id: u64) -> Result<ffi::MatrixRecoveryStatus, String> {
    let result = runtime().block_on(matrix_backend::runtime::fetch_recovery_status(handle_id))?;

    Ok(ffi::MatrixRecoveryStatus {
        state: result.state,
        has_devices_to_verify_against: result.has_devices_to_verify_against,
        own_device_is_verified: result.own_device_is_verified,
        has_unverified_own_devices: result.has_unverified_own_devices,
    })
}

fn matrix_setup_recovery(
    handle_id: u64,
    use_ssss: bool,
    passphrase: &str,
    encryption_backup_online_enabled: bool,
) -> Result<ffi::MatrixSetupRecoveryResult, String> {
    let result = runtime().block_on(matrix_backend::runtime::setup_recovery(
        handle_id,
        use_ssss,
        passphrase,
        encryption_backup_online_enabled,
    ))?;

    Ok(ffi::MatrixSetupRecoveryResult {
        recovery_key: result.recovery_key,
    })
}

fn matrix_recover_encryption_secrets(
    handle_id: u64,
    key_or_passphrase: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::recover_encryption_secrets(
        handle_id,
        key_or_passphrase,
    ))
}

fn matrix_start_reset_encryption_identity(
    handle_id: u64,
) -> Result<ffi::MatrixResetEncryptionIdentityResult, String> {
    let result = runtime().block_on(matrix_backend::runtime::start_reset_encryption_identity(
        handle_id,
    ))?;

    Ok(ffi::MatrixResetEncryptionIdentityResult {
        completed: result.completed,
        auth_type: result.auth_type,
        approval_url: result.approval_url,
    })
}

fn matrix_continue_reset_encryption_identity_with_password(
    handle_id: u64,
    password: &str,
) -> Result<(), String> {
    runtime().block_on(
        matrix_backend::runtime::continue_reset_encryption_identity_with_password(
            handle_id, password,
        ),
    )
}

fn matrix_continue_reset_encryption_identity_after_approval(
    handle_id: u64,
) -> Result<(), String> {
    runtime().block_on(
        matrix_backend::runtime::continue_reset_encryption_identity_after_approval(handle_id),
    )
}

fn matrix_cancel_reset_encryption_identity(handle_id: u64) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::cancel_reset_encryption_identity(handle_id))
}

fn matrix_start_sign_out_device(
    handle_id: u64,
    device_id: &str,
) -> Result<ffi::MatrixDeviceSignOutResult, String> {
    let result =
        runtime().block_on(matrix_backend::runtime::start_sign_out_device(handle_id, device_id))?;

    Ok(ffi::MatrixDeviceSignOutResult {
        completed: result.completed,
        auth_type: result.auth_type,
        approval_url: result.approval_url,
    })
}

fn matrix_continue_sign_out_device_with_password(
    handle_id: u64,
    password: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::continue_sign_out_device_with_password(
        handle_id, password,
    ))
}

fn matrix_rename_device(handle_id: u64, device_id: &str, display_name: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::rename_device(
        handle_id,
        device_id,
        display_name,
    ))
}

fn matrix_start_self_verification(
    handle_id: u64,
) -> Result<ffi::MatrixVerificationSession, String> {
    let result = runtime().block_on(matrix_backend::runtime::start_self_verification(handle_id))?;

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
    handle_id: u64,
    user_id: &str,
) -> Result<ffi::MatrixVerificationSession, String> {
    let result =
        runtime().block_on(matrix_backend::runtime::start_user_verification(handle_id, user_id))?;

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
    handle_id: u64,
    user_id: &str,
    device_id: &str,
) -> Result<ffi::MatrixVerificationSession, String> {
    let result = runtime().block_on(matrix_backend::runtime::start_device_verification(
        handle_id, user_id, device_id,
    ))?;

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

fn matrix_unverify_device(handle_id: u64, user_id: &str, device_id: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::unverify_device(
        handle_id,
        user_id,
        device_id,
    ))
}

fn matrix_block_device(handle_id: u64, user_id: &str, device_id: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::block_device(
        handle_id,
        user_id,
        device_id,
    ))
}

fn matrix_unblock_device(handle_id: u64, user_id: &str, device_id: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::unblock_device(
        handle_id,
        user_id,
        device_id,
    ))
}

fn matrix_fetch_user_verification_state(
    handle_id: u64,
    user_id: &str,
) -> Result<ffi::MatrixUserVerificationState, String> {
    let result =
        runtime().block_on(matrix_backend::runtime::fetch_user_verification_state(handle_id, user_id))?;

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

fn matrix_take_pending_verification_flow_ids(handle_id: u64) -> Result<Vec<String>, String> {
    Ok(runtime().block_on(async move {
        matrix_backend::runtime::take_pending_verification_flow_ids(handle_id)
    })?)
}

fn matrix_fetch_verification_session(
    handle_id: u64,
    flow_id: &str,
) -> Result<ffi::MatrixVerificationSession, String> {
    let result =
        runtime().block_on(matrix_backend::runtime::fetch_verification_session(handle_id, flow_id))?;

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

fn matrix_clear_verification_session(handle_id: u64, flow_id: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::clear_verification_session(handle_id, flow_id))
}

fn matrix_advance_verification_session(handle_id: u64, flow_id: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::advance_verification_session(handle_id, flow_id))
}

fn matrix_cancel_verification_session(
    handle_id: u64,
    flow_id: &str,
    mismatch: bool,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::cancel_verification_session(
        handle_id, flow_id, mismatch,
    ))
}

fn matrix_fetch_user_profile(handle_id: u64, user_id: &str) -> Result<ffi::MatrixUserProfile, String> {
    let result = runtime().block_on(matrix_backend::runtime::fetch_user_profile(handle_id, user_id))?;

    Ok(ffi::MatrixUserProfile {
        display_name: result.display_name,
        avatar_url: result.avatar_url,
    })
}

fn matrix_search_users(
    handle_id: u64,
    search_term: &str,
    limit: u64,
) -> Result<Vec<ffi::MatrixDirectoryUser>, String> {
    runtime()
        .block_on(matrix_backend::runtime::search_users(
            handle_id,
            search_term,
            limit,
        ))
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
    handle_id: u64,
    search_term: &str,
    limit: u64,
    since: &str,
    server: &str,
) -> Result<ffi::MatrixPublicRoomDirectoryPage, String> {
    runtime()
        .block_on(matrix_backend::runtime::fetch_public_room_directory_page(
            handle_id,
            search_term,
            limit,
            since,
            server,
        ))
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

fn matrix_set_own_display_name(handle_id: u64, display_name: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::set_own_display_name(handle_id, display_name))
}

fn matrix_upload_own_avatar(handle_id: u64, file_path: &str, mime_type: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::upload_own_avatar(
        handle_id, file_path, mime_type,
    ))
}

fn matrix_remove_own_avatar(handle_id: u64) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::remove_own_avatar(handle_id))
}

fn matrix_ignore_user(handle_id: u64, user_id: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::ignore_user(handle_id, user_id))
}

fn matrix_unignore_user(handle_id: u64, user_id: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::unignore_user(handle_id, user_id))
}

fn matrix_fetch_room_list(handle_id: u64) -> Result<Vec<ffi::MatrixRoomSummary>, String> {
    runtime()
        .block_on(matrix_backend::runtime::fetch_room_list(handle_id))
        .map(|rooms| {
            rooms.into_iter()
                .map(|room| ffi::MatrixRoomSummary {
                    room_id: room.room_id,
                    display_name: room.display_name,
                    avatar_url: room.avatar_url,
                    topic: room.topic,
                    last_message: room.last_message,
                    last_message_kind: room.last_message_kind,
                    direct_chat_other_user_id: room.direct_chat_other_user_id,
                    is_invite: room.is_invite,
                    is_space: room.is_space,
                    is_direct: room.is_direct,
                    is_bot_room: room.is_bot_room,
                    is_encrypted: room.is_encrypted,
                    is_public: room.is_public,
                    unread_message_count: room.unread_message_count,
                    notification_count: room.notification_count,
                    highlight_count: room.highlight_count,
                    timestamp: room.timestamp,
                })
                .collect()
        })
}

fn matrix_fetch_room_settings(
    handle_id: u64,
    room_id: &str,
) -> Result<ffi::MatrixRoomSettings, String> {
    let result = runtime().block_on(matrix_backend::runtime::fetch_room_settings(handle_id, room_id))?;

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

fn matrix_fetch_media_content(
    handle_id: u64,
    mxc_uri: &str,
    width: i32,
    height: i32,
    crop: bool,
) -> Result<Vec<u8>, String> {
    runtime().block_on(matrix_backend::runtime::fetch_media_content(
        handle_id, mxc_uri, width, height, crop,
    ))
}

fn matrix_set_room_notification_mode(
    handle_id: u64,
    room_id: &str,
    mode: i32,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::set_room_notification_mode(
        handle_id, room_id, mode,
    ))
}

fn matrix_set_room_name(handle_id: u64, room_id: &str, name: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::set_room_name(handle_id, room_id, name))
}

fn matrix_set_room_topic(handle_id: u64, room_id: &str, topic: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::set_room_topic(handle_id, room_id, topic))
}

fn matrix_upload_room_avatar(
    handle_id: u64,
    room_id: &str,
    file_path: &str,
    mime_type: &str,
    width: i32,
    height: i32,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::upload_room_avatar(
        handle_id,
        room_id,
        file_path,
        mime_type,
        width,
        height,
    ))
}

fn matrix_remove_room_avatar(handle_id: u64, room_id: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::remove_room_avatar(handle_id, room_id))
}

fn matrix_enable_room_encryption(handle_id: u64, room_id: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::enable_room_encryption(handle_id, room_id))
}

fn matrix_set_room_history_visibility(
    handle_id: u64,
    room_id: &str,
    history_visibility: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::set_room_history_visibility(
        handle_id,
        room_id,
        history_visibility,
    ))
}

fn matrix_set_room_access_rules(
    handle_id: u64,
    room_id: &str,
    join_rule_kind: &str,
    guest_access: bool,
    allowed_room_ids: &Vec<String>,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::set_room_access_rules(
        handle_id,
        room_id,
        join_rule_kind,
        guest_access,
        allowed_room_ids,
    ))
}

fn matrix_select_active_room_timeline(handle_id: u64, room_id: &str) -> Result<(), String> {
    logging::ensure_initialized();
    matrix_backend::runtime::select_active_room_timeline(handle_id, room_id)
}

fn matrix_fetch_active_room_timeline(
    handle_id: u64,
) -> Result<Vec<ffi::MatrixTimelineItem>, String> {
    runtime()
        .block_on(matrix_backend::runtime::fetch_active_room_timeline(handle_id))
        .map(|items| {
            items.into_iter()
                .map(|item| ffi::MatrixTimelineItem {
                    item_id: item.item_id,
                    event_id: item.event_id,
                    thread_id: item.thread_id,
                    sender_id: item.sender_id,
                    sender_display_name: item.sender_display_name,
                    sender_avatar_url: item.sender_avatar_url,
                    body: item.body,
                    reply_event_id: item.reply_event_id,
                    reply_sender_id: item.reply_sender_id,
                    reply_sender_display_name: item.reply_sender_display_name,
                    reply_body: item.reply_body,
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
                    item_kind: item.item_kind,
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
    logging::ensure_initialized();
    matrix_backend::runtime::paginate_active_room_timeline_backwards(handle_id, page_size)
}

fn matrix_fetch_active_room_timeline_media_content(
    handle_id: u64,
    item_id: &str,
    width: i32,
    height: i32,
    crop: bool,
) -> Result<Vec<u8>, String> {
    runtime().block_on(matrix_backend::runtime::fetch_active_room_timeline_media_content(
        handle_id, item_id, width, height, crop,
    ))
}

fn matrix_send_room_message(
    handle_id: u64,
    room_id: &str,
    body: &str,
    formatted_html: &str,
    message_kind: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::send_room_message(
        handle_id,
        room_id,
        body,
        formatted_html,
        message_kind,
    ))
}

fn matrix_send_room_reply_message(
    handle_id: u64,
    room_id: &str,
    replied_to_event_id: &str,
    body: &str,
    formatted_html: &str,
    message_kind: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::send_room_reply_message(
        handle_id,
        room_id,
        replied_to_event_id,
        body,
        formatted_html,
        message_kind,
    ))
}

fn matrix_send_room_edit_message(
    handle_id: u64,
    room_id: &str,
    target_event_id: &str,
    body: &str,
    formatted_html: &str,
    message_kind: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::send_room_edit_message(
        handle_id,
        room_id,
        target_event_id,
        body,
        formatted_html,
        message_kind,
    ))
}

fn matrix_toggle_room_reaction(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reaction_key: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::toggle_room_reaction(
        handle_id,
        room_id,
        event_id,
        reaction_key,
    ))
}

fn matrix_redact_room_event(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reason: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::redact_room_event(
        handle_id, room_id, event_id, reason,
    ))
}

fn matrix_mark_room_event_as_read(handle_id: u64, room_id: &str, event_id: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::mark_room_event_as_read(
        handle_id, room_id, event_id,
    ))
}

fn matrix_report_room_event(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reason: &str,
    score: i32,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::report_room_event(
        handle_id, room_id, event_id, reason, score,
    ))
}

fn matrix_fetch_room_pinned_event_ids(
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<String>, String> {
    runtime().block_on(matrix_backend::runtime::fetch_room_pinned_event_ids(
        handle_id, room_id,
    ))
}

fn matrix_pin_room_event(handle_id: u64, room_id: &str, event_id: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::pin_room_event(
        handle_id, room_id, event_id,
    ))
}

fn matrix_unpin_room_event(handle_id: u64, room_id: &str, event_id: &str) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::unpin_room_event(
        handle_id, room_id, event_id,
    ))
}

fn matrix_fetch_active_room_raw_event_json(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<String, String> {
    runtime().block_on(matrix_backend::runtime::fetch_active_room_raw_event_json(
        handle_id, room_id, event_id,
    ))
}

fn matrix_fetch_room_read_receipts(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<Vec<ffi::MatrixReadReceiptEntry>, String> {
    Ok(runtime()
        .block_on(matrix_backend::runtime::fetch_room_read_receipts(
            handle_id, room_id, event_id,
        ))?
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
    handle_id: u64,
    room_id: &str,
) -> Result<ffi::MatrixRoomRedactionPermissions, String> {
    let result = runtime().block_on(matrix_backend::runtime::fetch_room_redaction_permissions(
        handle_id, room_id,
    ))?;

    Ok(ffi::MatrixRoomRedactionPermissions {
        can_redact_own: result.can_redact_own,
        can_redact_other: result.can_redact_other,
    })
}

fn matrix_send_room_attachment(
    handle_id: u64,
    room_id: &str,
    file_path: &str,
    filename: &str,
    caption: &str,
    reply_event_id: &str,
    mime_type: &str,
) -> Result<(), String> {
    runtime().block_on(matrix_backend::runtime::send_room_attachment(
        handle_id,
        room_id,
        file_path,
        filename,
        caption,
        reply_event_id,
        mime_type,
    ))
}

fn matrix_discover_login_flows(
    server_name_or_url: &str,
    verify_certificates: bool,
) -> Result<ffi::MatrixLoginFlows, String> {
    let result = runtime().block_on(matrix_backend::auth::discover_login_flows(
        server_name_or_url,
        verify_certificates,
    ))?;

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
    homeserver_url: &str,
    redirect_url: &str,
    identity_provider_id: &str,
    verify_certificates: bool,
) -> Result<String, String> {
    runtime().block_on(matrix_backend::auth::get_sso_login_url(
        homeserver_url,
        redirect_url,
        identity_provider_id,
        verify_certificates,
    ))
}

fn matrix_start_sso_callback_server(
    success_html: &str,
    failure_html: &str,
    timeout_ms: u32,
) -> Result<ffi::MatrixSsoCallbackServer, String> {
    logging::ensure_initialized();
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
    logging::ensure_initialized();
    let result = matrix_backend::auth::poll_sso_callback_server(listener_id)?;

    Ok(ffi::MatrixSsoCallbackStatus {
        ready: result.ready,
        success: result.success,
        login_token: result.login_token,
        callback_query: result.callback_query,
    })
}

fn matrix_stop_sso_callback_server(listener_id: u64) -> Result<(), String> {
    logging::ensure_initialized();
    matrix_backend::auth::stop_sso_callback_server(listener_id)
}

fn matrix_start_oauth_login(
    profile_id: &str,
    homeserver_url: &str,
    redirect_url: &str,
    user_id_hint: &str,
    device_id: &str,
    initial_device_display_name: &str,
    verify_certificates: bool,
) -> Result<ffi::MatrixOauthLoginStartResult, String> {
    let result = runtime().block_on(matrix_backend::auth::start_oauth_login(
        profile_id,
        homeserver_url,
        redirect_url,
        user_id_hint,
        device_id,
        initial_device_display_name,
        verify_certificates,
    ))?;

    Ok(ffi::MatrixOauthLoginStartResult {
        login_id: result.login_id,
        login_url: result.login_url,
    })
}

fn matrix_finish_oauth_login(
    login_id: u64,
    callback_query: &str,
) -> Result<ffi::MatrixLoginResult, String> {
    let result = runtime().block_on(matrix_backend::auth::finish_oauth_login(
        login_id,
        callback_query,
    ))?;

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
    profile_id: &str,
    homeserver_url: &str,
    user_id: &str,
    password: &str,
    device_id: &str,
    initial_device_display_name: &str,
    verify_certificates: bool,
) -> Result<ffi::MatrixLoginResult, String> {
    let result = runtime().block_on(matrix_backend::auth::login_password(
        profile_id,
        homeserver_url,
        user_id,
        password,
        device_id,
        initial_device_display_name,
        verify_certificates,
    ))?;

    Ok(ffi::MatrixLoginResult {
        user_id: result.user_id,
        access_token: result.access_token,
        device_id: result.device_id,
        homeserver_url: result.homeserver_url,
    })
}

fn matrix_login_token(
    profile_id: &str,
    homeserver_url: &str,
    login_token: &str,
    device_id: &str,
    initial_device_display_name: &str,
    verify_certificates: bool,
) -> Result<ffi::MatrixLoginResult, String> {
    let result = runtime().block_on(matrix_backend::auth::login_token(
        profile_id,
        homeserver_url,
        login_token,
        device_id,
        initial_device_display_name,
        verify_certificates,
    ))?;

    Ok(ffi::MatrixLoginResult {
        user_id: result.user_id,
        access_token: result.access_token,
        device_id: result.device_id,
        homeserver_url: result.homeserver_url,
    })
}
