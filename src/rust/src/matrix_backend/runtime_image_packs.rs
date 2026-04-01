// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::{BTreeMap, BTreeSet};

use super::*;
use matrix_sdk::{Room, room::ParentSpace, ruma::events::SyncStateEvent};
use ruma::{MxcUri, events::image_pack::{
    AccountImagePackEventContent, ImagePackRoomContent, ImagePackRoomsEventContent, PackImage,
    PackInfo, PackUsage, RoomImagePackEventContent,
}};

fn pack_usage_allows(usage: &BTreeSet<PackUsage>, target: PackUsage) -> bool {
    usage.is_empty() || usage.contains(&target)
}

fn image_usage_allows(
    image_usage: &BTreeSet<PackUsage>,
    pack_usage: &BTreeSet<PackUsage>,
    target: PackUsage,
) -> bool {
    if image_usage.is_empty() {
        pack_usage_allows(pack_usage, target)
    } else {
        image_usage.contains(&target)
    }
}

fn pack_info_has_content(pack_info: Option<&PackInfo>) -> bool {
    let Some(pack_info) = pack_info else {
        return false;
    };

    !pack_info.usage.is_empty()
        || pack_info
            .display_name
            .as_ref()
            .is_some_and(|value| !value.trim().is_empty())
        || pack_info.avatar_url.is_some()
        || pack_info
            .attribution
            .as_ref()
            .is_some_and(|value| !value.trim().is_empty())
}

fn pack_has_content(images: &BTreeMap<String, PackImage>, pack_info: Option<&PackInfo>) -> bool {
    !images.is_empty() || pack_info_has_content(pack_info)
}

fn pack_display_name(room: Option<&Room>, state_key: &str, pack_info: Option<&PackInfo>) -> String {
    if let Some(name) = pack_info
        .and_then(|info| info.display_name.as_ref())
        .map(|value| value.trim())
        .filter(|value| !value.is_empty())
    {
        return name.to_owned();
    }

    if let Some(name) = room
        .and_then(|candidate| candidate.name())
        .map(|value| value.trim().to_owned())
        .filter(|value| !value.is_empty())
    {
        return name;
    }

    if !state_key.trim().is_empty() {
        return state_key.to_owned();
    }

    room.map(|candidate| candidate.room_id().to_string())
        .unwrap_or_default()
}

fn pack_avatar_url(room: Option<&Room>, pack_info: Option<&PackInfo>) -> String {
    if let Some(url) = pack_info.and_then(|info| info.avatar_url.as_ref()) {
        return url.to_string();
    }

    room.and_then(|candidate| candidate.avatar_url().map(|url| url.to_string()))
        .unwrap_or_default()
}

fn into_matrix_image_pack(
    room: Option<&Room>,
    source_room_id: String,
    state_key: String,
    from_space: bool,
    is_globally_enabled: bool,
    images: BTreeMap<String, PackImage>,
    pack_info: Option<PackInfo>,
) -> Option<MatrixImagePack> {
    if !pack_has_content(&images, pack_info.as_ref()) {
        return None;
    }

    let pack_usage = pack_info
        .as_ref()
        .map(|info| &info.usage)
        .cloned()
        .unwrap_or_default();
    let is_emote_pack = pack_usage_allows(&pack_usage, PackUsage::Emoticon);
    let is_sticker_pack = pack_usage_allows(&pack_usage, PackUsage::Sticker);

    Some(MatrixImagePack {
        source_room_id,
        state_key: state_key.clone(),
        display_name: pack_display_name(room, &state_key, pack_info.as_ref()),
        avatar_url: pack_avatar_url(room, pack_info.as_ref()),
        attribution: pack_info
            .and_then(|info| info.attribution)
            .unwrap_or_default(),
        is_emote_pack,
        is_sticker_pack,
        from_space,
        is_globally_enabled,
        images: images
            .into_iter()
            .map(|(shortcode, image)| MatrixImagePackImage {
                shortcode,
                body: image.body.unwrap_or_default(),
                url: image.url.to_string(),
                is_emote: image_usage_allows(&image.usage, &pack_usage, PackUsage::Emoticon),
                is_sticker: image_usage_allows(&image.usage, &pack_usage, PackUsage::Sticker),
            })
            .collect(),
    })
}

fn pack_usage_from_matrix_pack(pack: &MatrixImagePack) -> BTreeSet<PackUsage> {
    let mut usage = BTreeSet::new();
    if pack.is_emote_pack {
        usage.insert(PackUsage::Emoticon);
    }
    if pack.is_sticker_pack {
        usage.insert(PackUsage::Sticker);
    }
    usage
}

