// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use matrix_sdk::ruma::serde::Raw;
use serde_json::{Map as JsonMap, Value as JsonValue};
use std::str::FromStr;

pub async fn send_typing_notice(handle_id: u64, room_id: &str, typing: bool) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    room.typing_notice(typing)
        .await
        .map_err(|e| format!("failed to send typing notice: {e}"))
}


pub async fn join_room(
    handle_id: u64,
    room_id_or_alias: &str,
    via: &[String],
    reason: &str,
) -> Result<String, (String, String)> {
    let client = client_for_handle(handle_id).map_err(|e| (e, String::new()))?;
    let parsed_room_or_alias =
        parse_room_or_alias_id(room_id_or_alias).map_err(|e| (e, String::new()))?;
    let via_server_names = parse_via_server_names(via).map_err(|e| (e, String::new()))?;
    let reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id_or_alias = room_id_or_alias.trim(),
        via_count = via_server_names.len(),
        has_reason = reason.is_some(),
        "Joining room via matrix-sdk backend runtime"
    );

    client
        .join_room_by_id_or_alias(parsed_room_or_alias.as_ref(), &via_server_names)
        .await
        .map(|room| room.room_id().to_string())
        .map_err(|error| {
            let matrix_errcode = matrix_errcode(&error);
            tracing::warn!(
                handle_id,
                room_id_or_alias = room_id_or_alias.trim(),
                via_count = via_server_names.len(),
                ?reason,
                %error,
                matrix_errcode,
                "Failed to join room via matrix-sdk backend runtime"
            );
            (error.to_string(), matrix_errcode)
        })
}

pub async fn knock_room(
    handle_id: u64,
    room_id_or_alias: &str,
    via: &[String],
    reason: &str,
) -> Result<String, String> {
    let client = client_for_handle(handle_id)?;
    let parsed_room_or_alias = parse_room_or_alias_id(room_id_or_alias)?;
    let via_server_names = parse_via_server_names(via)?;
    let reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id_or_alias = room_id_or_alias.trim(),
        via_count = via_server_names.len(),
        has_reason = reason.is_some(),
        "Knocking room via matrix-sdk backend runtime"
    );

    client
        .knock(parsed_room_or_alias, reason, via_server_names)
        .await
        .map(|room| room.room_id().to_string())
        .map_err(|e| format!("failed to knock room via matrix-sdk: {e}"))
}

#[allow(clippy::too_many_arguments)]
/// Parses a JSON object string into a `Raw<T>` for a createRoom field that the
/// server interprets, so Komai does not need to model its schema.
fn raw_json_object<T>(field: &str, json: &str) -> Result<Raw<T>, String> {
    let parsed: JsonValue = serde_json::from_str(json)
        .map_err(|e| format!("invalid json for createRoom '{field}': {e}"))?;
    if !parsed.is_object() {
        return Err(format!("createRoom '{field}' must be a json object"));
    }

    let raw = serde_json::value::to_raw_value(&parsed)
        .map_err(|e| format!("failed to re-encode createRoom '{field}': {e}"))?;
    Ok(Raw::from_json(raw))
}

