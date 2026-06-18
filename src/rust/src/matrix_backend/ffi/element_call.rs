// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::matrix_backend;

pub(crate) fn matrix_element_call_start_session(
    handle_id: u64,
    room_id: &str,
    base_url: &str,
    lang: &str,
    theme: &str,
) -> Result<u64, String> {
    matrix_backend::runtime::start_element_call_session(handle_id, room_id, base_url, lang, theme)
}

pub(crate) fn matrix_element_call_send_message(
    session_id: u64,
    message: &str,
) -> Result<(), String> {
    matrix_backend::runtime::send_element_call_message(session_id, message)
}

pub(crate) fn matrix_element_call_stop_session(session_id: u64) {
    matrix_backend::runtime::stop_element_call_session(session_id)
}

pub(crate) fn matrix_element_call_decline(
    handle_id: u64,
    room_id: &str,
    notification_event_id: &str,
) -> Result<(), String> {
    matrix_backend::runtime::decline_rtc_notification(handle_id, room_id, notification_event_id)
}
