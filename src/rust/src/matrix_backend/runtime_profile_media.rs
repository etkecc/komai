// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use matrix_sdk::ruma::{
    api::client::presence::{get_presence, set_presence},
    presence::PresenceState,
};
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

fn room_member_to_user_profile(member: &matrix_sdk::room::RoomMember) -> MatrixUserProfile {
    MatrixUserProfile {
        display_name: member.display_name().unwrap_or_default().to_owned(),
        avatar_url: member
            .avatar_url()
            .map(|url| normalize_mxc_uri(url.to_string()))
            .unwrap_or_default(),
    }
}

fn explicit_room_member_display_name(member: &matrix_sdk::room::RoomMember) -> Option<String> {
    member
        .event()
        .displayname_value()
        .map(str::to_owned)
}

async fn fetch_own_room_member(
    room: &Room,
) -> Result<(OwnedUserId, matrix_sdk::room::RoomMember), String> {
    let own_user_id = room.own_user_id().to_owned();
    let member = room
        .get_member(&own_user_id)
        .await
        .map_err(|e| {
            format!(
                "failed to fetch own matrix-sdk room member for '{}': {e}",
                room.room_id().as_str()
            )
        })?
        .ok_or_else(|| {
            format!(
                "matrix-sdk room '{}' does not have a joined member event for '{}'",
                room.room_id().as_str(),
                own_user_id
            )
        })?;

    Ok((own_user_id, member))
}