#[allow(clippy::too_many_arguments)]
pub async fn create_room(
    handle_id: u64,
    name: &str,
    topic: &str,
    room_alias_localpart: &str,
    invite_user_ids: &[String],
    preset: &str,
    is_direct: bool,
    is_encrypted: bool,
    is_space: bool,
    is_public: bool,
    room_version: &str,
    power_level_content_override_json: &str,
    initial_state_json: &str,
    creation_content_json: &str,
) -> Result<String, String> {
    let client = client_for_handle(handle_id)?;
    let invite = invite_user_ids
        .iter()
        .map(|user_id| parse_user_id(user_id))
        .collect::<Result<Vec<_>, _>>()?;

    let preset = match preset.trim() {
        "public_chat" => create_room::v3::RoomPreset::PublicChat,
        "trusted_private_chat" => create_room::v3::RoomPreset::TrustedPrivateChat,
        _ => create_room::v3::RoomPreset::PrivateChat,
    };

    let mut request = create_room::v3::Request::new();
    request.name = trim_reason(name);
    request.topic = trim_reason(topic);
    request.room_alias_name = trim_reason(room_alias_localpart);
    request.invite = invite;
    request.is_direct = is_direct;
    request.preset = Some(preset);
    request.visibility = if is_public {
        Visibility::Public
    } else {
        Visibility::Private
    };

    let room_version = room_version.trim();
    if !room_version.is_empty() {
        request.room_version = Some(RoomVersionId::try_from(room_version).map_err(|e| {
            format!("invalid createRoom room version '{room_version}': {e}")
        })?);
    }

    let power_level_content_override_json = power_level_content_override_json.trim();
    if !power_level_content_override_json.is_empty() {
        request.power_level_content_override = Some(raw_json_object(
            "powerLevelContentOverride",
            power_level_content_override_json,
        )?);
    }

    // The room type is what makes a room a space, and it lives in
    // creation_content. Merge rather than choose, so `isSpace` and a
    // caller-supplied creation content can be used together.
    let creation_content_json = creation_content_json.trim();
    if is_space || !creation_content_json.is_empty() {
        let mut content = if creation_content_json.is_empty() {
            JsonMap::new()
        } else {
            match serde_json::from_str::<JsonValue>(creation_content_json) {
                Ok(JsonValue::Object(map)) => map,
                Ok(_) => return Err("createRoom 'creationContent' must be a json object".to_owned()),
                Err(e) => return Err(format!("invalid json for createRoom 'creationContent': {e}")),
            }
        };

        if is_space {
            content.insert(
                "type".to_owned(),
                serde_json::to_value(RoomType::Space)
                    .map_err(|e| format!("failed to encode the space room type: {e}"))?,
            );
        }

        let raw = serde_json::value::to_raw_value(&JsonValue::Object(content))
            .map_err(|e| format!("failed to re-encode createRoom 'creationContent': {e}"))?;
        request.creation_content = Some(Raw::from_json(raw));
    }

    if is_encrypted {
        request.initial_state.push(
            InitialStateEvent::with_empty_state_key(
                RoomEncryptionEventContent::with_recommended_defaults(),
            )
            .to_raw_any(),
        );
    }

    // Caller-supplied initial state is appended after the encryption event, so
    // a caller that wants different encryption settings can override them.
    let initial_state_json = initial_state_json.trim();
    if !initial_state_json.is_empty() {
        let entries: Vec<JsonValue> = serde_json::from_str(initial_state_json)
            .map_err(|e| format!("invalid json for createRoom 'initialState': {e}"))?;
        for (index, entry) in entries.into_iter().enumerate() {
            if !entry.is_object() {
                return Err(format!(
                    "createRoom 'initialState' entry {index} must be a json object"
                ));
            }
            if !entry.get("type").is_some_and(JsonValue::is_string) {
                return Err(format!(
                    "createRoom 'initialState' entry {index} needs a string 'type'"
                ));
            }

            let raw = serde_json::value::to_raw_value(&entry).map_err(|e| {
                format!("failed to re-encode createRoom 'initialState' entry {index}: {e}")
            })?;
            request.initial_state.push(Raw::from_json(raw));
        }
    }

    tracing::info!(
        handle_id,
        is_direct,
        is_encrypted,
        is_space,
        is_public,
        invite_count = request.invite.len(),
        has_name = request.name.is_some(),
        has_topic = request.topic.is_some(),
        has_alias = request.room_alias_name.is_some(),
        has_room_version = request.room_version.is_some(),
        has_power_level_override = request.power_level_content_override.is_some(),
        initial_state_count = request.initial_state.len(),
        "Creating room via matrix-sdk backend runtime"
    );

    client
        .create_room(request)
        .await
        .map(|room| room.room_id().to_string())
        .map_err(|e| format!("failed to create room via matrix-sdk: {e}"))
}

