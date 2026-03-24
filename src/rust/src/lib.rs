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
        homeserver_url: String,
        user_id: String,
        device_id: String,
        state_store_root: String,
        cache_root: String,
    }

    struct MatrixBackendHandleInfo {
        handle_id: u64,
        has_session: bool,
        homeserver_url: String,
        user_id: String,
        device_id: String,
    }

    struct MatrixOwnProfile {
        display_name: String,
        avatar_url: String,
    }

    struct MatrixRoomSummary {
        room_id: String,
        display_name: String,
        avatar_url: String,
        topic: String,
        direct_chat_other_user_id: String,
        is_invite: bool,
        is_space: bool,
        is_direct: bool,
        is_bot_room: bool,
        is_encrypted: bool,
        unread_message_count: u64,
        notification_count: u64,
        highlight_count: u64,
        timestamp: u64,
    }

    struct MatrixTimelineItem {
        item_id: String,
        event_id: String,
        sender_id: String,
        sender_display_name: String,
        sender_avatar_url: String,
        body: String,
        item_kind: String,
        timestamp: u64,
        is_own: bool,
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
        fn matrix_notify_room_timeline_snapshot_updated(handle_id: u64, room_id: &str);
    }

    extern "Rust" {
        fn resolve_server(server_name: &str) -> Result<ResolveResult>;
        fn matrix_sdk_paths(profile_id: &str) -> MatrixSdkPaths;
        fn matrix_restore_session_preview(profile_id: &str) -> Result<MatrixRestorePreview>;
        fn matrix_start_restored_backend(profile_id: &str) -> Result<MatrixBackendHandleInfo>;
        fn matrix_stop_backend(handle_id: u64) -> Result<()>;
        fn matrix_start_backend_sync(handle_id: u64) -> Result<()>;
        fn matrix_fetch_own_profile(handle_id: u64) -> Result<MatrixOwnProfile>;
        fn matrix_fetch_room_list(handle_id: u64) -> Result<Vec<MatrixRoomSummary>>;
        fn matrix_fetch_media_content(
            handle_id: u64,
            mxc_uri: &str,
            width: i32,
            height: i32,
            crop: bool,
        ) -> Result<Vec<u8>>;
        fn matrix_select_active_room_timeline(handle_id: u64, room_id: &str) -> Result<()>;
        fn matrix_fetch_active_room_timeline(handle_id: u64) -> Result<Vec<MatrixTimelineItem>>;
        fn matrix_paginate_active_room_timeline_backwards(
            handle_id: u64,
            page_size: u16,
        ) -> Result<()>;
        fn matrix_send_room_message(
            handle_id: u64,
            room_id: &str,
            body: &str,
            formatted_html: &str,
            message_kind: &str,
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
        homeserver_url: result.homeserver_url,
        user_id: result.user_id,
        device_id: result.device_id,
    })
}

fn matrix_stop_backend(handle_id: u64) -> Result<(), String> {
    logging::ensure_initialized();
    matrix_backend::runtime::stop_backend(handle_id)
}

fn matrix_start_backend_sync(handle_id: u64) -> Result<(), String> {
    logging::ensure_initialized();
    matrix_backend::runtime::start_sync(handle_id)
}

fn matrix_fetch_own_profile(handle_id: u64) -> Result<ffi::MatrixOwnProfile, String> {
    let result = runtime().block_on(matrix_backend::runtime::fetch_own_profile(handle_id))?;

    Ok(ffi::MatrixOwnProfile {
        display_name: result.display_name,
        avatar_url: result.avatar_url,
    })
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
                    direct_chat_other_user_id: room.direct_chat_other_user_id,
                    is_invite: room.is_invite,
                    is_space: room.is_space,
                    is_direct: room.is_direct,
                    is_bot_room: room.is_bot_room,
                    is_encrypted: room.is_encrypted,
                    unread_message_count: room.unread_message_count,
                    notification_count: room.notification_count,
                    highlight_count: room.highlight_count,
                    timestamp: room.timestamp,
                })
                .collect()
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
                    sender_id: item.sender_id,
                    sender_display_name: item.sender_display_name,
                    sender_avatar_url: item.sender_avatar_url,
                    body: item.body,
                    item_kind: item.item_kind,
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
    })
}

fn matrix_stop_sso_callback_server(listener_id: u64) -> Result<(), String> {
    logging::ensure_initialized();
    matrix_backend::auth::stop_sso_callback_server(listener_id)
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
