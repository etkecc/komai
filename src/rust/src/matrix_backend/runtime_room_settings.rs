// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::fs;

use super::*;
use serde_json::Value as JsonValue;
use matrix_sdk::{
    deserialized_responses::SyncOrStrippedState,
    notification_settings::RoomNotificationMode,
    RoomMemberships,
    room::ParentSpace,
    ruma::{
        OwnedRoomAliasId, RoomAliasId,
        UInt,
        api::client::{
            discovery::get_capabilities::v3::RoomVersionStability,
            room::aliases as room_aliases,
            state::get_state_event_for_key,
        },
        api::error::ErrorKind,
        events::{
            SyncStateEvent,
            StateEventType,
            room::{
                avatar::ImageInfo,
                encryption::RoomEncryptionEventContent,
                guest_access::{GuestAccess, RoomGuestAccessEventContent},
                history_visibility::{HistoryVisibility, RoomHistoryVisibilityEventContent},
                join_rules::{AllowRule, JoinRule, RoomJoinRulesEventContent},
                power_levels::RoomPowerLevelsEventContent,
                power_levels::UserPowerLevel,
            },
            space::child::SpaceChildEventContent,
        },
    },
};
use mime::Mime;

fn notification_mode_to_int(mode: &RoomNotificationMode) -> i32 {
    match mode {
        RoomNotificationMode::Mute => 0,
        RoomNotificationMode::MentionsAndKeywordsOnly => 1,
        RoomNotificationMode::AllMessages => 2,
    }
}

fn notification_mode_from_int(mode: i32) -> RoomNotificationMode {
    match mode {
        0 => RoomNotificationMode::Mute,
        1 => RoomNotificationMode::MentionsAndKeywordsOnly,
        _ => RoomNotificationMode::AllMessages,
    }
}

fn history_visibility_to_key(value: HistoryVisibility) -> String {
    match value {
        HistoryVisibility::WorldReadable => "world_readable".to_owned(),
        HistoryVisibility::Shared => "shared".to_owned(),
        HistoryVisibility::Invited => "invited".to_owned(),
        HistoryVisibility::Joined => "joined".to_owned(),
        HistoryVisibility::_Custom(_) => "shared".to_owned(),
        _ => "shared".to_owned(),
    }
}

fn history_visibility_from_key(value: &str) -> HistoryVisibility {
    match value.trim() {
        "world_readable" => HistoryVisibility::WorldReadable,
        "invited" => HistoryVisibility::Invited,
        "joined" => HistoryVisibility::Joined,
        _ => HistoryVisibility::Shared,
    }
}

fn default_join_rule() -> JoinRule {
    JoinRule::Invite
}

fn extract_allowed_room_ids(join_rule: &JoinRule) -> Vec<String> {
    match join_rule {
        JoinRule::Restricted(restricted) | JoinRule::KnockRestricted(restricted) => restricted
            .allow
            .iter()
            .filter_map(|rule| match rule {
                AllowRule::RoomMembership(room_membership) => {
                    Some(room_membership.room_id.to_string())
                }
                AllowRule::_Custom(_) => None,
                _ => None,
            })
            .collect(),
        _ => Vec::new(),
    }
}

fn build_join_rule(kind: &str, allowed_room_ids: &[String]) -> Result<JoinRule, String> {
    let allow = allowed_room_ids
        .iter()
        .map(|room_id| parse_room_id(room_id).map(AllowRule::room_membership))
        .collect::<Result<Vec<_>, _>>()?;

    Ok(match kind.trim() {
        "public" => JoinRule::Public,
        "knock" => JoinRule::Knock,
        "private" => JoinRule::Private,
        "restricted" => JoinRule::Restricted(matrix_sdk::ruma::room::Restricted::new(allow)),
        "knock_restricted" => {
            JoinRule::KnockRestricted(matrix_sdk::ruma::room::Restricted::new(allow))
        }
        _ => JoinRule::Invite,
    })
}

async fn load_join_rule(room: &Room) -> Result<JoinRule, String> {
    let Some(raw_event) = room
        .get_state_event_static::<RoomJoinRulesEventContent>()
        .await
        .map_err(|e| format!("failed to load room join rules from matrix-sdk: {e}"))?
    else {
        return Ok(default_join_rule());
    };

    let event = raw_event
        .deserialize()
        .map_err(|e| format!("failed to deserialize room join rules from matrix-sdk: {e}"))?;
    Ok(match event {
        matrix_sdk::deserialized_responses::SyncOrStrippedState::Sync(ev) => ev.join_rule().clone(),
        matrix_sdk::deserialized_responses::SyncOrStrippedState::Stripped(ev) => ev.content.join_rule,
    })
}

