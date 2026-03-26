// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use matrix_sdk::ruma::{
    ServerName,
    api::client::directory::get_public_rooms_filtered,
    directory::Filter,
    room::RoomType,
};

pub async fn fetch_public_room_directory_page(
    handle_id: u64,
    search_term: &str,
    limit: u64,
    since: &str,
    server: &str,
) -> Result<MatrixPublicRoomDirectoryPage, String> {
    let client = client_for_handle(handle_id)?;

    let search_term = search_term.trim();
    let since = since.trim();
    let server = server.trim();

    let mut filter = Filter::new();
    if !search_term.is_empty() {
        filter.generic_search_term = Some(search_term.to_owned());
    }

    let parsed_server = if server.is_empty() {
        None
    } else {
        Some(
            ServerName::parse(server)
                .map_err(|e| format!("invalid room-directory server '{server}': {e}"))?,
        )
    };

    let limit_u32 = u32::try_from(limit)
        .map_err(|_| format!("room-directory search limit {limit} is too large"))?;

    let mut request = get_public_rooms_filtered::v3::Request::new();
    request.filter = filter;
    request.server = parsed_server;
    request.limit = Some(limit_u32.into());
    if !since.is_empty() {
        request.since = Some(since.to_owned());
    }

    tracing::info!(
        handle_id,
        search_term,
        limit,
        since,
        server,
        "Fetching Matrix public room directory page via matrix-sdk backend runtime"
    );

    let response = client.public_rooms_filtered(request).await.map_err(|e| {
        format!(
            "failed to fetch Matrix public room directory page for search '{search_term}': {e}"
        )
    })?;

    let total_room_count_estimate = response
        .total_room_count_estimate
        .and_then(|count| i32::try_from(u64::from(count)).ok())
        .unwrap_or(-1);

    let rooms = response
        .chunk
        .into_iter()
        .map(|room| MatrixPublicRoomDirectoryEntry {
            room_id: room.room_id.to_string(),
            room_server_name: room
                .room_id
                .server_name()
                .map(ToString::to_string)
                .unwrap_or_default(),
            display_name: room.name.unwrap_or_default(),
            avatar_url: room
                .avatar_url
                .map(|url| normalize_mxc_uri(url.to_string()))
                .unwrap_or_default(),
            topic: room.topic.unwrap_or_default(),
            canonical_alias: room
                .canonical_alias
                .map(|alias| alias.to_string())
                .unwrap_or_default(),
            member_count: room.num_joined_members.into(),
            is_world_readable: room.world_readable,
            is_space: matches!(room.room_type, Some(RoomType::Space)),
        })
        .collect::<Vec<_>>();

    let next_batch = response.next_batch.unwrap_or_default();

    tracing::info!(
        handle_id,
        search_term,
        limit,
        since,
        server,
        result_count = rooms.len(),
        next_batch,
        total_room_count_estimate,
        "Fetched Matrix public room directory page via matrix-sdk backend runtime"
    );

    Ok(MatrixPublicRoomDirectoryPage {
        rooms,
        next_batch,
        total_room_count_estimate,
    })
}
