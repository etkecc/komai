// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

#include "rust/cxx.h"

namespace komai::rust {
struct MatrixPersistedSessionSecrets;
struct MatrixRoomSummary;
struct MatrixNotificationItem;
struct MatrixCallInviteEvent;
struct MatrixCallCandidatesEvent;
struct MatrixCallAnswerEvent;
struct MatrixCallHangUpEvent;
struct MatrixCallSelectAnswerEvent;
struct MatrixCallRejectEvent;
struct MatrixCallNegotiateEvent;
}

namespace komai::rust_bridge {

::rust::String
matrix_profile_data_root(::rust::Str profile_id);

::rust::String
matrix_profile_cache_root(::rust::Str profile_id);

::rust::String
settings_profile_config_path(::rust::Str profile_id);

::rust::String
settings_profile_state_path(::rust::Str profile_id);

::rust::String
settings_profile_session_path(::rust::Str profile_id);

::rust::String
settings_read_text_file(::rust::Str path, ::rust::Str label);

bool
settings_path_exists(::rust::Str path);

bool
settings_write_text_file(::rust::Str path, ::rust::Str content, bool owner_read_write_only);

::komai::rust::MatrixPersistedSessionSecrets
matrix_load_session_secrets(::rust::Str profile_id);

void
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
matrix_notify_ignored_user_list_updated(std::uint64_t handle_id,
                                        ::rust::Vec<::rust::String> user_ids);

void
matrix_notify_initial_sync_ready(std::uint64_t handle_id);

void
matrix_notify_room_timeline_snapshot_updated(std::uint64_t handle_id, ::rust::Str room_id);

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

} // namespace komai::rust_bridge