fn parse_pack_image_url(url: &str) -> Result<ruma::OwnedMxcUri, String> {
    let trimmed = url.trim();
    if trimmed.is_empty() {
        return Err("image pack image url cannot be empty".to_owned());
    }

    let uri = Box::<MxcUri>::from(trimmed);
    uri.validate()
        .map_err(|error| format!("invalid image pack mxc uri '{trimmed}': {error}"))?;
    Ok(uri.to_owned().into())
}

fn pack_info_from_matrix_pack(pack: &MatrixImagePack) -> Result<Option<PackInfo>, String> {
    let mut pack_info = PackInfo::new();
    let trimmed_display_name = pack.display_name.trim();
    if !trimmed_display_name.is_empty() {
        pack_info.display_name = Some(trimmed_display_name.to_owned());
    }

    let trimmed_avatar_url = pack.avatar_url.trim();
    if !trimmed_avatar_url.is_empty() {
        pack_info.avatar_url = Some(parse_pack_image_url(trimmed_avatar_url)?);
    }

    let pack_usage = pack_usage_from_matrix_pack(pack);
    if !pack_usage.is_empty() {
        pack_info.usage = pack_usage;
    }

    let trimmed_attribution = pack.attribution.trim();
    if !trimmed_attribution.is_empty() {
        pack_info.attribution = Some(trimmed_attribution.to_owned());
    }

    if pack_info_has_content(Some(&pack_info)) {
        Ok(Some(pack_info))
    } else {
        Ok(None)
    }
}

fn pack_images_from_matrix_pack(pack: &MatrixImagePack) -> Result<BTreeMap<String, PackImage>, String> {
    let pack_usage = pack_usage_from_matrix_pack(pack);
    let mut images = BTreeMap::new();

    for image in &pack.images {
        let trimmed_shortcode = image.shortcode.trim();
        if trimmed_shortcode.is_empty() {
            return Err("image pack shortcode cannot be empty".to_owned());
        }

        let mut usage = BTreeSet::new();
        if image.is_emote {
            usage.insert(PackUsage::Emoticon);
        }
        if image.is_sticker {
            usage.insert(PackUsage::Sticker);
        }
        if usage == pack_usage {
            usage.clear();
        }

        let mut pack_image = PackImage::new(parse_pack_image_url(&image.url)?);
        pack_image.body = {
            let trimmed_body = image.body.trim();
            if trimmed_body.is_empty() {
                None
            } else {
                Some(trimmed_body.to_owned())
            }
        };
        pack_image.usage = usage;
        images.insert(trimmed_shortcode.to_owned(), pack_image);
    }

    Ok(images)
}

fn account_pack_content_from_matrix_pack(
    pack: &MatrixImagePack,
) -> Result<AccountImagePackEventContent, String> {
    let mut content = AccountImagePackEventContent::new(pack_images_from_matrix_pack(pack)?);
    content.pack = pack_info_from_matrix_pack(pack)?;
    Ok(content)
}

fn room_pack_content_from_matrix_pack(
    pack: &MatrixImagePack,
) -> Result<RoomImagePackEventContent, String> {
    let mut content = RoomImagePackEventContent::new(pack_images_from_matrix_pack(pack)?);
    content.pack = pack_info_from_matrix_pack(pack)?;
    Ok(content)
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
                tracing::debug!(
                    room_id = room.room_id().as_str(),
                    %error,
                    "Failed to inspect room parent space while loading image packs"
                );
            }
        }
    }

    room_ids.sort();
    room_ids.dedup();
    room_ids
}

fn enabled_room_pack_keys(
    content: Option<ImagePackRoomsEventContent>,
) -> BTreeMap<String, BTreeSet<String>> {
    let mut enabled = BTreeMap::new();

    let Some(content) = content else {
        return enabled;
    };

    for (room_id, packs) in content.rooms {
        enabled.insert(room_id.to_string(), packs.into_keys().collect());
    }

    enabled
}

async fn load_enabled_room_packs(client: &Client) -> Result<ImagePackRoomsEventContent, String> {
    client
        .account()
        .account_data::<ImagePackRoomsEventContent>()
        .await
        .map_err(|e| format!("failed to load matrix-sdk enabled image-pack rooms from storage: {e}"))?
        .map(|raw| {
            raw.deserialize()
                .map_err(|e| format!("failed to deserialize matrix-sdk enabled image-pack rooms: {e}"))
        })
        .transpose()
        .map(|content| content.unwrap_or_default())
}

async fn store_enabled_room_packs(
    client: &Client,
    content: ImagePackRoomsEventContent,
) -> Result<(), String> {
    client
        .account()
        .set_account_data(content)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to update matrix-sdk enabled image-pack rooms: {e}"))
}

