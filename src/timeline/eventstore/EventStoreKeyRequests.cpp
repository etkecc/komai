// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventStore.h"

#include "logging/Logging.h"

void
EventStore::receivedSessionKey(const std::string &session_id)
{
    nhlog::crypto()->warn(
      "Ignoring legacy EventStore room-session-key callback for room '{}' session '{}'; this "
      "flow is not migrated to the matrix-sdk backend yet",
      room_id_,
      session_id);
}

void
EventStore::clearDecryptionErrors()
{
    nhlog::crypto()->warn(
      "Ignoring legacy EventStore clear-decryption-errors request for room '{}'; this flow is "
      "not migrated to the matrix-sdk backend yet",
      room_id_);
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

    nhlog::crypto()->warn(
      "Ignoring legacy missing-session request for room {}; this flow is not migrated to the "
      "matrix-sdk backend yet",
      room_id_);
}

void
EventStore::enableKeyRequests(bool suppressKeyRequests_)
{
    suppressKeyRequests = suppressKeyRequests_;
}
