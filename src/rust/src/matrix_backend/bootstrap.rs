// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::path::Path;

use matrix_sdk::{Client, ClientBuildError};

use super::DerivedMatrixSdkPaths;

pub struct MatrixSdkBuildConfig<'a> {
    pub homeserver_url: &'a str,
    pub store_passphrase: Option<&'a str>,
}

pub async fn build_client(
    config: &MatrixSdkBuildConfig<'_>,
    paths: &DerivedMatrixSdkPaths,
) -> Result<Client, ClientBuildError> {
    Client::builder()
        .homeserver_url(config.homeserver_url)
        .sqlite_store_with_cache_path(
            Path::new(&paths.state_store_root),
            Path::new(&paths.user_cache_root),
            config.store_passphrase,
        )
        .build()
        .await
}
