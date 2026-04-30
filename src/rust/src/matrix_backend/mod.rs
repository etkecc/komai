// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pub mod auth;
pub mod bootstrap;
pub(crate) mod ffi;
pub mod image_metadata;
pub mod registration;
pub mod runtime;
pub mod session_persistence;

pub struct DerivedMatrixSdkPaths {
    pub profile_data_root: String,
    pub profile_cache_root: String,
    pub matrix_data_root: String,
    pub matrix_cache_root: String,
    pub state_store_root: String,
    pub cache_root: String,
    pub event_cache_root: String,
    pub media_cache_root: String,
}

fn join_path(base: &str, segment: &str) -> String {
    if base.ends_with('/') {
        format!("{base}{segment}")
    } else {
        format!("{base}/{segment}")
    }
}

pub fn derive_matrix_sdk_paths(
    profile_data_root: &str,
    profile_cache_root: &str,
) -> DerivedMatrixSdkPaths {
    let matrix_data_root = join_path(profile_data_root, "matrix-sdk");
    let matrix_cache_root = join_path(profile_cache_root, "matrix-sdk");
    let state_store_root = join_path(&matrix_data_root, "state-store");
    let cache_root = join_path(&matrix_cache_root, "cache");

    DerivedMatrixSdkPaths {
        profile_data_root: profile_data_root.to_owned(),
        profile_cache_root: profile_cache_root.to_owned(),
        matrix_data_root,
        matrix_cache_root,
        state_store_root,
        cache_root: cache_root.clone(),
        event_cache_root: join_path(&cache_root, "event-cache"),
        media_cache_root: join_path(&cache_root, "media-cache"),
    }
}
