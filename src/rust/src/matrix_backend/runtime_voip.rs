// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use serde::Deserialize;

#[derive(Debug, Deserialize)]
struct RawTurnServerInfoResponse {
    #[serde(default)]
    username: String,
    #[serde(default)]
    password: String,
    #[serde(default)]
    uris: Vec<String>,
    #[serde(default)]
    ttl: u64,
}

async fn fetch_turn_server_info_response_body(
    client: &matrix_sdk::Client,
    access_token: &str,
) -> Result<String, String> {
    let request_paths = [
        "/_matrix/client/v3/voip/turnServer",
        "/_matrix/client/r0/voip/turnServer",
    ];

    let mut last_error = None;

    for path in request_paths {
        let endpoint = client
            .homeserver()
            .join(path)
            .map_err(|e| format!("failed to build matrix TURN server URL for {path}: {e}"))?;
        let response = client
            .http_client()
            .get(endpoint.clone())
            .bearer_auth(access_token)
            .header(reqwest::header::ACCEPT, "application/json")
            .send()
            .await
            .map_err(|e| format!("failed to fetch matrix TURN server info from {endpoint}: {e}"))?;

        let status = response.status();
        let body = response
            .text()
            .await
            .map_err(|e| format!("failed to read matrix TURN server response body: {e}"))?;

        if status.is_success() {
            return Ok(body);
        }

        if status == reqwest::StatusCode::NOT_FOUND {
            last_error = Some(format!("TURN endpoint {endpoint} returned {status}"));
            continue;
        }

        let trimmed_body = body.trim();
        return Err(if trimmed_body.is_empty() {
            format!("failed to fetch matrix TURN server info from {endpoint}: HTTP {status}")
        } else {
            format!(
                "failed to fetch matrix TURN server info from {endpoint}: HTTP {status}; body={trimmed_body}"
            )
        });
    }

    Err(last_error.unwrap_or_else(|| {
        "failed to fetch matrix TURN server info: no supported endpoint succeeded".to_owned()
    }))
}

pub async fn fetch_turn_server_info(handle_id: u64) -> Result<MatrixTurnServerInfo, String> {
    let client = client_for_handle(handle_id)?;
    let access_token = client
        .access_token()
        .ok_or_else(|| "matrix TURN server info requested without an access token".to_owned())?;

    tracing::info!(handle_id, "Fetching matrix-sdk TURN server info");

    let body = fetch_turn_server_info_response_body(&client, &access_token).await?;
    let response: RawTurnServerInfoResponse = serde_json::from_str(&body)
        .map_err(|e| format!("failed to parse matrix TURN server info response: {e}; body={body}"))?;

    if response.uris.is_empty() {
        tracing::info!(handle_id, "Homeserver returned no TURN URIs");
    }

    if !response.uris.is_empty() && response.username.is_empty() && response.password.is_empty() {
        tracing::warn!(
            handle_id,
            "Homeserver returned TURN URIs without credentials; continuing without embedded auth"
        );
    }

    Ok(MatrixTurnServerInfo {
        username: response.username,
        password: response.password,
        uris: response.uris,
        ttl_seconds: response.ttl,
    })
}
