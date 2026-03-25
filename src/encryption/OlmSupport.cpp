// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include <nlohmann/json.hpp>

#include "logging/Logging.h"

namespace olm {

mtx::events::msg::Encrypted
encrypt_group_message_with_session(mtx::crypto::OutboundGroupSessionPtr &session,
                                   const std::string &device_id,
                                   nlohmann::json body)
{
    (void)session;
    (void)body;
    nhlog::crypto()->warn(
      "Ignoring legacy group-message encryption for device '{}'; this flow is not migrated to "
      "the matrix-sdk backend yet",
      device_id);
    return {};
}

void
send_key_request_for(mtx::events::EncryptedEvent<mtx::events::msg::Encrypted> e,
                     const std::string &request_id,
                     bool cancel)
{
    nhlog::crypto()->warn(
      "Ignoring legacy room-key {} for {}:{} (request_id={}); this flow is not migrated to "
      "the matrix-sdk backend yet",
      cancel ? "cancellation" : "request",
      e.sender,
      e.content.device_id,
      request_id);
}

void
handle_key_request_message(const mtx::events::DeviceEvent<mtx::events::msg::KeyRequest> &req)
{
    nhlog::crypto()->warn(
      "Ignoring legacy inbound room-key request {} from {}:{}; this flow is not migrated to "
      "the matrix-sdk backend yet",
      req.content.request_id,
      req.sender,
      req.content.requesting_device_id);
}

void
send_megolm_key_to_device(const std::string &user_id,
                          const std::string &device_id,
                          const mtx::events::msg::ForwardedRoomKey &payload)
{
    nhlog::crypto()->warn(
      "Ignoring legacy forwarded room-key send to {}:{} for session {}; this flow is not "
      "migrated to the matrix-sdk backend yet",
      user_id,
      device_id,
      payload.session_id);
}

DecryptionResult
decryptEvent(const MegolmSessionIndex &index,
             const mtx::events::EncryptedEvent<mtx::events::msg::Encrypted> &event,
             bool dont_write_db)
{
    (void)index;
    (void)event;
    (void)dont_write_db;
    return {DecryptionErrorCode::MissingSession,
            std::optional<std::string>("Legacy Megolm decryption is not migrated yet"),
            std::nullopt};
}

crypto::Trust
calculate_trust(const std::string &user_id,
                const std::string &room_id,
                const mtx::events::msg::Encrypted &event)
{
    (void)user_id;
    (void)room_id;
    (void)event;
    return crypto::Trust::MessageUnverified;
}

} // namespace olm
