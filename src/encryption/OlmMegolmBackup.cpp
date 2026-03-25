// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include <nlohmann/json.hpp>

#include "logging/Logging.h"

namespace olm {

mtx::events::msg::Encrypted
encrypt_group_message(const std::string &room_id, const std::string &device_id, nlohmann::json body)
{
    (void)body;
    nhlog::crypto()->warn(
      "Ignoring legacy Megolm room-message encryption for room '{}' on device '{}'; this flow "
      "is not migrated to the matrix-sdk backend yet",
      room_id,
      device_id);
    return {};
}

void
create_inbound_megolm_session(const mtx::events::DeviceEvent<mtx::events::msg::RoomKey> &roomKey,
                              const std::string &sender_key,
                              const std::string &sender_ed25519)
{
    nhlog::crypto()->warn(
      "Ignoring legacy inbound Megolm session creation for room '{}' session '{}' from sender "
      "key '{}' (ed25519 '{}'); this flow is not migrated to the matrix-sdk backend yet",
      roomKey.content.room_id,
      roomKey.content.session_id,
      sender_key,
      sender_ed25519);
}

void
import_inbound_megolm_session(
  const mtx::events::DeviceEvent<mtx::events::msg::ForwardedRoomKey> &roomKey)
{
    nhlog::crypto()->warn(
      "Ignoring legacy forwarded Megolm session import for room '{}' session '{}'; this flow "
      "is not migrated to the matrix-sdk backend yet",
      roomKey.content.room_id,
      roomKey.content.session_id);
}

void
mark_keys_as_published()
{
    nhlog::crypto()->warn(
      "Ignoring legacy Olm key-publication bookkeeping; this flow is not migrated to the "
      "matrix-sdk backend yet");
}

} // namespace olm
