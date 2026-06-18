// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! MatrixRTC notification + decline plumbing (MSC4075 / MSC4310), the receive
//! side of Element Call ringing.
//!
//! Mirrors the legacy `runtime_calls` event-subscription pattern: a single raw
//! `AnySyncMessageLikeEvent` handler watches every synced room for
//! `m.rtc.notification` (an incoming call) and `m.rtc.decline` (a call rejected
//! on one of our devices), parses the JSON by hand (the ruma RTC types sit
//! behind unstable features we don't enable), and forwards typed structs to the
//! C++ `ElementCallController`, which owns the ring decision and UI. This is
//! server-independent (it doesn't rely on the homeserver having an RTC push
//! rule configured) and compiles unconditionally (no QtWebEngine dependency);
//! on `-DELEMENT_CALL=OFF` builds the C++ side simply ignores the callbacks.

use matrix_sdk::ruma::{events::AnySyncMessageLikeEvent, serde::Raw};
use serde_json::{Value, json};

use super::*;

/// Maximum tolerated divergence between the sender's clock (`sender_ts`) and the
/// server's (`origin_server_ts`) before we distrust `sender_ts` and fall back to
/// the server timestamp when computing the ring expiry. Matches the MSC4075
/// reference value used by ruma's `RtcNotificationEventContent::expiration_ts`.
const MAX_SENDER_TS_OFFSET_MS: u64 = 20_000;

pub(crate) fn install_rtc_event_handlers(
    handle_id: u64,
    client: matrix_sdk::Client,
) -> Vec<EventHandlerDropGuard> {
    let handle = client.add_event_handler({
        move |raw: Raw<AnySyncMessageLikeEvent>, room: matrix_sdk::Room| async move {
            let Ok(value) = serde_json::from_str::<Value>(raw.json().get()) else {
                return;
            };
            let Some(event_type) = value.get("type").and_then(|v| v.as_str()) else {
                return;
            };
            match event_type.trim() {
                "m.rtc.notification" | "org.matrix.msc4075.rtc.notification" => {
                    dispatch_rtc_notification(handle_id, &room, &value);
                }
                "m.rtc.decline" | "org.matrix.msc4310.rtc.decline" => {
                    dispatch_rtc_decline(handle_id, &room, &value);
                }
                _ => {}
            }
        }
    });

    vec![client.event_handler_drop_guard(handle)]
}

fn dispatch_rtc_notification(handle_id: u64, room: &matrix_sdk::Room, value: &Value) {
    let event_id = value.get("event_id").and_then(|v| v.as_str()).unwrap_or_default();
    let sender = value.get("sender").and_then(|v| v.as_str()).unwrap_or_default();
    if event_id.is_empty() || sender.is_empty() {
        return;
    }
    let content = value.get("content");

    let notification_type = content
        .and_then(|c| c.get("notification_type"))
        .and_then(|v| v.as_str())
        .unwrap_or("notification")
        .to_owned();
    let lifetime_ms = content.and_then(|c| c.get("lifetime")).and_then(|v| v.as_u64()).unwrap_or(0);
    let sender_ts = content.and_then(|c| c.get("sender_ts")).and_then(|v| v.as_u64()).unwrap_or(0);
    let origin_server_ts =
        value.get("origin_server_ts").and_then(|v| v.as_u64()).unwrap_or(0);

    let own_user_id = room.own_user_id().as_str().to_owned();
    let is_self = sender == own_user_id;

    // MSC4075: `m.mentions` decides who the notification targets. Ring only if we
    // are addressed (listed in `user_ids`, or a room-wide mention). If the field
    // is absent we cannot tell, so we err on the side of notifying.
    let mentions_me = match content.and_then(|c| c.get("m.mentions")) {
        Some(mentions) => {
            let room_mention = mentions.get("room").and_then(|v| v.as_bool()).unwrap_or(false);
            let user_listed = mentions
                .get("user_ids")
                .and_then(|v| v.as_array())
                .map(|ids| ids.iter().any(|id| id.as_str() == Some(own_user_id.as_str())))
                .unwrap_or(false);
            room_mention || user_listed
        }
        None => true,
    };

    // Ring expiry: prefer the sender's timestamp, but fall back to the server's
    // if the two diverge too far (untrusted sender clock), per MSC4075.
    let start_ts = if sender_ts == 0 {
        origin_server_ts
    } else if origin_server_ts == 0 {
        sender_ts
    } else if sender_ts.abs_diff(origin_server_ts) > MAX_SENDER_TS_OFFSET_MS {
        origin_server_ts
    } else {
        sender_ts
    };
    let expires_at_ms = start_ts.saturating_add(lifetime_ms);

    crate::ffi::matrix_notify_rtc_notification(
        handle_id,
        crate::ffi::MatrixRtcNotificationEvent {
            room_id: room.room_id().to_string(),
            event_id: event_id.to_owned(),
            sender_id: sender.to_owned(),
            notification_type,
            is_self,
            mentions_me,
            lifetime_ms,
            expires_at_ms,
        },
    );
}

fn dispatch_rtc_decline(handle_id: u64, room: &matrix_sdk::Room, value: &Value) {
    let sender = value.get("sender").and_then(|v| v.as_str()).unwrap_or_default();
    // `m.rtc.decline` is an `m.reference` to the notification it rejects.
    let notification_event_id = value
        .get("content")
        .and_then(|c| c.get("m.relates_to"))
        .and_then(|r| r.get("event_id"))
        .and_then(|v| v.as_str())
        .unwrap_or_default();
    if sender.is_empty() || notification_event_id.is_empty() {
        return;
    }

    let is_self = sender == room.own_user_id().as_str();
    crate::ffi::matrix_notify_rtc_decline(
        handle_id,
        crate::ffi::MatrixRtcDeclineEvent {
            room_id: room.room_id().to_string(),
            notification_event_id: notification_event_id.to_owned(),
            sender_id: sender.to_owned(),
            is_self,
        },
    );
}

/// Decline an incoming MatrixRTC call notification: send an `m.rtc.decline`
/// (MSC4310) referencing the notification event. This stops the ring on our
/// other devices and, for a DM caller that `waitForCallPickup`s, makes the
/// caller leave. Fire-and-forget: validate the room synchronously, then spawn
/// the send.
pub fn decline_rtc_notification(
    handle_id: u64,
    room_id: &str,
    notification_event_id: &str,
) -> Result<(), String> {
    let room = room_for_handle(handle_id, room_id)?;
    let notification_event_id = notification_event_id.to_owned();
    crate::matrix_backend::ffi::runtime().spawn(async move {
        let content = json!({
            "m.relates_to": {
                "rel_type": "m.reference",
                "event_id": notification_event_id,
            }
        });
        match room.send_raw("org.matrix.msc4310.rtc.decline", content).await {
            Ok(_) => tracing::info!("Sent m.rtc.decline for the incoming call notification"),
            Err(error) => tracing::warn!(%error, "Failed to send m.rtc.decline"),
        }
    });
    Ok(())
}
