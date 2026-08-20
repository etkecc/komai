// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};

use super::blocking::ffi_block_on;

pub(crate) fn matrix_fetch_room_settings(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<ffi::MatrixRoomSettings, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_settings",
        matrix_backend::runtime::fetch_room_settings(handle_id, room_id),
    )?;

    Ok(ffi::MatrixRoomSettings {
        room_id: result.room_id,
        room_name: result.room_name,
        room_topic: result.room_topic,
        room_avatar_url: result.room_avatar_url,
        room_version: result.room_version,
        member_count: result.member_count,
        notifications: result.notifications,
        join_rule: result.join_rule,
        history_visibility: result.history_visibility,
        allowed_room_ids: result.allowed_room_ids,
        parent_space_room_ids: result.parent_space_room_ids,
        guest_access: result.guest_access,
        is_encrypted: result.is_encrypted,
        can_change_name: result.can_change_name,
        can_change_topic: result.can_change_topic,
        can_change_avatar: result.can_change_avatar,
        can_change_join_rules: result.can_change_join_rules,
        can_change_history_visibility: result.can_change_history_visibility,
        can_change_encryption: result.can_change_encryption,
        can_upgrade_room: result.can_upgrade_room,
    })
}

pub(crate) fn matrix_fetch_room_versions_capability(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixRoomVersionsCapability, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_versions_capability",
        matrix_backend::runtime::fetch_room_versions_capability(handle_id),
    )?;

    Ok(ffi::MatrixRoomVersionsCapability {
        default_version: result.default_version,
        stable: result.stable,
    })
}

pub(crate) fn matrix_fetch_room_aliases(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<ffi::MatrixRoomAliases, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_aliases",
        matrix_backend::runtime::fetch_room_aliases(handle_id, room_id),
    )?;

    Ok(ffi::MatrixRoomAliases {
        canonical_alias: result.canonical_alias,
        alt_aliases: result.alt_aliases,
        published_aliases: result.published_aliases,
    })
}

pub(crate) fn matrix_apply_room_aliases(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    aliases: ffi::MatrixRoomAliases,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_apply_room_aliases",
        matrix_backend::runtime::apply_room_aliases(
            handle_id,
            room_id,
            matrix_backend::runtime::MatrixRoomAliases {
                canonical_alias: aliases.canonical_alias,
                alt_aliases: aliases.alt_aliases,
                published_aliases: aliases.published_aliases,
            },
        ),
    )
}

pub(crate) fn matrix_fetch_room_members(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<ffi::MatrixRoomMember>, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_members",
        matrix_backend::runtime::fetch_room_members(handle_id, room_id),
    )?;
    Ok(result
        .into_iter()
        .map(|member| ffi::MatrixRoomMember {
            user_id: member.user_id,
            display_name: member.display_name,
            avatar_url: member.avatar_url,
            power_level: member.power_level,
            is_invited: member.is_invited,
        })
        .collect())
}

pub(crate) fn matrix_fetch_room_power_levels(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<ffi::MatrixRoomPowerLevels, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_power_levels",
        matrix_backend::runtime::fetch_room_power_levels(handle_id, room_id),
    )?;

    Ok(ffi::MatrixRoomPowerLevels {
        room_version: result.room_version,
        creators: result.creators,
        events: result
            .events
            .into_iter()
            .map(|entry| ffi::MatrixPowerLevelEntry {
                key: entry.key,
                level: entry.level,
            })
            .collect(),
        users: result
            .users
            .into_iter()
            .map(|entry| ffi::MatrixPowerLevelEntry {
                key: entry.key,
                level: entry.level,
            })
            .collect(),
        ban: result.ban,
        events_default: result.events_default,
        invite: result.invite,
        kick: result.kick,
        redact: result.redact,
        state_default: result.state_default,
        users_default: result.users_default,
    })
}

