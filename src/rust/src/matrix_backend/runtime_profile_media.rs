// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use mime::Mime;
use std::fs;

fn profile_response_to_user_profile(
    profile: matrix_sdk::ruma::api::client::profile::get_profile::v3::Response,
) -> Result<MatrixUserProfile, String> {
    let display_name = profile
        .get_static::<DisplayName>()
        .map_err(|e| format!("failed to parse display name from matrix-sdk profile response: {e}"))?
        .unwrap_or_default();

    let avatar_url = profile
        .get_static::<AvatarUrl>()
        .map_err(|e| format!("failed to parse avatar URL from matrix-sdk profile response: {e}"))?
        .map(|url| normalize_mxc_uri(url.to_string()))
        .unwrap_or_default();

    Ok(MatrixUserProfile {
        display_name,
        avatar_url,
    })
}

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

    let profile = profile_response_to_user_profile(profile)?;

    tracing::debug!(
        handle_id,
        user_id,
        has_display_name = !profile.display_name.is_empty(),
        has_avatar_url = !profile.avatar_url.is_empty(),
        "Fetched own profile via matrix-sdk backend runtime"
    );

    Ok(MatrixOwnProfile {
        display_name: profile.display_name,
        avatar_url: profile.avatar_url,
    })
}

pub async fn fetch_user_profile(handle_id: u64, user_id: &str) -> Result<MatrixUserProfile, String> {
    let client = client_for_handle(handle_id)?;
    let parsed_user_id = parse_user_id(user_id)?;

    tracing::debug!(
        handle_id,
        user_id = parsed_user_id.as_str(),
        "Fetching user profile via matrix-sdk backend runtime"
    );

    let profile = client
        .account()
        .fetch_user_profile_of(&parsed_user_id)
        .await
        .map_err(|e| format!("failed to fetch profile for '{}': {e}", parsed_user_id.as_str()))?;

    profile_response_to_user_profile(profile)
}

pub async fn set_own_display_name(handle_id: u64, display_name: &str) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let display_name = display_name.trim();

    tracing::info!(
        handle_id,
        has_display_name = !display_name.is_empty(),
        "Setting own display name via matrix-sdk backend runtime"
    );

    client
        .account()
        .set_display_name((!display_name.is_empty()).then_some(display_name))
        .await
        .map_err(|e| format!("failed to set own display name via matrix-sdk: {e}"))
}

pub async fn upload_own_avatar(
    handle_id: u64,
    file_path: &str,
    mime_type: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let mime = mime_type
        .trim()
        .parse::<Mime>()
        .map_err(|e| format!("invalid avatar mime type '{mime_type}': {e}"))?;
    let data = fs::read(file_path)
        .map_err(|e| format!("failed to read avatar file '{file_path}': {e}"))?;

    tracing::info!(
        handle_id,
        file_path,
        mime_type,
        "Uploading own avatar via matrix-sdk backend runtime"
    );

    client
        .account()
        .upload_avatar(&mime, data)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to upload own avatar via matrix-sdk: {e}"))
}

pub async fn remove_own_avatar(handle_id: u64) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;

    tracing::info!(handle_id, "Removing own avatar via matrix-sdk backend runtime");

    client
        .account()
        .set_avatar_url(None)
        .await
        .map_err(|e| format!("failed to remove own avatar via matrix-sdk: {e}"))
}

pub async fn ignore_user(handle_id: u64, user_id: &str) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let parsed_user_id = parse_user_id(user_id)?;

    tracing::info!(
        handle_id,
        user_id = parsed_user_id.as_str(),
        "Ignoring user via matrix-sdk backend runtime"
    );

    client
        .account()
        .ignore_user(&parsed_user_id)
        .await
        .map_err(|e| format!("failed to ignore user '{}': {e}", parsed_user_id.as_str()))
}

pub async fn unignore_user(handle_id: u64, user_id: &str) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let parsed_user_id = parse_user_id(user_id)?;

    tracing::info!(
        handle_id,
        user_id = parsed_user_id.as_str(),
        "Unignoring user via matrix-sdk backend runtime"
    );

    client
        .account()
        .unignore_user(&parsed_user_id)
        .await
        .map_err(|e| format!("failed to unignore user '{}': {e}", parsed_user_id.as_str()))
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
