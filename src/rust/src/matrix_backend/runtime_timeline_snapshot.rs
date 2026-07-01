// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Timeline snapshot building: converts matrix-sdk-ui TimelineItem vectors
// into the flat MatrixTimelineItem structs consumed by the C++ side.

use super::*;
use super::event_summary::summarize_timeline_content;

use matrix_sdk_ui::timeline::{TimelineEventShieldState, TimelineEventShieldStateCode};

use std::collections::{HashMap, HashSet};

/// Map matrix-sdk-ui's `get_shield(false)` to the `(color, code)` snake_case
/// tags we ship to the UI. `""` for color means "no shield — render as
/// verified/clean"; an empty code means no shield was present at all.
fn shield_tags(shield: TimelineEventShieldState) -> (String, String) {
    match shield {
        TimelineEventShieldState::None => (String::new(), String::new()),
        TimelineEventShieldState::Red { code, .. } => {
            ("red".to_owned(), shield_code_tag(code).to_owned())
        }
        TimelineEventShieldState::Grey { code, .. } => {
            ("grey".to_owned(), shield_code_tag(code).to_owned())
        }
    }
}

fn shield_code_tag(code: TimelineEventShieldStateCode) -> &'static str {
    match code {
        TimelineEventShieldStateCode::AuthenticityNotGuaranteed => "authenticity_not_guaranteed",
        TimelineEventShieldStateCode::UnknownDevice => "unknown_device",
        TimelineEventShieldStateCode::UnsignedDevice => "unsigned_device",
        TimelineEventShieldStateCode::UnverifiedIdentity => "unverified_identity",
        TimelineEventShieldStateCode::SentInClear => "sent_in_clear",
        TimelineEventShieldStateCode::VerificationViolation => "verification_violation",
        TimelineEventShieldStateCode::MismatchedSender => "mismatched_sender",
    }
}

/// Compute which own event IDs should show "read" status based on other
/// members' read receipt positions in the timeline.  Read receipts are
/// point-in-time markers (one per user, attached to their latest read
/// event), and the SDK keeps them on the timeline items via
/// `track_read_marker_and_receipts` — so we walk the items, find the newest
/// one carrying a receipt from somebody other than us, and treat every own
/// event at or before that watermark as read.
pub fn compute_read_own_event_ids(
    items: &Vector<Arc<TimelineItem>>,
    own_user_id: Option<&matrix_sdk::ruma::UserId>,
) -> HashSet<String> {
    let own_user_id = own_user_id.map(|id| id.as_str());

    // Items are in SDK order (oldest first).  Find the highest index
    // (newest) whose item has a read receipt from another member.
    let mut watermark_idx: Option<usize> = None;
    for (idx, item) in items.iter().enumerate() {
        if let Some(event) = item.as_event() {
            let read_by_other = event
                .read_receipts()
                .keys()
                .any(|user_id| Some(user_id.as_str()) != own_user_id);
            if read_by_other {
                watermark_idx = Some(idx);
            }
        }
    }

    let mut read_ids = HashSet::new();
    if let Some(wm) = watermark_idx {
        for item in items.iter().take(wm + 1) {
            if let Some(event) = item.as_event() {
                if event.is_own() {
                    if let Some(eid) = event.event_id() {
                        read_ids.insert(eid.to_string());
                    }
                }
            }
        }
    }

    read_ids
}

pub fn build_room_timeline_snapshot(
    values: &Vector<Arc<TimelineItem>>,
    own_user_id: Option<&matrix_sdk::ruma::UserId>,
    read_own_event_ids: &HashSet<String>,
    thread_reply_counts: Option<&HashMap<String, u32>>,
) -> (Vec<MatrixTimelineItem>, HashMap<String, MatrixTimelineMediaRequest>) {
    let mut items = Vec::new();
    let mut media_lookup = HashMap::new();

    for item in values.iter() {
        if let Some((mut summary, media_request, reply_media_request)) =
            timeline_item_to_summary(item.as_ref(), own_user_id, read_own_event_ids)
        {
            // Apply cached thread reply counts from list_threads() when the
            // SDK's own thread_summary is unpopulated (the common case — see
            // docs/architecture/thread-reply-counts.md).
            if let Some(counts) = thread_reply_counts {
                if !summary.event_id.is_empty() {
                    if let Some(&count) = counts.get(&summary.event_id) {
                        summary.is_thread_root = true;
                        if summary.thread_reply_count == 0 {
                            summary.thread_reply_count = count;
                        }
                    }
                }
            }

            if let Some(media_request) = media_request {
                media_lookup.insert(summary.item_id.clone(), media_request.clone());
                if !summary.event_id.is_empty() {
                    media_lookup.insert(summary.event_id.clone(), media_request);
                }
            }
            if let Some(reply_media_request) = reply_media_request {
                if !summary.reply_event_id.is_empty() {
                    media_lookup
                        .entry(summary.reply_event_id.clone())
                        .or_insert(reply_media_request);
                }
            }
            items.push(summary);
        }
    }

    // Reverse so index 0 = newest, matching the BottomToTop ListView
    // layout in QML where index 0 sits at the visual bottom.
    items.reverse();

    (items, media_lookup)
}