pub(crate) fn matrix_fetch_room_child_spaces(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<ffi::MatrixChildSpaceEntry>, String> {
    let results = ffi_block_on(
        context,
        "matrix_fetch_room_child_spaces",
        matrix_backend::runtime::fetch_room_child_spaces(handle_id, room_id),
    )?;

    Ok(results
        .into_iter()
        .map(|entry| ffi::MatrixChildSpaceEntry {
            room_id: entry.room_id,
            display_name: entry.display_name,
            avatar_url: entry.avatar_url,
            power_levels: ffi::MatrixRoomPowerLevels {
                room_version: entry.power_levels.room_version,
                creators: entry.power_levels.creators,
                events: entry
                    .power_levels
                    .events
                    .into_iter()
                    .map(|e| ffi::MatrixPowerLevelEntry {
                        key: e.key,
                        level: e.level,
                    })
                    .collect(),
                users: entry
                    .power_levels
                    .users
                    .into_iter()
                    .map(|e| ffi::MatrixPowerLevelEntry {
                        key: e.key,
                        level: e.level,
                    })
                    .collect(),
                ban: entry.power_levels.ban,
                events_default: entry.power_levels.events_default,
                invite: entry.power_levels.invite,
                kick: entry.power_levels.kick,
                redact: entry.power_levels.redact,
                state_default: entry.power_levels.state_default,
                users_default: entry.power_levels.users_default,
            },
        })
        .collect())
}

pub(crate) fn matrix_apply_room_power_levels(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    power_levels: ffi::MatrixRoomPowerLevels,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_apply_room_power_levels",
        matrix_backend::runtime::apply_room_power_levels(
            handle_id,
            room_id,
            matrix_backend::runtime::MatrixRoomPowerLevels {
                room_version: power_levels.room_version,
                creators: power_levels.creators,
                events: power_levels
                    .events
                    .into_iter()
                    .map(|entry| matrix_backend::runtime::MatrixPowerLevelEntry {
                        key: entry.key,
                        level: entry.level,
                    })
                    .collect(),
                users: power_levels
                    .users
                    .into_iter()
                    .map(|entry| matrix_backend::runtime::MatrixPowerLevelEntry {
                        key: entry.key,
                        level: entry.level,
                    })
                    .collect(),
                ban: power_levels.ban,
                events_default: power_levels.events_default,
                invite: power_levels.invite,
                kick: power_levels.kick,
                redact: power_levels.redact,
                state_default: power_levels.state_default,
                users_default: power_levels.users_default,
            },
        ),
    )
}

pub(crate) fn matrix_fetch_media_content(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    mxc_uri: &str,
    width: i32,
    height: i32,
    crop: bool,
) -> Result<Vec<u8>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_media_content",
        matrix_backend::runtime::fetch_media_content(handle_id, mxc_uri, width, height, crop),
    )
}

pub(crate) fn matrix_set_room_notification_mode(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    mode: i32,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_room_notification_mode",
        matrix_backend::runtime::set_room_notification_mode(handle_id, room_id, mode),
    )
}

pub(crate) fn matrix_set_room_name(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    name: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_room_name",
        matrix_backend::runtime::set_room_name(handle_id, room_id, name),
    )
}

pub(crate) fn matrix_set_room_topic(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    topic: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_room_topic",
        matrix_backend::runtime::set_room_topic(handle_id, room_id, topic),
    )
}

pub(crate) fn matrix_fetch_room_state_event(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_type: &str,
    state_key: &str,
) -> Result<ffi::MatrixRoomStateEvent, String> {
    ffi_block_on(
        context,
        "matrix_fetch_room_state_event",
        matrix_backend::runtime::fetch_room_state_event(handle_id, room_id, event_type, state_key),
    )
    .map(|event| ffi::MatrixRoomStateEvent {
        exists: event.exists,
        content_json: event.content_json,
    })
}

pub(crate) fn matrix_send_room_state_event(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_type: &str,
    state_key: &str,
    content_json: &str,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_send_room_state_event",
        matrix_backend::runtime::send_room_state_event(
            handle_id,
            room_id,
            event_type,
            state_key,
            content_json,
        ),
    )
}

pub(crate) fn matrix_upload_room_avatar(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    file_path: &str,
    mime_type: &str,
    width: i32,
    height: i32,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_upload_room_avatar",
        matrix_backend::runtime::upload_room_avatar(
            handle_id,
            room_id,
            file_path,
            mime_type,
            width,
            height,
        ),
    )
}

pub(crate) fn matrix_remove_room_avatar(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_remove_room_avatar",
        matrix_backend::runtime::remove_room_avatar(handle_id, room_id),
    )
}

pub(crate) fn matrix_enable_room_encryption(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_enable_room_encryption",
        matrix_backend::runtime::enable_room_encryption(handle_id, room_id),
    )
}

pub(crate) fn matrix_set_room_history_visibility(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    history_visibility: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_room_history_visibility",
        matrix_backend::runtime::set_room_history_visibility(handle_id, room_id, history_visibility),
    )
}

pub(crate) fn matrix_set_room_access_rules(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    join_rule_kind: &str,
    guest_access: bool,
    allowed_room_ids: &Vec<String>,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_room_access_rules",
        matrix_backend::runtime::set_room_access_rules(
            handle_id,
            room_id,
            join_rule_kind,
            guest_access,
            allowed_room_ids,
        ),
    )
}
