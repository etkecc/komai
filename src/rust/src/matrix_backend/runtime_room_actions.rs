// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use std::str::FromStr;

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

    if is_space {
        let mut creation_content = create_room::v3::CreationContent::new();
        creation_content.room_type = Some(RoomType::Space);
        request.creation_content =
            Some(Raw::new(&creation_content).expect("ruma creation content should serialize"));
    }

    if is_encrypted {
        request.initial_state.push(
            InitialStateEvent::with_empty_state_key(
                RoomEncryptionEventContent::with_recommended_defaults(),
            )
            .to_raw_any(),
        );
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
    let mut request = leave_room::v3::Request::new(parsed_room_id);
    request.reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        has_reason = request.reason.is_some(),
        "Leaving room via matrix-sdk backend runtime"
    );

    client
        .send(request)
        .await
        .map(|_| ())
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

pub async fn invite_user(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let parsed_room_id = parse_room_id(room_id)?;
    let parsed_user_id = parse_user_id(user_id)?;
    let mut request = invite_user::v3::Request::new(
        parsed_room_id,
        invite_user::v3::InvitationRecipient::UserId {
            user_id: parsed_user_id,
        },
    );
    request.reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        user_id = user_id.trim(),
        has_reason = request.reason.is_some(),
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
    let client = client_for_handle(handle_id)?;
    let parsed_room_id = parse_room_id(room_id)?;
    let parsed_user_id = parse_user_id(user_id)?;
    let mut request = kick_user::v3::Request::new(parsed_room_id, parsed_user_id);
    request.reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        user_id = user_id.trim(),
        has_reason = request.reason.is_some(),
        "Kicking user via matrix-sdk backend runtime"
    );

    client
        .send(request)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to kick user via matrix-sdk: {e}"))
}

pub async fn ban_user(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let parsed_room_id = parse_room_id(room_id)?;
    let parsed_user_id = parse_user_id(user_id)?;
    let mut request = ban_user::v3::Request::new(parsed_room_id, parsed_user_id);
    request.reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        user_id = user_id.trim(),
        has_reason = request.reason.is_some(),
        "Banning user via matrix-sdk backend runtime"
    );

    client
        .send(request)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to ban user via matrix-sdk: {e}"))
}

pub async fn unban_user(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let parsed_room_id = parse_room_id(room_id)?;
    let parsed_user_id = parse_user_id(user_id)?;
    let mut request = unban_user::v3::Request::new(parsed_room_id, parsed_user_id);
    request.reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        user_id = user_id.trim(),
        has_reason = request.reason.is_some(),
        "Unbanning user via matrix-sdk backend runtime"
    );

    client
        .send(request)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to unban user via matrix-sdk: {e}"))
}