fn timeline_item_to_summary(
    item: &TimelineItem,
    own_user_id: Option<&matrix_sdk::ruma::UserId>,
    read_own_event_ids: &HashSet<String>,
) -> Option<(MatrixTimelineItem, Option<MatrixTimelineMediaRequest>, Option<MatrixTimelineMediaRequest>)> {
    let item_id = item.unique_id().0.clone();

    if let Some(event) = item.as_event() {
        let sender_id = event.sender().to_string();
        let (sender_display_name, sender_avatar_url) = match event.sender_profile() {
            TimelineDetails::Ready(profile) => (
                profile
                    .display_name
                    .clone()
                    .unwrap_or_else(|| sender_id.clone()),
                profile
                    .avatar_url
                    .as_ref()
                    .map(|url| normalize_mxc_uri(url.to_string()))
                    .unwrap_or_default(),
            ),
            _ => (sender_id.clone(), String::new()),
        };
        let summary = summarize_timeline_content(event.content(), own_user_id, &sender_display_name);
        let body = summary.body;
        let formatted_body = summary.formatted_body;
        let thread_id = summary.thread_root_id;
        let is_thread_root = summary.is_thread_root;
        let thread_reply_count = summary.thread_reply_count;
        let reply_event_id = summary.reply_event_id;
        let reply_sender_id = summary.reply_sender_id;
        let reply_sender_display_name = summary.reply_sender_display_name;
        let reply_item_kind = summary.reply_item_kind;
        let reply_matrix_event_type = summary.reply_matrix_event_type;
        let reply_body = summary.reply_body;
        let reply_formatted_body = summary.reply_formatted_body;
        let reply_media = summary.reply_media;
        let reactions = summary.reactions;
        let reactions_summary = summary.reactions_summary;
        let special_effect_names = summary.special_effect_names;
        let item_kind = summary.kind;
        let membership_change_kind = summary.membership_change_kind;
        let matrix_event_type = summary.matrix_event_type;
        let is_edited = summary.is_edited;
        let is_voice_message = summary.is_voice_message;
        let waveform = summary.waveform;
        let media = summary.media;
        let state_event_target_user = summary.state_event_target_user;
        let state_event_target_user_id = summary.state_event_target_user_id;
        let state_event_detail = summary.state_event_detail;
        let state_event_reason = summary.state_event_reason;
        let state_event_has_sender = summary.state_event_has_sender;
        let power_level_changes = summary.power_level_changes;
        let server_acl_changes = summary.server_acl_changes;
        let tombstone_replacement_room_id = summary.tombstone_replacement_room_id;
        let utd_cause = summary.utd_cause;
        let media_request = media.as_ref().and_then(|media| {
            media.source.clone().map(|source| MatrixTimelineMediaRequest {
                source,
                thumbnail_source: media.thumbnail_source.clone(),
                size_bytes: media.media_size_bytes,
            })
        });
        let reply_media_request = reply_media.as_ref().and_then(|media| {
            media.source.clone().map(|source| MatrixTimelineMediaRequest {
                source,
                thumbnail_source: media.thumbnail_source.clone(),
                size_bytes: media.media_size_bytes,
            })
        });

        let (delivery_state, send_error, is_recoverable) =
            matrix_timeline_delivery_state(event, own_user_id, read_own_event_ids);
        // `encryption_info()` is `Some` only for events decrypted from a Megolm
        // session. UTDs are encrypted on the wire but haven't been decrypted,
        // so treat them as encrypted too — otherwise the UI would render the
        // "sent in clear" warning on top of the UTD placeholder.
        let is_encrypted_event =
            event.encryption_info().is_some() || item_kind == "unable_to_decrypt";
        let (shield_color, shield_code) = shield_tags(event.get_shield(false));
        return Some((
            MatrixTimelineItem {
                item_id,
                event_id: event.event_id().map(ToString::to_string).unwrap_or_default(),
                transaction_id: event.transaction_id().map(ToString::to_string).unwrap_or_default(),
                delivery_state,
                send_error,
                is_recoverable,
                thread_id,
                is_thread_root,
                thread_reply_count,
                sender_id,
                sender_display_name,
                sender_avatar_url,
                body,
                formatted_body,
                reply_event_id,
                reply_sender_id,
                reply_sender_display_name,
                reply_item_kind,
                reply_matrix_event_type,
                reply_body,
                reply_formatted_body,
                reply_media_url: reply_media
                    .as_ref()
                    .map(|media| media.media_url.clone())
                    .unwrap_or_default(),
                reply_thumbnail_url: reply_media
                    .as_ref()
                    .map(|media| media.thumbnail_url.clone())
                    .unwrap_or_default(),
                reply_file_name: reply_media
                    .as_ref()
                    .map(|media| media.file_name.clone())
                    .unwrap_or_default(),
                reply_mime_type: reply_media
                    .as_ref()
                    .map(|media| media.mime_type.clone())
                    .unwrap_or_default(),
                reply_media_width: reply_media
                    .as_ref()
                    .map(|media| media.media_width)
                    .unwrap_or(0),
                reply_media_height: reply_media
                    .as_ref()
                    .map(|media| media.media_height)
                    .unwrap_or(0),
                reply_media_duration_ms: reply_media
                    .as_ref()
                    .map(|media| media.media_duration_ms)
                    .unwrap_or(0),
                reply_media_size_bytes: reply_media
                    .as_ref()
                    .map(|media| media.media_size_bytes)
                    .unwrap_or(0),
                reply_blurhash: reply_media
                    .as_ref()
                    .map(|media| media.blurhash.clone())
                    .unwrap_or_default(),
                reactions,
                reactions_summary,
                special_effect_names,
                item_kind,
                membership_change_kind,
                matrix_event_type,
                is_edited,
                media_url: media
                    .as_ref()
                    .map(|media| media.media_url.clone())
                    .unwrap_or_default(),
                thumbnail_url: media
                    .as_ref()
                    .map(|media| media.thumbnail_url.clone())
                    .unwrap_or_default(),
                file_name: media
                    .as_ref()
                    .map(|media| media.file_name.clone())
                    .unwrap_or_default(),
                mime_type: media
                    .as_ref()
                    .map(|media| media.mime_type.clone())
                    .unwrap_or_default(),
                media_width: media.as_ref().map(|media| media.media_width).unwrap_or(0),
                media_height: media
                    .as_ref()
                    .map(|media| media.media_height)
                    .unwrap_or(0),
                media_duration_ms: media
                    .as_ref()
                    .map(|media| media.media_duration_ms)
                    .unwrap_or(0),
                media_size_bytes: media
                    .as_ref()
                    .map(|media| media.media_size_bytes)
                    .unwrap_or(0),
                blurhash: media
                    .as_ref()
                    .map(|media| media.blurhash.clone())
                    .unwrap_or_default(),
                media_is_encrypted: media
                    .as_ref()
                    .map(|media| media.media_is_encrypted)
                    .unwrap_or(false),
                thumbnail_is_encrypted: media
                    .as_ref()
                    .map(|media| media.thumbnail_is_encrypted)
                    .unwrap_or(false),
                is_voice_message,
                waveform,
                timestamp: u64::from(event.timestamp().get()),
                is_own: event.is_own(),
                state_event_target_user,
                state_event_target_user_id,
                state_event_detail,
                state_event_reason,
                state_event_has_sender,
                utd_cause,
                is_encrypted_event,
                shield_color,
                shield_code,
                power_level_changes,
                server_acl_changes,
                tombstone_replacement_room_id,
            },
            media_request,
            reply_media_request,
        ));
    }

    match item.as_virtual() {
        Some(VirtualTimelineItem::DateDivider(timestamp)) => Some((
            MatrixTimelineItem {
                item_id,
                event_id: String::new(),
                transaction_id: String::new(),
                delivery_state: String::new(),
                send_error: String::new(),
                is_recoverable: false,
                thread_id: String::new(),
                is_thread_root: false,
                thread_reply_count: 0,
                sender_id: String::new(),
                sender_display_name: String::new(),
                sender_avatar_url: String::new(),
                body: String::new(),
                formatted_body: String::new(),
                reply_event_id: String::new(),
                reply_sender_id: String::new(),
                reply_sender_display_name: String::new(),
                reply_item_kind: String::new(),
                reply_matrix_event_type: String::new(),
                reply_body: String::new(),
                reply_formatted_body: String::new(),
                reply_media_url: String::new(),
                reply_thumbnail_url: String::new(),
                reply_file_name: String::new(),
                reply_mime_type: String::new(),
                reply_media_width: 0,
                reply_media_height: 0,
                reply_media_duration_ms: 0,
                reply_media_size_bytes: 0,
                reply_blurhash: String::new(),
                reactions: Vec::new(),
                reactions_summary: String::new(),
                special_effect_names: Vec::new(),
                item_kind: "date_divider".to_owned(),
                membership_change_kind: String::new(),
                matrix_event_type: String::new(),
                is_edited: false,
                media_url: String::new(),
                thumbnail_url: String::new(),
                file_name: String::new(),
                mime_type: String::new(),
                media_width: 0,
                media_height: 0,
                media_duration_ms: 0,
                media_size_bytes: 0,
                blurhash: String::new(),
                media_is_encrypted: false,
                thumbnail_is_encrypted: false,
                is_voice_message: false,
                waveform: Vec::new(),
                timestamp: u64::from(timestamp.get()),
                is_own: false,
                state_event_target_user: String::new(),
                state_event_target_user_id: String::new(),
                state_event_detail: String::new(),
                state_event_reason: String::new(),
                state_event_has_sender: false,
                utd_cause: String::new(),
                is_encrypted_event: false,
                shield_color: String::new(),
                shield_code: String::new(),
                power_level_changes: Vec::new(),
                server_acl_changes: None,
                tombstone_replacement_room_id: String::new(),
            },
            None,
            None,
        )),
        Some(VirtualTimelineItem::ReadMarker) | Some(VirtualTimelineItem::TimelineStart) | None => {
            None
        }
    }
}