fn set_room_pack_enabled(
    content: &mut ImagePackRoomsEventContent,
    room_id: &ruma::OwnedRoomId,
    state_key: &str,
    enabled: bool,
) {
    if enabled {
        content
            .rooms
            .entry(room_id.clone())
            .or_default()
            .insert(state_key.to_owned(), ImagePackRoomContent::new());
        return;
    }

    if let Some(room_packs) = content.rooms.get_mut(room_id) {
        room_packs.remove(state_key);
        if room_packs.is_empty() {
            content.rooms.remove(room_id);
        }
    }
}

async fn update_enabled_room_packs(
    client: &Client,
    room_id: &str,
    previous_state_key: Option<&str>,
    new_state_key: Option<&str>,
) -> Result<(), String> {
    let parsed_room_id = parse_room_id(room_id)?;
    let mut enabled_room_packs = load_enabled_room_packs(client).await?;

    if let Some(previous_state_key) = previous_state_key {
        set_room_pack_enabled(&mut enabled_room_packs, &parsed_room_id, previous_state_key, false);
    }
    if let Some(new_state_key) = new_state_key {
        set_room_pack_enabled(&mut enabled_room_packs, &parsed_room_id, new_state_key, true);
    }

    store_enabled_room_packs(client, enabled_room_packs).await
}

async fn fetch_account_pack(client: &Client) -> Result<Option<MatrixImagePack>, String> {
    let maybe_pack = client
        .account()
        .account_data::<AccountImagePackEventContent>()
        .await
        .map_err(|e| format!("failed to load matrix-sdk account image pack from storage: {e}"))?;

    let Some(raw_pack) = maybe_pack else {
        return Ok(None);
    };

    let content = raw_pack
        .deserialize()
        .map_err(|e| format!("failed to deserialize matrix-sdk account image pack: {e}"))?;

    Ok(into_matrix_image_pack(
        None,
        String::new(),
        String::new(),
        false,
        false,
        content.images,
        content.pack,
    ))
}

async fn fetch_room_packs(
    room: &Room,
    from_space: bool,
    enabled_state_keys: &BTreeSet<String>,
) -> Result<Vec<MatrixImagePack>, String> {
    let raw_events = room
        .get_state_events_static::<RoomImagePackEventContent>()
        .await
        .map_err(|e| {
            format!(
                "failed to load matrix-sdk room image packs from storage for '{}': {e}",
                room.room_id()
            )
        })?;

    let mut packs = Vec::new();
    for raw_event in raw_events {
        let event = raw_event.deserialize().map_err(|e| {
            format!(
                "failed to deserialize matrix-sdk room image pack for '{}': {e}",
                room.room_id()
            )
        })?;

        let (state_key, content): (String, RoomImagePackEventContent) = match event {
            matrix_sdk::deserialized_responses::SyncOrStrippedState::Sync(
                SyncStateEvent::Original(event),
            ) => (event.state_key.to_owned(), event.content),
            matrix_sdk::deserialized_responses::SyncOrStrippedState::Sync(
                SyncStateEvent::Redacted(_),
            ) => continue,
            matrix_sdk::deserialized_responses::SyncOrStrippedState::Stripped(_) => continue,
        };

        if let Some(pack) = into_matrix_image_pack(
            Some(room),
            room.room_id().to_string(),
            state_key.clone(),
            from_space,
            enabled_state_keys.contains(&state_key),
            content.images,
            content.pack,
        ) {
            packs.push(pack);
        }
    }

    Ok(packs)
}

pub async fn fetch_image_packs(handle_id: u64, room_id: &str) -> Result<Vec<MatrixImagePack>, String> {
    let client = client_for_handle(handle_id)?;
    let current_room = joined_room_for_handle(handle_id, room_id)?;

    let enabled_room_pack_map = enabled_room_pack_keys(Some(load_enabled_room_packs(&client).await?));

    let parent_space_room_ids = fetch_parent_space_room_ids(&current_room).await;
    let parent_space_room_id_set = parent_space_room_ids.iter().cloned().collect::<BTreeSet<_>>();

    let mut room_ids_to_fetch = BTreeSet::new();
    room_ids_to_fetch.insert(current_room.room_id().to_string());
    room_ids_to_fetch.extend(parent_space_room_ids);
    room_ids_to_fetch.extend(enabled_room_pack_map.keys().cloned());

    let mut packs = Vec::new();
    if let Some(account_pack) = fetch_account_pack(&client).await? {
        packs.push(account_pack);
    }

    for candidate_room_id in room_ids_to_fetch {
        let Ok(parsed_room_id) = parse_room_id(candidate_room_id.as_str()) else {
            tracing::warn!(
                handle_id,
                room_id = candidate_room_id.as_str(),
                "Skipping invalid room id while loading matrix-sdk image packs"
            );
            continue;
        };

        let Some(room) = client.get_room(&parsed_room_id) else {
            tracing::debug!(
                handle_id,
                room_id = candidate_room_id.as_str(),
                "Skipping unknown room while loading matrix-sdk image packs"
            );
            continue;
        };

        let enabled_state_keys = enabled_room_pack_map
            .get(candidate_room_id.as_str())
            .cloned()
            .unwrap_or_default();
        let from_space = parent_space_room_id_set.contains(candidate_room_id.as_str());
        packs.extend(fetch_room_packs(&room, from_space, &enabled_state_keys).await?);
    }

    packs.sort_by(|left, right| {
        let left_kind = if left.source_room_id.is_empty() {
            0
        } else if left.source_room_id == current_room.room_id().to_string() {
            1
        } else if left.from_space {
            2
        } else {
            3
        };
        let right_kind = if right.source_room_id.is_empty() {
            0
        } else if right.source_room_id == current_room.room_id().to_string() {
            1
        } else if right.from_space {
            2
        } else {
            3
        };

        left_kind
            .cmp(&right_kind)
            .then_with(|| {
                left.display_name
                    .to_lowercase()
                    .cmp(&right.display_name.to_lowercase())
            })
            .then_with(|| left.source_room_id.cmp(&right.source_room_id))
            .then_with(|| left.state_key.cmp(&right.state_key))
    });

    Ok(packs)
}

