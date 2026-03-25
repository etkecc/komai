// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

pub async fn fetch_own_profile(handle_id: u64) -> Result<MatrixOwnProfile, String> {
    let client = client_for_handle(handle_id)?;
    let user_id = client
        .user_id()
        .map(|user_id| user_id.to_string())
        .unwrap_or_default();

    tracing::debug!(handle_id, user_id, "Fetching own profile via matrix-sdk backend runtime");

    let profile = client
        .account()
        .fetch_user_profile()
        .await
        .map_err(|e| format!("failed to fetch own profile via matrix-sdk: {e}"))?;

    let display_name = profile
        .get_static::<DisplayName>()
        .map_err(|e| format!("failed to parse display name from matrix-sdk profile response: {e}"))?
        .unwrap_or_default();

    let avatar_url = profile
        .get_static::<AvatarUrl>()
        .map_err(|e| format!("failed to parse avatar URL from matrix-sdk profile response: {e}"))?
        .map(|url| normalize_mxc_uri(url.to_string()))
        .unwrap_or_default();

    tracing::debug!(
        handle_id,
        user_id,
        has_display_name = !display_name.is_empty(),
        has_avatar_url = !avatar_url.is_empty(),
        "Fetched own profile via matrix-sdk backend runtime"
    );

    Ok(MatrixOwnProfile {
        display_name,
        avatar_url,
    })
}

pub async fn fetch_media_content(
    handle_id: u64,
    mxc_uri: &str,
    width: i32,
    height: i32,
    crop: bool,
) -> Result<Vec<u8>, String> {
    let client = client_for_handle(handle_id)?;
    let normalized_mxc_uri = normalize_mxc_uri(mxc_uri.trim().to_owned());
    let uri = Box::<MxcUri>::from(normalized_mxc_uri.as_str());
    uri.validate()
        .map_err(|e| format!("invalid mxc uri '{normalized_mxc_uri}': {e}"))?;
    let uri = uri.to_owned();

    let request = if width > 0 && height > 0 {
        let width =
            UInt::try_from(width).map_err(|_| format!("invalid thumbnail width: {width}"))?;
        let height =
            UInt::try_from(height).map_err(|_| format!("invalid thumbnail height: {height}"))?;
        let method = if crop { Method::Crop } else { Method::Scale };

        MediaRequestParameters {
            source: MediaSource::Plain(uri.into()),
            format: MediaFormat::Thumbnail(MediaThumbnailSettings::with_method(
                method, width, height,
            )),
        }
    } else {
        MediaRequestParameters {
            source: MediaSource::Plain(uri.into()),
            format: MediaFormat::File,
        }
    };

    tracing::debug!(
        handle_id,
        mxc_uri = normalized_mxc_uri,
        width,
        height,
        crop,
        "Fetching matrix media content via matrix-sdk backend runtime"
    );

    client
        .media()
        .get_media_content(&request, false)
        .await
        .map_err(|e| format!("failed to fetch matrix media content via matrix-sdk: {e}"))
}