pub async fn leave_room(handle_id: u64, room_id: &str, reason: &str) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let parsed_room_id = parse_room_id(room_id)?;
    let trimmed_reason = reason.trim();

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        has_reason = !trimmed_reason.is_empty(),
        "Leaving room via matrix-sdk backend runtime"
    );

    let room = client
        .get_room(&parsed_room_id)
        .ok_or_else(|| format!("room {room_id} not found in matrix-sdk room list"))?;

    // matrix-sdk's Room::leave() doesn't expose the optional `reason` on
    // m.room.member/leave. Send the leave request manually first so the
    // membership event carries the user-supplied reason, then delegate to
    // room.leave() for predecessor cleanup, local state transition, and
    // auto-forgetting invites. POST /leave is idempotent server-side, and
    // matrix-sdk's leave() short-circuits if state has already settled to
    // Left, so the pair never sends two reason-bearing requests.
    if !trimmed_reason.is_empty() {
        let mut request = leave_room::v3::Request::new(parsed_room_id.clone());
        request.reason = Some(trimmed_reason.to_owned());
        if let Err(error) = client.send(request).await {
            tracing::warn!(
                handle_id,
                room_id = room_id.trim(),
                %error,
                "Failed to send leave request with reason; falling back to plain leave"
            );
        }
    }

    room.leave()
        .await
        .map_err(|e| format!("failed to leave room via matrix-sdk: {e}"))
}

pub async fn toggle_room_tag(
    handle_id: u64,
    room_id: &str,
    tag: &str,
    enabled: bool,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let tag = tag.trim();
    if tag.is_empty() {
        return Err("cannot toggle a matrix-sdk room tag without a tag id".to_owned());
    }

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        tag,
        enabled,
        "Toggling matrix-sdk room tag"
    );

    match tag {
        "m.favourite" => room
            .set_is_favourite(enabled, None)
            .await
            .map_err(|e| format!("failed to toggle matrix-sdk favourite room tag: {e}"))?,
        "m.lowpriority" => room
            .set_is_low_priority(enabled, None)
            .await
            .map_err(|e| format!("failed to toggle matrix-sdk low-priority room tag: {e}"))?,
        "m.server_notice" => {
            if enabled {
                room.set_tag(TagName::ServerNotice, TagInfo::new())
                    .await
                    .map_err(|e| {
                        format!("failed to add matrix-sdk server-notice room tag: {e}")
                    })?;
            } else {
                room.remove_tag(TagName::ServerNotice).await.map_err(|e| {
                    format!("failed to remove matrix-sdk server-notice room tag: {e}")
                })?;
            }
        }
        custom if custom.starts_with("u.") => {
            let tag_name = TagName::User(
                UserTagName::from_str(custom)
                    .map_err(|e| format!("invalid custom matrix room tag '{custom}': {e}"))?,
            );
            if enabled {
                room.set_tag(tag_name, TagInfo::new())
                    .await
                    .map_err(|e| format!("failed to add custom matrix room tag '{custom}': {e}"))?;
            } else {
                room.remove_tag(tag_name).await.map_err(|e| {
                    format!("failed to remove custom matrix room tag '{custom}': {e}")
                })?;
            }
        }
        other => {
            return Err(format!(
                "unsupported matrix-sdk room tag '{other}' for room '{}'",
                room_id.trim()
            ));
        }
    }

    Ok(())
}

pub async fn set_room_is_direct(
    handle_id: u64,
    room_id: &str,
    is_direct: bool,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        is_direct,
        "Updating matrix-sdk direct-message room state"
    );

    room.set_is_direct(is_direct).await.map_err(|e| {
        format!(
            "failed to {} matrix-sdk direct-message room state: {e}",
            if is_direct { "set" } else { "clear" }
        )
    })?;

    Ok(())
}

