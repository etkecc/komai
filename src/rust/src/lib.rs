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
        event_cache_root: String,
        media_cache_root: String,
    }

    unsafe extern "C++" {
        include!("matrix/backend/MatrixBackendBridge.h");

        #[namespace = "komai::rust_bridge"]
        fn matrix_profile_data_root(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn matrix_profile_cache_root(profile_id: &str) -> String;
        #[namespace = "komai::rust_bridge"]
        fn matrix_storage_user_component(profile_id: &str, user_id: &str) -> String;
    }

    extern "Rust" {
        fn resolve_server(server_name: &str) -> Result<ResolveResult>;
        fn matrix_sdk_paths(profile_id: &str, user_id: &str) -> MatrixSdkPaths;
    }
}

fn runtime() -> &'static Runtime {
    static RT: OnceLock<Runtime> = OnceLock::new();
    RT.get_or_init(|| {
        Runtime::new().expect("failed to create tokio runtime")
    })
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

fn matrix_sdk_paths(profile_id: &str, user_id: &str) -> ffi::MatrixSdkPaths {
    let paths = matrix_backend::derive_matrix_sdk_paths(
        &ffi::matrix_profile_data_root(profile_id),
        &ffi::matrix_profile_cache_root(profile_id),
        &ffi::matrix_storage_user_component(profile_id, user_id),
    );

    ffi::MatrixSdkPaths {
        profile_data_root: paths.profile_data_root,
        profile_cache_root: paths.profile_cache_root,
        matrix_data_root: paths.matrix_data_root,
        matrix_cache_root: paths.matrix_cache_root,
        state_store_root: paths.state_store_root,
        event_cache_root: paths.event_cache_root,
        media_cache_root: paths.media_cache_root,
    }
}
