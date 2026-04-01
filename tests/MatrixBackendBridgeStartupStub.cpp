// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "komai-rust-cxxbridge/ffi.h"
#include "matrix/backend/MatrixBackendBridge.h"
#include "settings/SettingsStorage.h"

namespace komai::rust_bridge {

::rust::String
matrix_profile_data_root(::rust::Str)
{
    return {};
}

::rust::String
matrix_profile_cache_root(::rust::Str)
{
    return {};
}

::rust::String
settings_read_text_file(::rust::Str path, ::rust::Str label)
{
    const auto labelString = std::string(label);
    return ::rust::String(
      settings::storage::readTextFile(QString::fromStdString(std::string(path)),
                                      labelString.c_str())
        .toStdString());
}

bool
settings_write_text_file(::rust::Str path, ::rust::Str content, bool owner_read_write_only)
{
    return settings::storage::writeTextFile(QString::fromStdString(std::string(path)),
                                            QString::fromStdString(std::string(content)),
                                            owner_read_write_only);
}

::komai::rust::MatrixPersistedSessionSecrets
matrix_load_session_secrets(::rust::Str)
{
    return {};
}

void
matrix_save_session_secrets(::rust::Str, ::rust::Str, ::rust::Str, ::rust::Str)
{}

void
matrix_clear_session_secrets(::rust::Str)
{}

void
matrix_notify_room_list_snapshot_updated(std::uint64_t, ::rust::Vec<::komai::rust::MatrixRoomSummary>)
{}

void
matrix_notify_ignored_user_list_updated(std::uint64_t, ::rust::Vec<::rust::String>)
{}

void
matrix_notify_initial_sync_ready(std::uint64_t)
{}

void
matrix_notify_room_timeline_snapshot_updated(std::uint64_t, ::rust::Str)
{}

void
matrix_notify_notification_received(std::uint64_t, ::rust::Str, ::rust::Str)
{}

void
matrix_notify_notification_item_received(std::uint64_t, ::komai::rust::MatrixNotificationItem)
{}

void
matrix_notify_call_invite_received(std::uint64_t, ::komai::rust::MatrixCallInviteEvent)
{}

void
matrix_notify_call_candidates_received(std::uint64_t, ::komai::rust::MatrixCallCandidatesEvent)
{}

void
matrix_notify_call_answer_received(std::uint64_t, ::komai::rust::MatrixCallAnswerEvent)
{}

void
matrix_notify_call_hangup_received(std::uint64_t, ::komai::rust::MatrixCallHangUpEvent)
{}

void
matrix_notify_call_select_answer_received(std::uint64_t, ::komai::rust::MatrixCallSelectAnswerEvent)
{}

void
matrix_notify_call_reject_received(std::uint64_t, ::komai::rust::MatrixCallRejectEvent)
{}

void
matrix_notify_call_negotiate_received(std::uint64_t, ::komai::rust::MatrixCallNegotiateEvent)
{}

void
matrix_notify_sync_stopped(std::uint64_t, ::rust::Str, bool)
{}

} // namespace komai::rust_bridge
