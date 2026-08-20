// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};

use super::blocking::ffi_block_on;

#[allow(clippy::too_many_arguments)]
pub(crate) fn matrix_send_room_message(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    body: &str,
    use_markdown_formatting: bool,
    message_kind: &str,
    mention_user_ids: &str,
    mentions_room: bool,
    use_send_queue: bool,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_send_room_message",
        matrix_backend::runtime::send_room_message(
            handle_id,
            room_id,
            body,
            use_markdown_formatting,
            message_kind,
            mention_user_ids,
            mentions_room,
            use_send_queue,
        ),
    )
}

pub(crate) fn matrix_send_room_message_like_event_json(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_type: &str,
    content_json: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_send_room_message_like_event_json",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            event_type,
            content_json,
        ),
    )
}

pub(crate) fn matrix_send_call_invite(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    version: &str,
    lifetime: u32,
    invitee: &str,
    offer_sdp: &str,
    offer_type: &str,
) -> Result<(), String> {
    let content_json = matrix_backend::runtime::serialize_call_invite(
        call_id, party_id, version, lifetime, invitee, offer_sdp, offer_type,
    )?;
    ffi_block_on(
        context,
        "matrix_send_call_invite",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.invite",
            &content_json,
        ),
    )
}

pub(crate) fn matrix_send_call_candidates(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    version: &str,
    candidates: Vec<ffi::MatrixCallIceCandidate>,
) -> Result<(), String> {
    let content_json =
        matrix_backend::runtime::serialize_call_candidates(call_id, party_id, version, &candidates)?;
    ffi_block_on(
        context,
        "matrix_send_call_candidates",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.candidates",
            &content_json,
        ),
    )
}

pub(crate) fn matrix_send_call_answer(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    version: &str,
    answer_sdp: &str,
    answer_type: &str,
) -> Result<(), String> {
    let content_json = matrix_backend::runtime::serialize_call_answer(
        call_id, party_id, version, answer_sdp, answer_type,
    )?;
    ffi_block_on(
        context,
        "matrix_send_call_answer",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.answer",
            &content_json,
        ),
    )
}

pub(crate) fn matrix_send_call_hangup(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    version: &str,
    reason: &str,
) -> Result<(), String> {
    let content_json =
        matrix_backend::runtime::serialize_call_hangup(call_id, party_id, version, reason)?;
    ffi_block_on(
        context,
        "matrix_send_call_hangup",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.hangup",
            &content_json,
        ),
    )
}

pub(crate) fn matrix_send_call_select_answer(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    version: &str,
    selected_party_id: &str,
) -> Result<(), String> {
    let content_json = matrix_backend::runtime::serialize_call_select_answer(
        call_id, party_id, version, selected_party_id,
    )?;
    ffi_block_on(
        context,
        "matrix_send_call_select_answer",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.select_answer",
            &content_json,
        ),
    )
}

pub(crate) fn matrix_send_call_reject(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    version: &str,
) -> Result<(), String> {
    let content_json =
        matrix_backend::runtime::serialize_call_reject(call_id, party_id, version)?;
    ffi_block_on(
        context,
        "matrix_send_call_reject",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.reject",
            &content_json,
        ),
    )
}

pub(crate) fn matrix_send_call_negotiate(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    call_id: &str,
    party_id: &str,
    lifetime: u32,
    description_sdp: &str,
    description_type: &str,
) -> Result<(), String> {
    let content_json = matrix_backend::runtime::serialize_call_negotiate(
        call_id, party_id, lifetime, description_sdp, description_type,
    )?;
    ffi_block_on(
        context,
        "matrix_send_call_negotiate",
        matrix_backend::runtime::send_room_message_like_event_json(
            handle_id,
            room_id,
            "m.call.negotiate",
            &content_json,
        ),
    )
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn matrix_send_room_reply_message(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    replied_to_event_id: &str,
    body: &str,
    use_markdown_formatting: bool,
    message_kind: &str,
    thread_id: &str,
    mention_user_ids: &str,
    mentions_room: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_send_room_reply_message",
        matrix_backend::runtime::send_room_reply_message(
            handle_id,
            room_id,
            replied_to_event_id,
            body,
            use_markdown_formatting,
            message_kind,
            thread_id,
            mention_user_ids,
            mentions_room,
        ),
    )
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn matrix_send_room_edit_message(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    target_event_id: &str,
    body: &str,
    use_markdown_formatting: bool,
    message_kind: &str,
    mention_user_ids: &str,
    mentions_room: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_send_room_edit_message",
        matrix_backend::runtime::send_room_edit_message(
            handle_id,
            room_id,
            target_event_id,
            body,
            use_markdown_formatting,
            message_kind,
            mention_user_ids,
            mentions_room,
        ),
    )
}

pub(crate) fn matrix_toggle_room_reaction(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reaction_key: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_toggle_room_reaction",
        matrix_backend::runtime::toggle_room_reaction(
            handle_id,
            room_id,
            event_id,
            reaction_key,
        ),
    )
}

pub(crate) fn matrix_redact_room_event(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reason: &str,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_redact_room_event",
        matrix_backend::runtime::redact_room_event(handle_id, room_id, event_id, reason),
    )
}

pub(crate) fn matrix_cancel_room_local_echo(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    transaction_id: &str,
) -> Result<bool, String> {
    ffi_block_on(
        context,
        "matrix_cancel_room_local_echo",
        matrix_backend::runtime::cancel_local_echo(handle_id, room_id, transaction_id),
    )
}

