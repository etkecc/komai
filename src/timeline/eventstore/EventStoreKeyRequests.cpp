// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventStore.h"

#include <utility>

#include "logging/Logging.h"

void
EventStore::receivedSessionKey(const std::string &session_id)
{
    if (!pending_key_requests.count(session_id))
        return;

    auto request = pending_key_requests.at(session_id);

    // Don't request keys again until Komai is restarted (for now)
    pending_key_requests[session_id].events.clear();

    if (!request.events.empty())
        olm::send_key_request_for(request.events.front(), request.request_id, true);

    for (const auto &e : request.events) {
        auto idx = idToIndex(e.event_id);
        if (idx) {
            decryptedEvents_.remove({room_id_, e.event_id});
            events_by_id_.remove({room_id_, e.event_id});
            events_.remove({room_id_, toInternalIdx(*idx)});
            emit dataChanged(*idx, *idx);
        }

        if (auto edit = e.content.relations.replaces()) {
            auto edit_idx = idToIndex(edit.value());
            if (edit_idx) {
                decryptedEvents_.remove({room_id_, e.event_id});
                events_by_id_.remove({room_id_, e.event_id});
                events_.remove({room_id_, toInternalIdx(*edit_idx)});
                emit dataChanged(*edit_idx, *edit_idx);
            }
        }
    }
}

void
EventStore::clearDecryptionErrors()
{
    auto keys = decryptedEvents_.keys();
    for (const auto &key : std::as_const(keys)) {
        if (key.room == this->room_id_)
            decryptedEvents_.remove(key);
    }

    if (size() > 0)
        emit dataChanged(0, size() - 1);
}

void
EventStore::refetchOnlineKeyBackupKeys()
{
    if (!this->pending_key_requests.empty()) {
        nhlog::crypto()->warn(
          "Ignoring legacy key-backup refetch for room {}; this flow is not migrated to the "
          "matrix-sdk backend yet",
          this->room_id_);
    }
}

void
EventStore::requestSession(const mtx::events::EncryptedEvent<mtx::events::msg::Encrypted> &ev,
                           bool manual)
{
    (void)ev;
    (void)manual;
    // we may not want to request keys during initial sync and such
    if (suppressKeyRequests)
        return;

    nhlog::crypto()->warn(
      "Ignoring legacy missing-session request for room {}; this flow is not migrated to the "
      "matrix-sdk backend yet",
      room_id_);
}

void
EventStore::enableKeyRequests(bool suppressKeyRequests_)
{
    if (!suppressKeyRequests_) {
        auto keys = decryptedEvents_.keys();
        for (const auto &key : std::as_const(keys))
            if (key.room == this->room_id_)
                decryptedEvents_.remove(key);
        suppressKeyRequests = false;
    } else
        suppressKeyRequests = true;
}
