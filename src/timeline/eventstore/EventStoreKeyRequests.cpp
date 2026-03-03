// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventStore.h"

#include <QDateTime>

#include <utility>

#include "matrix/MatrixClient.h"

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
EventStore::refetchOnlineKeyBackupKeys()
{
    for (const auto &[session_id, request] : this->pending_key_requests) {
        (void)request;
        olm::lookup_keybackup(this->room_id_, session_id);
    }
}

void
EventStore::requestSession(const mtx::events::EncryptedEvent<mtx::events::msg::Encrypted> &ev,
                           bool manual)
{
    // we may not want to request keys during initial sync and such
    if (suppressKeyRequests)
        return;

    auto copy    = ev;
    copy.room_id = room_id_;
    if (pending_key_requests.count(ev.content.session_id)) {
        auto &r = pending_key_requests.at(ev.content.session_id);
        r.events.push_back(copy);

        // automatically request once every 2 min, manually every 30 s
        qint64 delay = manual ? 30 : (60 * 2);
        if (r.requested_at + delay < QDateTime::currentSecsSinceEpoch()) {
            r.requested_at = QDateTime::currentSecsSinceEpoch();
            olm::lookup_keybackup(room_id_, ev.content.session_id);
            olm::send_key_request_for(copy, r.request_id);
        }
    } else {
        PendingKeyRequests request;
        request.request_id   = "key_request." + http::client()->generate_txn_id();
        request.requested_at = QDateTime::currentSecsSinceEpoch();
        request.events.push_back(copy);
        olm::lookup_keybackup(room_id_, ev.content.session_id);
        olm::send_key_request_for(copy, request.request_id);
        pending_key_requests[ev.content.session_id] = request;
    }
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