pub async fn set_own_room_display_name(
    handle_id: u64,
    room_id: &str,
    display_name: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let own_user_id = room.own_user_id().to_owned();
    let member = room
        .get_member(&own_user_id)
        .await
        .map_err(|e| {
            format!(
                "failed to fetch own matrix-sdk room member for '{}': {e}",
                room_id.trim()
            )
        })?
        .ok_or_else(|| {
            format!(
                "matrix-sdk room '{}' does not have a joined member event for '{}'",
                room_id.trim(),
                own_user_id
            )
        })?;

    let trimmed_display_name = display_name.trim();
    let mut content = RoomMemberEventContent::new(MembershipState::Join);
    content.avatar_url = member.event().avatar_url().map(ToOwned::to_owned);
    content.displayname = (!trimmed_display_name.is_empty()).then(|| trimmed_display_name.to_owned());

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        own_user_id = own_user_id.as_str(),
        has_display_name = content.displayname.is_some(),
        "Updating matrix-sdk room-specific display name"
    );

    room.send_state_event_for_key(&own_user_id, content)
        .await
        .map(|_| ())
        .map_err(|e| {
            format!(
                "failed to update matrix-sdk room-specific display name for '{}': {e}",
                room_id.trim()
            )
        })
}

pub async fn invite_user(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let parsed_room_id = parse_room_id(room_id)?;
    let parsed_user_id = parse_user_id(user_id)?;
    let invite_reason = trim_reason(reason);
    let mut invite_user_id = invite_user::v3::InviteUserId::new(parsed_user_id);
    invite_user_id.reason = invite_reason.clone();
    let request = invite_user::v3::Request::new(
        parsed_room_id,
        invite_user::v3::InvitationRecipient::UserId(invite_user_id),
    );

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        user_id = user_id.trim(),
        has_reason = invite_reason.is_some(),
        "Inviting user via matrix-sdk backend runtime"
    );

    client
        .send(request)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to invite user via matrix-sdk: {e}"))
}

pub async fn kick_user(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let parsed_user_id = parse_user_id(user_id)?;
    let reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        user_id = user_id.trim(),
        has_reason = reason.is_some(),
        "Kicking user via matrix-sdk backend runtime"
    );

    room.kick_user(&parsed_user_id, reason.as_deref())
        .await
        .map_err(|e| format!("failed to kick user via matrix-sdk: {e}"))
}

pub async fn ban_user(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let parsed_user_id = parse_user_id(user_id)?;
    let reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        user_id = user_id.trim(),
        has_reason = reason.is_some(),
        "Banning user via matrix-sdk backend runtime"
    );

    room.ban_user(&parsed_user_id, reason.as_deref())
        .await
        .map_err(|e| format!("failed to ban user via matrix-sdk: {e}"))
}

pub async fn unban_user(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let parsed_user_id = parse_user_id(user_id)?;
    let reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        user_id = user_id.trim(),
        has_reason = reason.is_some(),
        "Unbanning user via matrix-sdk backend runtime"
    );

    room.unban_user(&parsed_user_id, reason.as_deref())
        .await
        .map_err(|e| format!("failed to unban user via matrix-sdk: {e}"))
}

/// POST /_matrix/client/v3/rooms/{room_id}/upgrade.  Returns the new room's
/// id (the server tombstones the old room and creates the successor).
/// `additional_creators` is honored from room version 12 onwards; older
/// servers silently drop it.
pub async fn upgrade_room(
    handle_id: u64,
    room_id: &str,
    new_version: &str,
    additional_creators: &[String],
) -> Result<String, String> {
    let client = client_for_handle(handle_id)?;
    let parsed_room_id = parse_room_id(room_id)?;
    let parsed_new_version = RoomVersionId::try_from(new_version.trim())
        .map_err(|e| format!("invalid room version '{}': {e}", new_version.trim()))?;

    let mut parsed_creators: Vec<OwnedUserId> = Vec::with_capacity(additional_creators.len());
    for raw in additional_creators {
        parsed_creators.push(parse_user_id(raw)?);
    }

    let mut request = upgrade_room::v3::Request::new(parsed_room_id, parsed_new_version);
    request.additional_creators = parsed_creators;

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        new_version = new_version.trim(),
        additional_creator_count = additional_creators.len(),
        "Upgrading room via matrix-sdk backend runtime"
    );

    let response = client
        .send(request)
        .await
        .map_err(|e| format!("failed to upgrade room via matrix-sdk: {e}"))?;

    Ok(response.replacement_room.to_string())
}