/// Collect event IDs of timeline items whose reply details are unavailable
/// so the caller can trigger `Timeline::fetch_details_for_event` for each.
pub fn collect_unavailable_reply_event_ids(
    values: &Vector<Arc<TimelineItem>>,
) -> Vec<OwnedEventId> {
    let mut result = Vec::new();
    for item in values.iter() {
        let Some(event) = item.as_event() else { continue };
        let Some(event_id) = event.event_id() else { continue };
        let Some(in_reply_to) = event.content().in_reply_to() else { continue };
        if matches!(in_reply_to.event, TimelineDetails::Unavailable) {
            result.push(event_id.to_owned());
        }
    }
    result
}

/// Returns the `(delivery_state, send_error, is_recoverable)` triple for a
/// timeline item. `send_error` is populated only for `SendingFailed` items;
/// `is_recoverable` is true only when the SDK considers the failure
/// transient (e.g. network hiccup) and would benefit from an unwedge retry.
fn matrix_timeline_delivery_state(
    event: &matrix_sdk_ui::timeline::EventTimelineItem,
    own_user_id: Option<&matrix_sdk::ruma::UserId>,
    read_own_event_ids: &HashSet<String>,
) -> (String, String, bool) {
    use matrix_sdk_ui::timeline::EventSendState;

    let _ = own_user_id; // reserved for future use

    // The SDK distinguishes `NotSentYet` (queued, no HTTP attempt yet) from
    // `Sent` (HTTP succeeded, awaiting sync echo), but the gap is fleeting
    // and visually identical (clock + "Sending") so we collapse both into
    // "pending". "sent" then means the event is in the canonical timeline
    // post-sync — the honest user-facing notion of "it left your client",
    // not a claim that any remote homeserver or device has seen it.
    match event.send_state() {
        Some(EventSendState::NotSentYet { .. } | EventSendState::Sent { .. }) => {
            ("pending".to_owned(), String::new(), false)
        }
        Some(EventSendState::SendingFailed { error, is_recoverable }) => (
            "failed".to_owned(),
            error.to_string(),
            *is_recoverable,
        ),
        None if event.is_own() => {
            let is_read = event
                .event_id()
                .map(|eid| read_own_event_ids.contains(eid.as_str()))
                .unwrap_or(false);
            let label = if is_read {
                "read".to_owned()
            } else {
                "sent".to_owned()
            };
            (label, String::new(), false)
        }
        None => (String::new(), String::new(), false),
    }
}