async fn load_guest_access(room: &Room) -> Result<GuestAccess, String> {
    let Some(raw_event) = room
        .get_state_event_static::<RoomGuestAccessEventContent>()
        .await
        .map_err(|e| format!("failed to load room guest access from matrix-sdk: {e}"))?
    else {
        return Ok(GuestAccess::Forbidden);
    };

    let event = raw_event
        .deserialize()
        .map_err(|e| format!("failed to deserialize room guest access from matrix-sdk: {e}"))?;
    Ok(match event {
        matrix_sdk::deserialized_responses::SyncOrStrippedState::Sync(ev) => {
            ev.guest_access().clone()
        }
        matrix_sdk::deserialized_responses::SyncOrStrippedState::Stripped(ev) => {
            ev.content.guest_access.unwrap_or(GuestAccess::Forbidden)
        }
    })
}

async fn load_history_visibility(room: &Room) -> Result<HistoryVisibility, String> {
    let Some(raw_event) = room
        .get_state_event_static::<RoomHistoryVisibilityEventContent>()
        .await
        .map_err(|e| format!("failed to load room history visibility from matrix-sdk: {e}"))?
    else {
        return Ok(HistoryVisibility::Shared);
    };

    let event = raw_event.deserialize().map_err(|e| {
        format!("failed to deserialize room history visibility from matrix-sdk: {e}")
    })?;
    Ok(match event {
        matrix_sdk::deserialized_responses::SyncOrStrippedState::Sync(ev) => {
            ev.history_visibility().clone()
        }
        matrix_sdk::deserialized_responses::SyncOrStrippedState::Stripped(ev) => {
            ev.content.history_visibility
        }
    })
}

async fn fetch_parent_space_room_ids(room: &Room) -> Vec<String> {
    let Ok(mut parents) = room.parent_spaces().await else {
        return Vec::new();
    };

    let mut room_ids = Vec::new();
    while let Some(parent) = parents.next().await {
        match parent {
            Ok(ParentSpace::Reciprocal(parent_room))
            | Ok(ParentSpace::WithPowerlevel(parent_room))
            | Ok(ParentSpace::Illegitimate(parent_room)) => {
                room_ids.push(parent_room.room_id().to_string());
            }
            Ok(ParentSpace::Unverifiable(parent_room_id)) => {
                room_ids.push(parent_room_id.to_string());
            }
            Err(error) => {
                tracing::debug!(room_id = room.room_id().as_str(), %error, "Failed to inspect room parent space");
            }
        }
    }

    room_ids.sort();
    room_ids.dedup();
    room_ids
}

fn parse_room_alias(alias: &str) -> Result<OwnedRoomAliasId, String> {
    RoomAliasId::parse(alias.trim()).map_err(|e| format!("invalid room alias '{alias}': {e}"))
}

async fn fetch_published_room_aliases(room: &Room) -> Result<Vec<OwnedRoomAliasId>, String> {
    let request = room_aliases::v3::Request::new(room.room_id().to_owned());
    let response = room
        .client()
        .send(request)
        .await
        .map_err(|e| format!("failed to fetch published room aliases via matrix-sdk: {e}"))?;

    Ok(response.aliases)
}

pub async fn fetch_room_power_levels(
    handle_id: u64,
    room_id: &str,
) -> Result<MatrixRoomPowerLevels, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let power_levels = room.power_levels_or_default().await;
    let room_info = room.clone_info();
    let creators = room
        .creators()
        .unwrap_or_default()
        .into_iter()
        .map(|creator| creator.to_string())
        .collect();

    Ok(MatrixRoomPowerLevels {
        room_version: room_info
            .room_version()
            .map(ToString::to_string)
            .unwrap_or_default(),
        creators,
        events: power_levels
            .events
            .iter()
            .map(|(key, level)| MatrixPowerLevelEntry {
                key: key.to_string(),
                level: (*level).into(),
            })
            .collect(),
        users: power_levels
            .users
            .iter()
            .map(|(key, level)| MatrixPowerLevelEntry {
                key: key.to_string(),
                level: (*level).into(),
            })
            .collect(),
        ban: power_levels.ban.into(),
        events_default: power_levels.events_default.into(),
        invite: power_levels.invite.into(),
        kick: power_levels.kick.into(),
        redact: power_levels.redact.into(),
        state_default: power_levels.state_default.into(),
        users_default: power_levels.users_default.into(),
    })
}

