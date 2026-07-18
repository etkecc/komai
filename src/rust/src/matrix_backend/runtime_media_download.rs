// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Full-file timeline media download with observable progress.  Used by the
// media overlay's download-then-play path (video on no-Range homeservers),
// where matrix-sdk's `get_media_content` is all-or-nothing and the UI could
// only show an indeterminate spinner.  The download streams the body chunk
// by chunk, publishing (received, total) into a registry the C++ side polls
// via `active_timeline_media_download_progress`.

use std::io::Read;

use futures_util::StreamExt as _;
use matrix_sdk_base::media::store::IgnoreMediaRetentionPolicy;

use super::*;
use super::media_proxy::{media_throttle_kbps, parse_mxc_uri};

/// In-flight download progress, keyed by `"{handle_id}:{item_id}"`.
/// Values are `(received_bytes, total_bytes)`; `total_bytes` is 0 while
/// unknown.  Entries exist only while a download is running.
fn download_progress_registry() -> &'static Mutex<HashMap<String, (u64, u64)>> {
    static REGISTRY: OnceLock<Mutex<HashMap<String, (u64, u64)>>> = OnceLock::new();
    REGISTRY.get_or_init(|| Mutex::new(HashMap::new()))
}

fn progress_key(handle_id: u64, item_id: &str) -> String {
    format!("{handle_id}:{item_id}")
}

fn set_progress(key: &str, received: u64, total: u64) {
    download_progress_registry()
        .lock()
        .expect("poisoned media download progress registry mutex")
        .insert(key.to_owned(), (received, total));
}

/// Removes the registry entry when the download ends, on every exit path.
struct ProgressEntryGuard {
    key: String,
}

impl Drop for ProgressEntryGuard {
    fn drop(&mut self) {
        download_progress_registry()
            .lock()
            .expect("poisoned media download progress registry mutex")
            .remove(&self.key);
    }
}

pub fn active_timeline_media_download_progress(handle_id: u64, item_id: &str) -> (u64, u64) {
    download_progress_registry()
        .lock()
        .expect("poisoned media download progress registry mutex")
        .get(&progress_key(handle_id, item_id.trim()))
        .copied()
        .unwrap_or((0, 0))
}

