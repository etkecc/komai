// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

#include "rust/cxx.h"

namespace komai::rust {
struct MatrixPersistedSessionSecrets;
struct MatrixRoomSummary;
struct MatrixRoomPreviewUpdate;
struct MatrixNotificationItem;
struct SettingsOptionalString;
struct MatrixCallInviteEvent;
struct MatrixCallCandidatesEvent;
struct MatrixCallAnswerEvent;
struct MatrixCallHangUpEvent;
struct MatrixCallSelectAnswerEvent;
struct MatrixCallRejectEvent;
struct MatrixCallNegotiateEvent;
struct MatrixRtcNotificationEvent;
struct MatrixRtcDeclineEvent;
}

namespace komai::rust_bridge {

::rust::String
matrix_profile_data_root(::rust::Str profile_id);

::rust::String
matrix_profile_cache_root(::rust::Str profile_id);

::rust::String
settings_profile_directory(::rust::Str profile_id);

::rust::String
settings_secure_store_key(::rust::Str profile_id, ::rust::Str key_name);

::komai::rust::SettingsOptionalString
settings_read_secure_value(::rust::Str key);

void
settings_write_secure_value(::rust::Str key, ::rust::Str value);

bool
settings_write_secure_value_blocking(::rust::Str key, ::rust::Str value);

void
settings_delete_secure_value(::rust::Str key);

bool
settings_delete_secure_value_blocking(::rust::Str key);

::rust::String
settings_read_text_file(::rust::Str path, ::rust::Str label);

bool
settings_path_exists(::rust::Str path);

bool
settings_remove_path(::rust::Str path);

bool
settings_write_text_file(::rust::Str path, ::rust::Str content, bool owner_read_write_only);

bool
settings_delete_all_profile_secrets_from_store(::rust::Str profile_id,
                                               bool uses_file_secrets_provider);

::komai::rust::MatrixPersistedSessionSecrets
matrix_load_session_secrets(::rust::Str profile_id);

bool
matrix_save_session_secrets(::rust::Str profile_id,
                            ::rust::Str store_passphrase,
                            ::rust::Str homeserver_url,
                            ::rust::Str serialized_session);

void
matrix_clear_session_secrets(::rust::Str profile_id);

void
matrix_notify_room_list_snapshot_updated(std::uint64_t handle_id,
                                         ::rust::Vec<::komai::rust::MatrixRoomSummary> room_list);

void
matrix_notify_room_previews_backfilled(std::uint64_t handle_id,
                                       ::rust::Vec<::komai::rust::MatrixRoomPreviewUpdate> updates);

void
matrix_notify_ignored_user_list_updated(std::uint64_t handle_id,
                                        ::rust::Vec<::rust::String> user_ids);

void
matrix_notify_initial_sync_ready(std::uint64_t handle_id);

void
matrix_notify_sync_connection_state_changed(std::uint64_t handle_id, bool is_connected);

void
matrix_notify_room_timeline_snapshot_updated(std::uint64_t handle_id, ::rust::Str room_id);

void
matrix_notify_room_timeline_pagination_state(std::uint64_t handle_id,
                                             ::rust::Str room_id,
                                             bool in_progress);

void
matrix_notify_room_pinned_events_changed(std::uint64_t handle_id,
                                         ::rust::Str room_id,
                                         ::rust::Vec<::rust::String> event_ids);

void
matrix_notify_thread_timeline_snapshot_updated(std::uint64_t handle_id,
                                               ::rust::Str room_id,
                                               ::rust::Str thread_root_id);

void
matrix_notify_notification_received(std::uint64_t handle_id,
                                    ::rust::Str room_id,
                                    ::rust::Str event_id);

void
matrix_notify_notification_item_received(std::uint64_t handle_id,
                                         ::komai::rust::MatrixNotificationItem item);

void
matrix_notify_call_invite_received(std::uint64_t handle_id,
                                   ::komai::rust::MatrixCallInviteEvent event);

void
matrix_notify_call_candidates_received(std::uint64_t handle_id,
                                       ::komai::rust::MatrixCallCandidatesEvent event);

void
matrix_notify_call_answer_received(std::uint64_t handle_id,
                                   ::komai::rust::MatrixCallAnswerEvent event);

void
matrix_notify_call_hangup_received(std::uint64_t handle_id,
                                   ::komai::rust::MatrixCallHangUpEvent event);

void
matrix_notify_call_select_answer_received(std::uint64_t handle_id,
                                          ::komai::rust::MatrixCallSelectAnswerEvent event);

void
matrix_notify_call_reject_received(std::uint64_t handle_id,
                                   ::komai::rust::MatrixCallRejectEvent event);

void
matrix_notify_call_negotiate_received(std::uint64_t handle_id,
                                      ::komai::rust::MatrixCallNegotiateEvent event);

void
matrix_notify_sync_stopped(std::uint64_t handle_id, ::rust::Str reason, bool is_auth_error);

void
matrix_notify_typing_users_updated(std::uint64_t handle_id,
                                   ::rust::Str room_id,
                                   ::rust::Vec<::rust::String> display_names);

// Element Call widget driver -> webview. Routed by session_id to the matching
// ElementCallWidgetSession on the GUI thread. Defined unconditionally (the Rust
// widget driver is always compiled); they are no-ops in -DELEMENT_CALL=OFF
// builds where ElementCallWidgetSession does not exist.
void
matrix_notify_element_call_widget_url_ready(std::uint64_t session_id, ::rust::Str url);

void
matrix_notify_element_call_widget_message(std::uint64_t session_id, ::rust::Str message);

void
matrix_notify_element_call_widget_stopped(std::uint64_t session_id, ::rust::Str reason);

// MatrixRTC ring/notify (MSC4075) + decline (MSC4310) -> ElementCallController on
// the GUI thread. Defined unconditionally (the Rust RTC handlers are always
// compiled); they early-out in -DELEMENT_CALL=OFF builds where Element Call is
// unsupported.
void
matrix_notify_rtc_notification(std::uint64_t handle_id,
                               ::komai::rust::MatrixRtcNotificationEvent event);

void
matrix_notify_rtc_decline(std::uint64_t handle_id, ::komai::rust::MatrixRtcDeclineEvent event);

} // namespace komai::rust_bridge