fn presence_state_from_token(presence_state: &str) -> Result<PresenceState, String> {
    match presence_state.trim() {
        "" | "automatic" | "automatic_presence" | "online" => Ok(PresenceState::Online),
        "unavailable" => Ok(PresenceState::Unavailable),
        "offline" => Ok(PresenceState::Offline),
        other => Err(format!("unsupported own presence state '{other}'")),
    }
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

pub async fn fetch_own_presence(handle_id: u64) -> Result<MatrixOwnPresence, String> {
    let client = client_for_handle(handle_id)?;
    let user_id = client
        .user_id()
        .map(|user_id| user_id.to_owned())
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} has no user id"))?;

    tracing::debug!(
        handle_id,
        user_id = user_id.as_str(),
        "Fetching own presence via matrix-sdk backend runtime"
    );

    let response = client
        .send(get_presence::v3::Request::new(user_id))
        .await
        .map_err(|e| format!("failed to fetch own presence via matrix-sdk: {e}"))?;

    Ok(MatrixOwnPresence {
        state: response.presence.as_ref().to_owned(),
        status_message: response.status_msg.unwrap_or_default(),
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

pub async fn fetch_room_member_profile(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
) -> Result<MatrixUserProfile, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let parsed_user_id = parse_user_id(user_id)?;

    tracing::debug!(
        handle_id,
        room_id = room.room_id().as_str(),
        user_id = parsed_user_id.as_str(),
        "Fetching room member profile via matrix-sdk backend runtime"
    );

    let member = room
        .get_member(&parsed_user_id)
        .await
        .map_err(|e| {
            format!(
                "failed to fetch room member '{}' in '{}': {e}",
                parsed_user_id.as_str(),
                room.room_id().as_str()
            )
        })?
        .ok_or_else(|| {
            format!(
                "matrix-sdk room '{}' does not have a member event for '{}'",
                room.room_id().as_str(),
                parsed_user_id.as_str()
            )
        })?;

    Ok(room_member_to_user_profile(&member))
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

pub async fn set_own_presence(
    handle_id: u64,
    presence_state: &str,
    status_message: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let user_id = client
        .user_id()
        .map(|user_id| user_id.to_owned())
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} has no user id"))?;
    let presence = presence_state_from_token(presence_state)?;
    let trimmed_status_message = status_message.trim();
    let mut request = set_presence::v3::Request::new(user_id, presence.clone());
    request.status_msg = (!trimmed_status_message.is_empty()).then_some(trimmed_status_message.to_owned());

    tracing::info!(
        handle_id,
        presence_state = presence.as_ref(),
        has_status_message = !trimmed_status_message.is_empty(),
        "Setting own presence via matrix-sdk backend runtime"
    );

    client
        .send(request)
        .await
        .map(|_: set_presence::v3::Response| ())
        .map_err(|e| format!("failed to set own presence via matrix-sdk: {e}"))
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
    // Avatars always go through metadata stripping — they're publicly visible
    // to every member of every room you're in, so there's no scenario where a
    // user would benefit from leaking GPS / camera metadata via their avatar.
    let data = crate::matrix_backend::image_metadata::strip_image_metadata(data, &mime);

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

pub async fn upload_own_room_avatar(
    handle_id: u64,
    room_id: &str,
    file_path: &str,
    mime_type: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let room = joined_room_for_handle(handle_id, room_id)?;
    let (own_user_id, member) = fetch_own_room_member(&room).await?;
    let mime = mime_type
        .trim()
        .parse::<Mime>()
        .map_err(|e| format!("invalid avatar mime type '{mime_type}': {e}"))?;
    let data = fs::read(file_path)
        .map_err(|e| format!("failed to read avatar file '{file_path}': {e}"))?;
    // See `upload_own_avatar` — same rationale: room-specific avatars are
    // public to every member of that room, so always strip metadata.
    let data = crate::matrix_backend::image_metadata::strip_image_metadata(data, &mime);

    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        file_path,
        mime_type,
        "Uploading own room avatar via matrix-sdk backend runtime"
    );

    let response = client
        .media()
        .upload(&mime, data, None)
        .await
        .map_err(|e| format!("failed to upload own room avatar media via matrix-sdk: {e}"))?;

    let mut content = RoomMemberEventContent::new(MembershipState::Join);
    content.displayname = explicit_room_member_display_name(&member);
    content.avatar_url = Some(response.content_uri);

    room.send_state_event_for_key(&own_user_id, content)
        .await
        .map(|_| ())
        .map_err(|e| {
            format!(
                "failed to update matrix-sdk room-specific avatar for '{}': {e}",
                room.room_id().as_str()
            )
        })
}

pub async fn remove_own_room_avatar(handle_id: u64, room_id: &str) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let (own_user_id, member) = fetch_own_room_member(&room).await?;

    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        "Removing own room avatar via matrix-sdk backend runtime"
    );

    let mut content = RoomMemberEventContent::new(MembershipState::Join);
    content.displayname = explicit_room_member_display_name(&member);
    content.avatar_url = None;

    room.send_state_event_for_key(&own_user_id, content)
        .await
        .map(|_| ())
        .map_err(|e| {
            format!(
                "failed to remove matrix-sdk room-specific avatar for '{}': {e}",
                room.room_id().as_str()
            )
        })
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
    ensure_handle_auth_usable(handle_id)?;
    let client = client_for_handle(handle_id)?;
    let normalized_mxc_uri = normalize_mxc_uri(mxc_uri.trim().to_owned());
    let uri_ref = <&MxcUri>::from(normalized_mxc_uri.as_str());
    uri_ref
        .validate()
        .map_err(|e| format!("invalid mxc uri '{normalized_mxc_uri}': {e}"))?;
    let uri = uri_ref.to_owned();

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

    // matrix-sdk's get_media_content does not enforce its own deadline on
    // this path. When a media URL is unreachable (e.g. matrix.org's
    // authenticated media gating against a homeserver that isn't relaying
    // it, or any peer that just refuses without responding), the request
    // hangs indefinitely. Each hung fetch holds a C++ thread-pool worker;
    // a single broken avatar in a busy room re-queues fetches on every
    // layout pass and eventually starves all media downloads. Capping the
    // wait turns the hang into a normal "fetch failed" the rest of the
    // pipeline already knows how to handle.
    //
    // Thumbnails (avatars, inline previews) run on the UI hot path — give
    // up fast so a broken peer's avatar doesn't leave the slot empty for
    // long. Full-file fetches (user-initiated downloads, viewer opens)
    // legitimately handle large bodies on slow links, so they get a
    // longer ceiling.
    let fetch_timeout = if width > 0 && height > 0 {
        std::time::Duration::from_secs(10)
    } else {
        std::time::Duration::from_secs(60)
    };

    match tokio::time::timeout(
        fetch_timeout,
        client.media().get_media_content(&request, false),
    )
    .await
    {
        Ok(Ok(bytes)) => Ok(bytes),
        Ok(Err(e)) => Err(format!("failed to fetch matrix media content via matrix-sdk: {e}")),
        Err(_elapsed) => {
            tracing::warn!(
                handle_id,
                mxc_uri = %normalized_mxc_uri,
                timeout_seconds = fetch_timeout.as_secs(),
                "matrix-sdk get_media_content timed out — likely homeserver federation media issue"
            );
            Err(format!(
                "matrix-sdk get_media_content timed out after {}s",
                fetch_timeout.as_secs()
            ))
        }
    }
}
