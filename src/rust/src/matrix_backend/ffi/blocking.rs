// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::sync::OnceLock;

use resolvematrix::server::MatrixResolver;
use tokio::runtime::Runtime;

use crate::{ffi, matrix_backend};

pub(crate) fn runtime() -> &'static Runtime {
    static RT: OnceLock<Runtime> = OnceLock::new();
    RT.get_or_init(|| Runtime::new().expect("failed to create tokio runtime"))
}

pub(crate) fn resolver() -> &'static MatrixResolver {
    static RES: OnceLock<MatrixResolver> = OnceLock::new();
    RES.get_or_init(|| {
        runtime()
            .block_on(MatrixResolver::new())
            .expect("failed to create MatrixResolver")
    })
}

pub(crate) fn ffi_block_on<F, T>(
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

pub(crate) fn resolve_server(
    context: ffi::MatrixFfiBlockingContext,
    server_name: &str,
) -> Result<ffi::ResolveResult, String> {
    let resolution = ffi_block_on(context, "resolve_server", resolver().resolve_server(server_name))
        .map_err(|e| format!("failed to resolve server '{}': {}", server_name, e))?;

    Ok(ffi::ResolveResult {
        base_url: resolution.base_url(),
    })
}

pub(crate) fn matrix_sdk_paths(profile_id: &str) -> ffi::MatrixSdkPaths {
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
