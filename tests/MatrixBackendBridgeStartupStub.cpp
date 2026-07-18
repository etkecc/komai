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
settings_profile_directory(::rust::Str profile_id)
{
    return ::rust::String(
      settings::storage::profileDirPath(QString::fromStdString(std::string(profile_id)))
        .toStdString());
}

::rust::String
settings_secure_store_key(::rust::Str profile_id, ::rust::Str key_name)
{
    const auto keyName = std::string(key_name);
    return ::rust::String(
      settings::storage::secureStoreKey(QString::fromStdString(std::string(profile_id)),
                                        keyName.c_str())
        .toStdString());
}

::komai::rust::SettingsOptionalString
settings_read_secure_value(::rust::Str key)
{
    const auto value = settings::storage::readSecureValue(QString::fromStdString(std::string(key)));
    return {
      .has_value = value.has_value(),
      .value     = ::rust::String(value ? value->toStdString() : std::string()),
    };
}

void
settings_write_secure_value(::rust::Str key, ::rust::Str value)
{
    settings::storage::writeSecureValue(QString::fromStdString(std::string(key)),
                                        QString::fromStdString(std::string(value)));
}

bool
settings_write_secure_value_blocking(::rust::Str key, ::rust::Str value)
{
    return settings::storage::writeSecureValueBlocking(QString::fromStdString(std::string(key)),
                                                       QString::fromStdString(std::string(value)));
}

void
settings_delete_secure_value(::rust::Str key)
{
    settings::storage::deleteSecureValue(QString::fromStdString(std::string(key)));
}

bool
settings_delete_secure_value_blocking(::rust::Str key)
{
    return settings::storage::deleteSecureValueBlocking(QString::fromStdString(std::string(key)));
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
settings_path_exists(::rust::Str path)
{
    return settings::storage::pathExists(QString::fromStdString(std::string(path)));
}

bool
settings_remove_path(::rust::Str path)
{
    return settings::storage::removePath(QString::fromStdString(std::string(path)));
}

bool
settings_write_text_file(::rust::Str path, ::rust::Str content, bool owner_read_write_only)
{
    return settings::storage::writeTextFile(QString::fromStdString(std::string(path)),
                                            QString::fromStdString(std::string(content)),
                                            owner_read_write_only);
}

bool
settings_delete_all_profile_secrets_from_store(::rust::Str, bool)
{
    return true;
}

::komai::rust::MatrixPersistedSessionSecrets
matrix_load_session_secrets(::rust::Str)
{
    return {};
}

bool
matrix_save_session_secrets(::rust::Str, ::rust::Str, ::rust::Str, ::rust::Str)
{
    return true;
}

void
matrix_clear_session_secrets(::rust::Str)
{}

void
matrix_notify_room_list_snapshot_updated(std::uint64_t, ::rust::Vec<::komai::rust::MatrixRoomSummary>)
{}

void
matrix_notify_room_previews_backfilled(std::uint64_t, ::rust::Vec<::komai::rust::MatrixRoomPreviewUpdate>)
{}

void
matrix_notify_ignored_user_list_updated(std::uint64_t, ::rust::Vec<::rust::String>)
{}

void
matrix_notify_initial_sync_ready(std::uint64_t)
{}

void
matrix_notify_sync_connection_state_changed(std::uint64_t, bool)
{}

void
matrix_notify_room_timeline_snapshot_updated(std::uint64_t, ::rust::Str)
{}

void
matrix_notify_room_timeline_pagination_state(std::uint64_t, ::rust::Str, bool)
{}

void
matrix_notify_room_pinned_events_changed(std::uint64_t,
                                         ::rust::Str,
                                         ::rust::Vec<::rust::String>)
{}

void
matrix_notify_thread_timeline_snapshot_updated(std::uint64_t, ::rust::Str, ::rust::Str)
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

void
matrix_notify_typing_users_updated(std::uint64_t, ::rust::Str, ::rust::Vec<::rust::String>)
{}

void
matrix_notify_element_call_widget_url_ready(std::uint64_t, ::rust::Str)
{}

void
matrix_notify_element_call_widget_message(std::uint64_t, ::rust::Str)
{}

void
matrix_notify_element_call_widget_stopped(std::uint64_t, ::rust::Str)
{}

void
matrix_notify_rtc_notification(std::uint64_t, ::komai::rust::MatrixRtcNotificationEvent)
{}

void
matrix_notify_rtc_decline(std::uint64_t, ::komai::rust::MatrixRtcDeclineEvent)
{}

} // namespace komai::rust_bridge