pub async fn apply_room_power_levels(
    handle_id: u64,
    room_id: &str,
    power_levels: MatrixRoomPowerLevels,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let mut current = room.power_levels_or_default().await;

    current.ban = power_levels
        .ban
        .try_into()
        .map_err(|e| format!("invalid ban power level: {e}"))?;
    current.events_default = power_levels
        .events_default
        .try_into()
        .map_err(|e| format!("invalid events_default power level: {e}"))?;
    current.invite = power_levels
        .invite
        .try_into()
        .map_err(|e| format!("invalid invite power level: {e}"))?;
    current.kick = power_levels
        .kick
        .try_into()
        .map_err(|e| format!("invalid kick power level: {e}"))?;
    current.redact = power_levels
        .redact
        .try_into()
        .map_err(|e| format!("invalid redact power level: {e}"))?;
    current.state_default = power_levels
        .state_default
        .try_into()
        .map_err(|e| format!("invalid state_default power level: {e}"))?;
    current.users_default = power_levels
        .users_default
        .try_into()
        .map_err(|e| format!("invalid users_default power level: {e}"))?;

    current.events.clear();
    for entry in power_levels.events {
        let level = entry
            .level
            .try_into()
            .map_err(|e| format!("invalid event power level for '{}': {e}", entry.key))?;
        current.events.insert(entry.key.as_str().into(), level);
    }

    current.users.clear();
    for entry in power_levels.users {
        let level = entry
            .level
            .try_into()
            .map_err(|e| format!("invalid user power level for '{}': {e}", entry.key))?;
        let user_id = parse_user_id(&entry.key)?;
        current.users.insert(user_id.to_owned(), level);
    }

    let content = RoomPowerLevelsEventContent::try_from(current)
        .map_err(|e| format!("failed to serialize room power levels: {e}"))?;
    room.send_state_event(content)
        .await
        .map_err(|e| format!("failed to apply room power levels via matrix-sdk: {e}"))?;

    Ok(())
}

pub async fn set_user_power_level(
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    power_level: i64,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let parsed_user_id = parse_user_id(user_id)?;
    let mut current = room.power_levels_or_default().await;

    let level = power_level
        .try_into()
        .map_err(|e| format!("invalid power level: {e}"))?;
    current.users.insert(parsed_user_id.to_owned(), level);

    let content = RoomPowerLevelsEventContent::try_from(current)
        .map_err(|e| format!("failed to serialize room power levels: {e}"))?;
    room.send_state_event(content)
        .await
        .map_err(|e| format!("failed to set user power level via matrix-sdk: {e}"))?;

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        user_id = user_id.trim(),
        power_level,
        "Set user power level via matrix-sdk backend runtime"
    );

    Ok(())
}

pub struct MatrixChildSpaceEntry {
    pub room_id: String,
    pub display_name: String,
    pub avatar_url: String,
    pub power_levels: MatrixRoomPowerLevels,
}

pub async fn fetch_room_child_spaces(
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<MatrixChildSpaceEntry>, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;

    let child_events = room
        .get_state_events_static::<SpaceChildEventContent>()
        .await
        .map_err(|e| format!("failed to fetch space child events: {e}"))?;

    let mut child_room_ids = Vec::new();
    for raw_event in child_events {
        let child_room_id: Option<String> = match raw_event.deserialize() {
            Ok(SyncOrStrippedState::Sync(SyncStateEvent::Original(e))) => {
                Some(e.state_key.to_string())
            }
            Ok(SyncOrStrippedState::Stripped(e)) => Some(e.state_key.to_string()),
            _ => None,
        };
        if let Some(id) = child_room_id {
            child_room_ids.push(id);
        }
    }

    let client = client_for_handle(handle_id)?;
    let mut results = Vec::new();

    for child_id in child_room_ids {
        let Ok(room_id) = child_id.as_str().try_into() else {
            continue;
        };
        let Some(child_room) = client.get_room(room_id) else {
            continue;
        };

        let power_levels = match fetch_room_power_levels(handle_id, &child_id).await {
            Ok(pl) => pl,
            Err(_) => continue,
        };

        let display_name = child_room
            .name()
            .unwrap_or_else(|| child_id.clone());

        let avatar_url = child_room
            .avatar_url()
            .map(|u| normalize_mxc_uri(u.to_string()))
            .unwrap_or_default();

        results.push(MatrixChildSpaceEntry {
            room_id: child_id,
            display_name,
            avatar_url,
            power_levels,
        });
    }

    Ok(results)
}