pub fn build_timeline_media_request_parameters(
    media_request: &MatrixTimelineMediaRequest,
    width: i32,
    height: i32,
    crop: bool,
) -> Result<MediaRequestParameters, String> {
    // Only prefer the dedicated thumbnail_source when an explicit size is
    // requested (width/height > 0), indicating the caller wants a thumbnail.
    // When width=0 and height=0 the caller wants the full-size original
    // (e.g. media overlay, animated image loader, video player).
    if width > 0 && height > 0 {
        if let Some(thumbnail_source) = media_request.thumbnail_source.clone() {
            return Ok(MediaRequestParameters {
                source: thumbnail_source,
                format: MediaFormat::File,
            });
        }

        if matches!(&media_request.source, MediaSource::Plain(_)) {
            let width =
                UInt::try_from(width).map_err(|_| format!("invalid thumbnail width: {width}"))?;
            let height =
                UInt::try_from(height).map_err(|_| format!("invalid thumbnail height: {height}"))?;
            let method = if crop { Method::Crop } else { Method::Scale };

            return Ok(MediaRequestParameters {
                source: media_request.source.clone(),
                format: MediaFormat::Thumbnail(MediaThumbnailSettings::with_method(
                    method, width, height,
                )),
            });
        }
    }

    Ok(MediaRequestParameters {
        source: media_request.source.clone(),
        format: MediaFormat::File,
    })
}
