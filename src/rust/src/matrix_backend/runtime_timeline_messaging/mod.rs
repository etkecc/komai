// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Message sending, editing, reactions, redactions, read receipts, and reporting.

use super::*;

use matrix_sdk::{
    attachment::{
        AttachmentConfig, AttachmentInfo, BaseAudioInfo, BaseFileInfo, BaseImageInfo,
        BaseVideoInfo,
    },
    room::Receipts,
    room::edit::EditedContent,
    room::reply::{EnforceThread, Reply},
    ruma::{
        EventId, UInt,
        html::{HtmlSanitizerMode, RemoveReplyFallback},
        events::EventContentFromType,
        events::room::message::{
            AddMentions, FormattedBody, MessageType, RoomMessageEventContentWithoutRelation,
            TextMessageEventContent,
        },
    },
};
use image::GenericImageView;
use matrix_sdk_base::latest_event::LatestEventValue as BaseLatestEventValue;
use mime::Mime;
use serde_json::{Map as JsonMap, Value as JsonValue};
use std::{fs, path::Path};

mod echo;
mod mutations;
mod read;
mod send;
pub(super) use send::deliver_message_content;

pub use echo::{cancel_local_echo, retry_local_echo};
pub use mutations::{redact_room_event, report_room_event, toggle_room_reaction};
pub use read::{mark_room_as_read, mark_room_event_as_read, mark_room_unread};
pub use send::{
    send_room_attachment, send_room_edit_message, send_room_message,
    send_room_message_like_event_json, send_room_reply_message,
};

#[cfg(test)]
mod tests;