pub async fn fetch_room_settings(
    handle_id: u64,
    room_id: &str,
) -> Result<MatrixRoomSettings, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let client = client_for_handle(handle_id)?;
    let own_user_id = client
        .user_id()
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} has no logged-in user"))?
        .to_owned();

    let join_rule = load_join_rule(&room).await?;
    let guest_access = load_guest_access(&room).await?;
    let history_visibility = load_history_visibility(&room).await?;
    let power_levels = room.power_levels().await.ok();
    let room_info = room.clone_info();

    let notification_mode = room.notification_mode().await.unwrap_or(RoomNotificationMode::AllMessages);
    let parent_space_room_ids = fetch_parent_space_room_ids(&room).await;

    tracing::debug!(
        handle_id,
        room_id = room.room_id().as_str(),
        "Fetched room settings via matrix-sdk backend runtime"
    );

    Ok(MatrixRoomSettings {
        room_id: room.room_id().to_string(),
        room_name: room.name().unwrap_or_default(),
        room_topic: room.topic().unwrap_or_default(),
        room_avatar_url: room.avatar_url().map(|uri| uri.to_string()).unwrap_or_default(),
        room_version: room_info
            .room_version()
            .map(ToString::to_string)
            .unwrap_or_default(),
        member_count: room.active_members_count(),
        notifications: notification_mode_to_int(&notification_mode),
        join_rule: join_rule.as_str().to_owned(),
        history_visibility: history_visibility_to_key(history_visibility),
        allowed_room_ids: extract_allowed_room_ids(&join_rule),
        parent_space_room_ids,
        guest_access: guest_access == GuestAccess::CanJoin,
        is_encrypted: room.encryption_state().is_encrypted(),
        can_change_name: power_levels
            .as_ref()
            .is_some_and(|levels| levels.user_can_send_state(&own_user_id, StateEventType::RoomName)),
        can_change_topic: power_levels
            .as_ref()
            .is_some_and(|levels| levels.user_can_send_state(&own_user_id, StateEventType::RoomTopic)),
        can_change_avatar: power_levels
            .as_ref()
            .is_some_and(|levels| levels.user_can_send_state(&own_user_id, StateEventType::RoomAvatar)),
        can_change_join_rules: power_levels.as_ref().is_some_and(|levels| {
            levels.user_can_send_state(&own_user_id, StateEventType::RoomJoinRules)
        }),
        can_change_history_visibility: power_levels.as_ref().is_some_and(|levels| {
            levels.user_can_send_state(&own_user_id, StateEventType::RoomHistoryVisibility)
        }),
        can_change_encryption: power_levels.as_ref().is_some_and(|levels| {
            levels.user_can_send_state(&own_user_id, StateEventType::RoomEncryption)
        }),
        // Sending m.room.tombstone is the auth-rule gate the server enforces
        // on POST /rooms/{room_id}/upgrade, so the same power-level check
        // tells us whether the user can upgrade this room.
        can_upgrade_room: power_levels.as_ref().is_some_and(|levels| {
            levels.user_can_send_state(&own_user_id, StateEventType::RoomTombstone)
        }),
    })
}

