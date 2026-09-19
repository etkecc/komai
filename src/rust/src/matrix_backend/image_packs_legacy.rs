// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Preserve Komai's existing im.ponies image-pack wire format. Ruma 0.17
//! replaced MSC2545 with stable room packs, which omit personal packs and
//! per-image usage overrides. Switching types would hide existing user data.

use std::collections::{BTreeMap, BTreeSet};

use matrix_sdk::ruma::{
    OwnedMxcUri, OwnedRoomId,
    events::{macros::EventContent, room::ImageInfo},
};
use serde::{Deserialize, Serialize};

pub(super) use matrix_sdk::ruma::events::{
    image_pack::rooms::RoomImagePackMeta as ImagePackRoomContent,
    room::image_pack::{ImagePackMeta as PackInfo, PackUsage},
};

#[derive(Clone, Debug, Default, Deserialize, Serialize, EventContent)]
#[ruma_event(type = "im.ponies.room_emotes", kind = State, state_key_type = String)]
pub struct RoomImagePackEventContent {
    pub images: BTreeMap<String, PackImage>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pack: Option<PackInfo>,
}

impl RoomImagePackEventContent {
    pub(super) fn new(images: BTreeMap<String, PackImage>) -> Self {
        Self { images, pack: None }
    }
}

#[derive(Clone, Debug, Default, Deserialize, Serialize, EventContent)]
#[ruma_event(type = "im.ponies.user_emotes", kind = GlobalAccountData)]
pub struct AccountImagePackEventContent {
    pub images: BTreeMap<String, PackImage>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pack: Option<PackInfo>,
}

impl AccountImagePackEventContent {
    pub(super) fn new(images: BTreeMap<String, PackImage>) -> Self {
        Self { images, pack: None }
    }
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct PackImage {
    pub url: OwnedMxcUri,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub body: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub info: Option<ImageInfo>,
    #[serde(default, skip_serializing_if = "BTreeSet::is_empty")]
    pub usage: BTreeSet<PackUsage>,
}

impl PackImage {
    pub(super) fn new(url: OwnedMxcUri) -> Self {
        Self {
            url,
            body: None,
            info: None,
            usage: BTreeSet::new(),
        }
    }
}

#[derive(Clone, Debug, Default, Deserialize, Serialize, EventContent)]
#[ruma_event(type = "im.ponies.emote_rooms", kind = GlobalAccountData)]
pub struct ImagePackRoomsEventContent {
    pub rooms: BTreeMap<OwnedRoomId, BTreeMap<String, ImagePackRoomContent>>,
}

#[cfg(test)]
mod tests {
    use super::*;
    use matrix_sdk::ruma::events::{GlobalAccountDataEvent, SyncStateEvent};
    use serde_json::json;

    #[test]
    fn legacy_personal_pack_preserves_per_image_usage() {
        let value = json!({
            "type": "im.ponies.user_emotes",
            "content": {
                "images": {
                    "cat": {"url": "mxc://example.org/cat", "usage": ["sticker"]},
                    "dog": {"url": "mxc://example.org/dog"}
                },
                "pack": {"display_name": "Pets", "usage": ["emoticon"]}
            }
        });
        let event: GlobalAccountDataEvent<AccountImagePackEventContent> =
            serde_json::from_value(value.clone()).unwrap();
        assert!(
            event.content.images["cat"]
                .usage
                .contains(&PackUsage::Sticker)
        );
        assert!(event.content.images["dog"].usage.is_empty());
        assert_eq!(serde_json::to_value(event).unwrap(), value);
    }

    #[test]
    fn legacy_room_pack_and_enabled_rooms_keep_their_event_types() {
        let value = json!({
            "type": "im.ponies.room_emotes", "state_key": "pets",
            "event_id": "$pack", "sender": "@user:example.org", "origin_server_ts": 1,
            "content": {"images": {"cat": {"url": "mxc://example.org/cat"}}}
        });
        let event: SyncStateEvent<RoomImagePackEventContent> =
            serde_json::from_value(value.clone()).unwrap();
        let SyncStateEvent::Original(event) = event else {
            panic!("pack should not be redacted");
        };
        assert_eq!(event.state_key, "pets");
        assert_eq!(
            serde_json::to_value(event.content).unwrap(),
            value["content"]
        );

        let enabled = json!({
            "type": "im.ponies.emote_rooms",
            "content": {"rooms": {"!room:example.org": {"pets": {}}}}
        });
        let event: GlobalAccountDataEvent<ImagePackRoomsEventContent> =
            serde_json::from_value(enabled.clone()).unwrap();
        assert_eq!(serde_json::to_value(event).unwrap(), enabled);
        assert_eq!(
            serde_json::to_value(RoomImagePackEventContent::new(BTreeMap::new())).unwrap(),
            json!({"images": {}}),
        );
    }
}
