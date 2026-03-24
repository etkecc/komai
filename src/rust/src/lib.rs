// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::sync::OnceLock;

use resolvematrix::server::MatrixResolver;
use tokio::runtime::Runtime;

pub mod ipc;
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

    struct MatrixLoginResult {
        user_id: String,
        access_token: String,
        device_id: String,
        homeserver_url: String,
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
    }

    extern "Rust" {
        fn resolve_server(server_name: &str) -> Result<ResolveResult>;
        fn matrix_sdk_paths(profile_id: &str) -> MatrixSdkPaths;
        fn matrix_restore_session_preview(profile_id: &str) -> Result<MatrixRestorePreview>;
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