/// Returns the homeserver's `m.room_versions` capability — its default room
/// version for new rooms plus the list of stable versions it supports.
/// matrix-sdk caches the `/capabilities` response in the state store, so
/// subsequent calls within a session are local.
pub async fn fetch_room_versions_capability(
    handle_id: u64,
) -> Result<MatrixRoomVersionsCapability, String> {
    let client = client_for_handle(handle_id)?;
    let caps = client
        .homeserver_capabilities()
        .room_versions()
        .await
        .map_err(|e| format!("failed to fetch room versions capability: {e}"))?;

    let default_version = caps.default.to_string();
    let mut stable: Vec<String> = caps
        .available
        .iter()
        .filter(|(_, stability)| matches!(stability, RoomVersionStability::Stable))
        .map(|(version, _)| version.to_string())
        .collect();

    // Some homeservers omit the default from `available`; make sure it shows
    // up in the dropdown so a user can re-select it.
    if !stable.iter().any(|v| v == &default_version) {
        stable.push(default_version.clone());
    }

    // Spec room versions are numeric strings ("1".."12" today); sort them
    // numerically so "10" doesn't sort before "2".  Custom non-numeric IDs
    // (e.g. unstable MSCs that some servers may still mark Stable) fall
    // back to lexicographic ordering after the numerics.
    stable.sort_by(|a, b| match (a.parse::<u32>().ok(), b.parse::<u32>().ok()) {
        (Some(an), Some(bn)) => an.cmp(&bn),
        (Some(_), None) => std::cmp::Ordering::Less,
        (None, Some(_)) => std::cmp::Ordering::Greater,
        (None, None) => a.cmp(b),
    });
    stable.dedup();

    Ok(MatrixRoomVersionsCapability {
        default_version,
        stable,
    })
}

pub async fn fetch_room_aliases(handle_id: u64, room_id: &str) -> Result<MatrixRoomAliases, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let published_aliases = fetch_published_room_aliases(&room).await?;

    Ok(MatrixRoomAliases {
        canonical_alias: room
            .canonical_alias()
            .map(|alias| alias.to_string())
            .unwrap_or_default(),
        alt_aliases: room.alt_aliases().into_iter().map(|alias| alias.to_string()).collect(),
        published_aliases: published_aliases
            .into_iter()
            .map(|alias| alias.to_string())
            .collect(),
    })
}

pub async fn apply_room_aliases(
    handle_id: u64,
    room_id: &str,
    aliases: MatrixRoomAliases,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let client = client_for_handle(handle_id)?;

    let current_canonical_alias = room
        .canonical_alias()
        .map(|alias| alias.to_string())
        .unwrap_or_default();
    let mut current_alt_aliases: Vec<String> =
        room.alt_aliases().into_iter().map(|alias| alias.to_string()).collect();
    current_alt_aliases.sort();
    current_alt_aliases.dedup();
    let current_published_aliases = fetch_published_room_aliases(&room).await?;

    let mut desired_published_aliases = aliases
        .published_aliases
        .iter()
        .map(|alias| parse_room_alias(alias))
        .collect::<Result<Vec<_>, _>>()?;
    desired_published_aliases.sort();
    desired_published_aliases.dedup();

    for alias in &current_published_aliases {
        if !desired_published_aliases.contains(alias) {
            client
                .remove_room_alias(alias.as_ref())
                .await
                .map_err(|e| format!("failed to remove room alias '{}' via matrix-sdk: {e}", alias))?;
        }
    }

    for alias in &desired_published_aliases {
        if !current_published_aliases.contains(alias) {
            client
                .create_room_alias(alias.as_ref(), room.room_id())
                .await
                .map_err(|e| format!("failed to create room alias '{}' via matrix-sdk: {e}", alias))?;
        }
    }

    let mut desired_alt_alias_strings = aliases.alt_aliases.clone();
    desired_alt_alias_strings.sort();
    desired_alt_alias_strings.dedup();

    let desired_alt_aliases = desired_alt_alias_strings
        .iter()
        .map(|alias| parse_room_alias(alias))
        .collect::<Result<Vec<_>, _>>()?;

    let desired_canonical_alias = if aliases.canonical_alias.trim().is_empty() {
        None
    } else {
        Some(parse_room_alias(&aliases.canonical_alias)?)
    };

    let canonical_changed = current_canonical_alias != aliases.canonical_alias;
    let alt_aliases_changed = current_alt_aliases != desired_alt_alias_strings;
    if canonical_changed || alt_aliases_changed {
        room.privacy_settings()
            .update_canonical_alias(desired_canonical_alias, desired_alt_aliases)
            .await
            .map_err(|e| format!("failed to update room canonical aliases via matrix-sdk: {e}"))?;
    }

    Ok(())
}