pub(crate) fn matrix_retry_room_local_echo(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    transaction_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_retry_room_local_echo",
        matrix_backend::runtime::retry_local_echo(handle_id, room_id, transaction_id),
    )
}

pub(crate) fn matrix_mark_room_event_as_read(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    public_receipt: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_mark_room_event_as_read",
        matrix_backend::runtime::mark_room_event_as_read(
            handle_id,
            room_id,
            event_id,
            public_receipt,
        ),
    )
}

pub(crate) fn matrix_mark_room_as_read(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    public_receipt: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_mark_room_as_read",
        matrix_backend::runtime::mark_room_as_read(handle_id, room_id, public_receipt),
    )
}

pub(crate) fn matrix_mark_room_unread(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    unread: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_mark_room_unread",
        matrix_backend::runtime::mark_room_unread(handle_id, room_id, unread),
    )
}

pub(crate) fn matrix_report_room_event(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reason: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_report_room_event",
        matrix_backend::runtime::report_room_event(handle_id, room_id, event_id, reason),
    )
}

pub(crate) fn matrix_fetch_room_thread_roots(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    include: &str,
    from: &str,
    limit: u32,
) -> Result<ffi::MatrixThreadRootsResult, String> {
    ffi_block_on(
        context,
        "matrix_fetch_room_thread_roots",
        matrix_backend::runtime::fetch_room_thread_roots(handle_id, room_id, include, from, limit),
    )
}

pub(crate) fn matrix_fetch_room_frequent_reactions(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    lookback_days: i32,
    max_results: u32,
    max_scanned_events: u64,
) -> Result<Vec<String>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_room_frequent_reactions",
        matrix_backend::runtime::fetch_room_frequent_reactions(
            handle_id,
            room_id,
            lookback_days,
            max_results,
            max_scanned_events,
        ),
    )
}

pub(crate) fn matrix_pin_room_event(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_pin_room_event",
        matrix_backend::runtime::pin_room_event(handle_id, room_id, event_id),
    )
}

pub(crate) fn matrix_unpin_room_event(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_unpin_room_event",
        matrix_backend::runtime::unpin_room_event(handle_id, room_id, event_id),
    )
}

pub(crate) fn matrix_fetch_active_room_raw_event_dialog_data(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<ffi::MatrixRawEventDialogData, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_active_room_raw_event_dialog_data",
        matrix_backend::runtime::fetch_active_room_raw_event_dialog_data(
            handle_id, room_id, event_id,
        ),
    )?;

    Ok(ffi::MatrixRawEventDialogData {
        cleartext_json: result.cleartext_json,
        cleartext_error: result.cleartext_error,
        wire_json: result.wire_json,
        wire_error: result.wire_error,
        wire_matches_cleartext: result.wire_matches_cleartext,
        body: result.body,
        formatted_body: result.formatted_body,
    })
}

pub(crate) fn matrix_fetch_active_room_event_content_for_forwarding(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<ffi::MatrixEventContentForForwarding, String> {
    let (event_type, content_json) = ffi_block_on(
        context,
        "matrix_fetch_active_room_event_content_for_forwarding",
        matrix_backend::runtime::fetch_active_room_event_content_for_forwarding(
            handle_id, room_id, event_id,
        ),
    )?;

    Ok(ffi::MatrixEventContentForForwarding {
        event_type,
        content_json,
    })
}

pub(crate) fn matrix_fetch_room_read_receipts(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<Vec<ffi::MatrixReadReceiptEntry>, String> {
    Ok(ffi_block_on(
        context,
        "matrix_fetch_room_read_receipts",
        matrix_backend::runtime::fetch_room_read_receipts(handle_id, room_id, event_id),
    )?
    .into_iter()
    .map(|entry| ffi::MatrixReadReceiptEntry {
        user_id: entry.user_id,
        display_name: entry.display_name,
        avatar_url: entry.avatar_url,
        timestamp: entry.timestamp,
    })
    .collect())
}

pub(crate) fn matrix_fetch_room_redaction_permissions(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<ffi::MatrixRoomRedactionPermissions, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_redaction_permissions",
        matrix_backend::runtime::fetch_room_redaction_permissions(handle_id, room_id),
    )?;

    Ok(ffi::MatrixRoomRedactionPermissions {
        can_redact_own: result.can_redact_own,
        can_redact_other: result.can_redact_other,
    })
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn matrix_send_room_attachment(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    file_path: &str,
    filename: &str,
    caption: &str,
    use_markdown_formatting: bool,
    reply_event_id: &str,
    thread_id: &str,
    mime_type: &str,
    duration_ms: u64,
    is_voice: bool,
    waveform: &[f32],
    strip_image_metadata: bool,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_send_room_attachment",
        matrix_backend::runtime::send_room_attachment(
            handle_id,
            room_id,
            file_path,
            filename,
            caption,
            use_markdown_formatting,
            reply_event_id,
            thread_id,
            mime_type,
            duration_ms,
            is_voice,
            waveform,
            strip_image_metadata,
        ),
    )
}

pub(crate) fn matrix_upload_media(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    file_path: &str,
    mime_type: &str,
    strip_image_metadata: bool,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_upload_media",
        matrix_backend::runtime::upload_media(handle_id, file_path, mime_type, strip_image_metadata),
    )
}

pub(crate) fn matrix_send_room_image(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    mxc_uri: &str,
    body: &str,
    filename: &str,
    info_json: &str,
    use_send_queue: bool,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_send_room_image",
        matrix_backend::runtime::send_room_image(
            handle_id,
            room_id,
            mxc_uri,
            body,
            filename,
            info_json,
            use_send_queue,
        ),
    )
}