pub async fn fetch_active_room_timeline_media_content_with_progress(
    handle_id: u64,
    item_id: &str,
) -> Result<Vec<u8>, String> {
    ensure_handle_auth_usable(handle_id)?;
    let client = client_for_handle(handle_id)?;
    let item_id = item_id.trim();
    if item_id.is_empty() {
        return Err("cannot fetch matrix-sdk timeline media without an item id".to_owned());
    }

    let media_request = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .and_then(|handle| {
            handle
                .room_timeline_media_lookup
                .lock()
                .expect("poisoned matrix room timeline media lookup mutex")
                .get(item_id)
                .cloned()
        })
        .ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no active timeline media for item '{item_id}'"
            )
        })?;

    // The same request `get_media_content(request, true)` would build for the
    // full-size file, so this path shares its SDK media-store cache entries.
    let request = MediaRequestParameters {
        source: media_request.source.clone(),
        format: MediaFormat::File,
    };

    if let Some(content) = cached_media_content(&client, &request).await {
        tracing::info!(
            handle_id,
            item_id,
            size = content.len(),
            "media download: served from SDK media store cache"
        );
        return Ok(content);
    }

    let mxc_uri = match &media_request.source {
        MediaSource::Plain(uri) => uri.to_string(),
        MediaSource::Encrypted(file) => file.url.to_string(),
    };
    let (server_name, media_id) = parse_mxc_uri(&mxc_uri)?;

    let access_token = client
        .access_token()
        .ok_or_else(|| "no access token available for media download".to_owned())?;

    let url = format!(
        "{}/_matrix/client/v1/media/download/{}/{}",
        client.homeserver().as_str().trim_end_matches('/'),
        server_name,
        media_id,
    );

    let key = progress_key(handle_id, item_id);
    let _progress_guard = ProgressEntryGuard { key: key.clone() };

    let response = client
        .http_client()
        .get(&url)
        // Same rationale as the media proxy: matrix-sdk's shared HTTP client
        // has a ~30s request timeout that would abort a slow/large download
        // mid-body.  Media downloads can legitimately take much longer.
        .timeout(Duration::from_secs(3600))
        .bearer_auth(&access_token)
        .send()
        .await
        .map_err(|e| format!("media download request failed: {e}"))?;

    if !response.status().is_success() {
        return Err(format!(
            "media download failed with HTTP {}",
            response.status()
        ));
    }

    // Prefer the response's Content-Length; Synapse's chunked 200 omits it,
    // so fall back to the size advertised in the event's media info (which
    // may be absent or wrong — the receiving loop keeps total >= received).
    let mut total = response
        .content_length()
        .filter(|&len| len > 0)
        .unwrap_or(media_request.size_bytes);

    let throttle_kbps = media_throttle_kbps();
    tracing::info!(
        handle_id,
        item_id,
        total,
        throttle_kbps,
        "media download: starting progress-tracked fetch"
    );
    set_progress(&key, 0, total);
    // Cap the preallocation: `total` can come from unvalidated event data.
    let mut content: Vec<u8> = Vec::with_capacity(total.min(64 * 1024 * 1024) as usize);
    let mut body = response.bytes_stream();
    while let Some(chunk) = body.next().await {
        let chunk = chunk.map_err(|e| format!("media download stream error: {e}"))?;
        content.extend_from_slice(&chunk);
        total = total.max(content.len() as u64);
        set_progress(&key, content.len() as u64, total);
        // Optional: pace the download to simulate a slow homeserver
        // (KOMAI_DEBUG_MEDIA_THROTTLE_KBPS), same knob as the media proxy.
        if let Some(kbps) = throttle_kbps {
            let ms = (chunk.len() as u64 * 1000) / (kbps * 1024).max(1);
            if ms > 0 {
                tokio::time::sleep(Duration::from_millis(ms)).await;
            }
        }
    }

    let content = match &media_request.source {
        MediaSource::Encrypted(file) => {
            let content_len = content.len();
            let mut cursor = std::io::Cursor::new(content);
            let mut reader = matrix_sdk_base::crypto::AttachmentDecryptor::new(
                &mut cursor,
                file.as_ref().clone().into(),
            )
            .map_err(|e| format!("failed to set up media decryption: {e}"))?;
            // Decrypted size equals the encrypted size rounded down to the
            // cipher block, so this capacity is exact enough.
            let mut decrypted = Vec::with_capacity(content_len);
            reader
                .read_to_end(&mut decrypted)
                .map_err(|e| format!("failed to decrypt media: {e}"))?;
            decrypted
        }
        MediaSource::Plain(_) => content,
    };

    tracing::info!(
        handle_id,
        item_id,
        received = content.len(),
        "media download: progress-tracked fetch complete"
    );

    store_media_content(&client, &request, &content).await;

    Ok(content)
}

/// Reads the SDK media store; any error is logged and treated as a miss.
async fn cached_media_content(
    client: &Client,
    request: &MediaRequestParameters,
) -> Option<Vec<u8>> {
    match client.media_store().lock().await {
        Ok(store) => match store.get_media_content(request).await {
            Ok(content) => content,
            Err(e) => {
                tracing::warn!("media download: SDK media store read failed: {e}");
                None
            }
        },
        Err(e) => {
            tracing::warn!("media download: SDK media store lock failed: {e}");
            None
        }
    }
}

/// Persists the downloaded media into the SDK media store — the same cache
/// `get_media_content(request, true)` populates — so later fetches of this
/// item (save-as, image provider) are served locally.  Best-effort.
async fn store_media_content(client: &Client, request: &MediaRequestParameters, content: &[u8]) {
    match client.media_store().lock().await {
        Ok(store) => {
            if let Err(e) = store
                .add_media_content(request, content.to_vec(), IgnoreMediaRetentionPolicy::No)
                .await
            {
                tracing::warn!("media download: SDK media store write failed: {e}");
            }
        }
        Err(e) => {
            tracing::warn!("media download: SDK media store lock failed: {e}");
        }
    }
}