pub async fn fetch_room_members(
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<MatrixRoomMember>, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    // Force a fresh /members request. With sliding sync the SDK persists
    // members_synced = true after the first fetch, so subsequent calls skip
    // the server and return stale data from the state store.
    room.mark_members_missing();
    let mut members = room
        .members(RoomMemberships::ACTIVE)
        .await
        .map_err(|e| format!("failed to fetch matrix-sdk room members: {e}"))?;

    members.sort_by(|a, b| {
        b.power_level()
            .cmp(&a.power_level())
            .then_with(|| {
                a.display_name()
                    .unwrap_or(a.user_id().as_str())
                    .cmp(b.display_name().unwrap_or(b.user_id().as_str()))
            })
            .then_with(|| a.user_id().cmp(b.user_id()))
    });

    Ok(members
        .into_iter()
        .map(|member| {
            use matrix_sdk::ruma::events::room::member::MembershipState;
            MatrixRoomMember {
                user_id: member.user_id().to_string(),
                display_name: member
                    .display_name()
                    .map(ToOwned::to_owned)
                    .unwrap_or_else(|| member.user_id().to_string()),
                avatar_url: member
                    .avatar_url()
                    .map(|uri| normalize_mxc_uri(uri.to_string()))
                    .unwrap_or_default(),
                power_level: match member.power_level() {
                    UserPowerLevel::Int(value) => i64::from(value),
                    UserPowerLevel::Infinite => i64::MAX,
                    _ => 0,
                },
                is_invited: member.membership() == &MembershipState::Invite,
            }
        })
        .collect())
}

pub async fn set_room_notification_mode(
    handle_id: u64,
    room_id: &str,
    mode: i32,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let client = client_for_handle(handle_id)?;
    let settings = client.notification_settings().await;
    let mode = notification_mode_from_int(mode);

    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        notification_mode = notification_mode_to_int(&mode),
        "Updating room notification mode via matrix-sdk backend runtime"
    );

    settings
        .set_room_notification_mode(room.room_id(), mode)
        .await
        .map_err(|e| format!("failed to update room notification mode via matrix-sdk: {e}"))
}

pub async fn set_room_name(handle_id: u64, room_id: &str, name: &str) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;

    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        has_name = !name.trim().is_empty(),
        "Updating room name via matrix-sdk backend runtime"
    );

    room.set_name(name.trim().to_owned())
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to update room name via matrix-sdk: {e}"))
}

pub async fn set_room_topic(handle_id: u64, room_id: &str, topic: &str) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;

    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        has_topic = !topic.trim().is_empty(),
        "Updating room topic via matrix-sdk backend runtime"
    );

    room.set_room_topic(topic.trim())
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to update room topic via matrix-sdk: {e}"))
}

pub async fn upload_room_avatar(
    handle_id: u64,
    room_id: &str,
    file_path: &str,
    mime_type: &str,
    width: i32,
    height: i32,
) -> Result<String, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let client = client_for_handle(handle_id)?;
    let file_path = file_path.trim();
    let mime: Mime = mime_type
        .trim()
        .parse()
        .map_err(|e| format!("invalid avatar mime type '{mime_type}': {e}"))?;
    let data = fs::read(file_path)
        .map_err(|e| format!("failed to read avatar file '{file_path}': {e}"))?;

    let mut info = ImageInfo::new();
    info.width = (width > 0)
        .then(|| UInt::new(width as u64))
        .flatten();
    info.height = (height > 0)
        .then(|| UInt::new(height as u64))
        .flatten();
    info.size = UInt::new(data.len() as u64);

    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        file_path,
        mime_type,
        byte_count = data.len(),
        "Uploading room avatar via matrix-sdk backend runtime"
    );

    let upload_response = client
        .media()
        .upload(&mime, data, None)
        .await
        .map_err(|e| format!("failed to upload room avatar media via matrix-sdk: {e}"))?;

    let content_uri = upload_response.content_uri.clone();
    info.blurhash = upload_response.blurhash;
    info.mimetype = Some(mime.to_string());

    room.set_avatar_url(&upload_response.content_uri, Some(info))
        .await
        .map_err(|e| format!("failed to set room avatar state event via matrix-sdk: {e}"))?;

    Ok(content_uri.to_string())
}

pub async fn remove_room_avatar(handle_id: u64, room_id: &str) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;

    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        "Removing room avatar via matrix-sdk backend runtime"
    );

    room.remove_avatar()
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to remove room avatar via matrix-sdk: {e}"))
}

pub async fn enable_room_encryption(handle_id: u64, room_id: &str) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;

    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        "Enabling room encryption via matrix-sdk backend runtime"
    );

    room.send_state_event(RoomEncryptionEventContent::with_recommended_defaults())
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to enable room encryption via matrix-sdk: {e}"))
}