pub async fn save_image_pack(
    handle_id: u64,
    room_id: &str,
    state_key: &str,
    previous_state_key: &str,
    has_previous_state_key: bool,
    pack: MatrixImagePack,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;

    if room_id.trim().is_empty() {
        tracing::info!(handle_id, "Saving matrix-sdk account image pack");
        client
            .account()
            .set_account_data(account_pack_content_from_matrix_pack(&pack)?)
            .await
            .map(|_| ())
            .map_err(|e| format!("failed to save matrix-sdk account image pack: {e}"))?;
        return Ok(());
    }

    let room = joined_room_for_handle(handle_id, room_id)?;
    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        state_key,
        has_previous_state_key,
        previous_state_key,
        "Saving matrix-sdk room image pack"
    );

    room.send_state_event_for_key(state_key, room_pack_content_from_matrix_pack(&pack)?)
        .await
        .map_err(|e| {
            format!(
                "failed to save matrix-sdk room image pack '{}' for '{}': {e}",
                state_key,
                room.room_id().as_str()
            )
        })?;

    if has_previous_state_key && previous_state_key != state_key {
        room.send_state_event_for_key(previous_state_key, RoomImagePackEventContent::new(BTreeMap::new()))
            .await
            .map_err(|e| {
                format!(
                    "failed to clear previous matrix-sdk room image pack '{}' for '{}': {e}",
                    previous_state_key,
                    room.room_id().as_str()
                )
            })?;
    }

    update_enabled_room_packs(
        &client,
        room.room_id().as_str(),
        if has_previous_state_key && previous_state_key != state_key {
            Some(previous_state_key)
        } else {
            None
        },
        if pack.is_globally_enabled {
            Some(state_key)
        } else {
            None
        },
    )
    .await?;

    Ok(())
}

pub async fn remove_image_pack(handle_id: u64, room_id: &str, state_key: &str) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;

    if room_id.trim().is_empty() {
        tracing::info!(handle_id, "Removing matrix-sdk account image pack");
        client
            .account()
            .set_account_data(AccountImagePackEventContent::new(BTreeMap::new()))
            .await
            .map(|_| ())
            .map_err(|e| format!("failed to remove matrix-sdk account image pack: {e}"))?;
        return Ok(());
    }

    let room = joined_room_for_handle(handle_id, room_id)?;
    tracing::info!(
        handle_id,
        room_id = room.room_id().as_str(),
        state_key,
        "Removing matrix-sdk room image pack"
    );

    room.send_state_event_for_key(state_key, RoomImagePackEventContent::new(BTreeMap::new()))
        .await
        .map_err(|e| {
            format!(
                "failed to remove matrix-sdk room image pack '{}' for '{}': {e}",
                state_key,
                room.room_id().as_str()
            )
        })?;

    update_enabled_room_packs(&client, room.room_id().as_str(), Some(state_key), None).await?;
    Ok(())
}

pub async fn set_image_pack_globally_enabled(
    handle_id: u64,
    room_id: &str,
    state_key: &str,
    enabled: bool,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let parsed_room_id = parse_room_id(room_id)?;

    tracing::info!(
        handle_id,
        room_id = parsed_room_id.as_str(),
        state_key,
        enabled,
        "Updating matrix-sdk global room image-pack enablement"
    );

    let mut enabled_room_packs = load_enabled_room_packs(&client).await?;
    set_room_pack_enabled(&mut enabled_room_packs, &parsed_room_id, state_key, enabled);
    store_enabled_room_packs(&client, enabled_room_packs).await
}
