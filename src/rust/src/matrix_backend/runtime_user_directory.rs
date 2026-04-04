// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

pub async fn search_users(
    handle_id: u64,
    search_term: &str,
    limit: u64,
) -> Result<Vec<MatrixDirectoryUser>, String> {
    let client = client_for_handle(handle_id)?;
    let search_term = search_term.trim().trim_start_matches('@');

    if search_term.is_empty() {
        return Ok(Vec::new());
    }

    tracing::info!(
        handle_id,
        search_term,
        limit,
        "Searching Matrix user directory via matrix-sdk backend runtime"
    );

    let response = client
        .search_users(search_term, limit)
        .await
        .map_err(|e| format!("failed to search Matrix user directory for '{search_term}': {e}"))?;

    let limited = response.limited;
    let users = response
        .results
        .into_iter()
        .map(|user| MatrixDirectoryUser {
            display_name: user.display_name.unwrap_or_default(),
            user_id: user.user_id.to_string(),
            avatar_url: user
                .avatar_url
                .map(|url| normalize_mxc_uri(url.to_string()))
                .unwrap_or_default(),
        })
        .collect::<Vec<_>>();

    tracing::info!(
        handle_id,
        search_term,
        limit,
        result_count = users.len(),
        limited,
        "Finished Matrix user directory search via matrix-sdk backend runtime"
    );

    Ok(users)
}