pub async fn set_room_history_visibility(
    handle_id: u64,
    room_id: &str,
    history_visibility: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let history_visibility = history_visibility_from_key(history_visibility);

    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        history_visibility = history_visibility_to_key(history_visibility.clone()),
        "Updating room history visibility via matrix-sdk backend runtime"
    );

    room.privacy_settings()
        .update_room_history_visibility(history_visibility)
        .await
        .map_err(|e| format!("failed to update room history visibility via matrix-sdk: {e}"))
}

pub async fn set_room_access_rules(
    handle_id: u64,
    room_id: &str,
    join_rule_kind: &str,
    guest_access: bool,
    allowed_room_ids: &[String],
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let join_rule = build_join_rule(join_rule_kind, allowed_room_ids)?;
    let guest_access = if guest_access {
        GuestAccess::CanJoin
    } else {
        GuestAccess::Forbidden
    };

    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        join_rule = join_rule.as_str(),
        guest_access = matches!(guest_access, GuestAccess::CanJoin),
        allowed_room_count = allowed_room_ids.len(),
        "Updating room access rules via matrix-sdk backend runtime"
    );

    room.privacy_settings()
        .update_join_rule(join_rule)
        .await
        .map_err(|e| format!("failed to update room join rules via matrix-sdk: {e}"))?;

    room.send_state_event(RoomGuestAccessEventContent::new(guest_access))
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to update room guest access via matrix-sdk: {e}"))
}

/// Result of reading one state event. `exists` is false when the room simply
/// has no such state, which is a normal answer rather than an error.
pub struct MatrixRoomStateEvent {
    pub exists: bool,
    pub content_json: String,
}

/// Reads one state event's content straight from the homeserver.
///
/// This deliberately does not use the local state store. Komai syncs via
/// sliding sync, so the store only holds the `required_state` types the room
/// list asks for; any other type -- notably a custom one -- would read back as
/// missing even though the room has it.
pub async fn fetch_room_state_event(
    handle_id: u64,
    room_id: &str,
    event_type: &str,
    state_key: &str,
) -> Result<MatrixRoomStateEvent, String> {
    let client = client_for_handle(handle_id)?;
    let parsed_room_id = parse_room_id(room_id)?;

    let event_type = event_type.trim();
    if event_type.is_empty() {
        return Err("cannot read a room state event without an event type".to_owned());
    }

    let request = get_state_event_for_key::v3::Request::new(
        parsed_room_id,
        event_type.into(),
        state_key.to_owned(),
    );

    match client.send(request).await {
        Ok(response) => Ok(MatrixRoomStateEvent {
            exists: true,
            content_json: response.event_or_content.get().to_owned(),
        }),
        // A room without this state answers M_NOT_FOUND, which is an answer,
        // not a failure. Anything else is a real error.
        Err(error) if matches!(error.client_api_error_kind(), Some(ErrorKind::NotFound)) => {
            Ok(MatrixRoomStateEvent {
                exists: false,
                content_json: String::new(),
            })
        }
        Err(error) => Err(format!(
            "failed to read room state event '{event_type}': {error}"
        )),
    }
}

/// Sends a state event with caller-supplied content. Returns the event ID.
///
/// The content replaces the event wholesale; it is not merged with what is
/// already there. For `m.room.power_levels` in particular, prefer
/// set_user_power_level, which reads first.
pub async fn send_room_state_event(
    handle_id: u64,
    room_id: &str,
    event_type: &str,
    state_key: &str,
    content_json: &str,
) -> Result<String, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;

    let event_type = event_type.trim();
    if event_type.is_empty() {
        return Err("cannot send a room state event without an event type".to_owned());
    }

    let content: JsonValue = serde_json::from_str(content_json)
        .map_err(|e| format!("invalid room state event content json: {e}"))?;
    if !content.is_object() {
        return Err("room state event content must be a json object".to_owned());
    }

    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        event_type,
        has_state_key = !state_key.is_empty(),
        "Sending room state event via matrix-sdk backend runtime"
    );

    room.send_state_event_raw(event_type, state_key, content)
        .await
        .map(|response| response.event_id.to_string())
        .map_err(|e| format!("failed to send room state event '{event_type}': {e}"))
}
