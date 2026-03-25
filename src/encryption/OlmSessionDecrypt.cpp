// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include <nlohmann/json.hpp>

#include "logging/Logging.h"

namespace olm {

nlohmann::json
try_olm_decryption(const std::string &sender_key, const mtx::events::msg::OlmCipherContent &msg)
{
    nhlog::crypto()->warn(
      "Ignoring legacy Olm session decrypt attempt for sender key '{}' (message type {}); "
      "this flow is not migrated to the matrix-sdk backend yet",
      sender_key,
      msg.type);
    return {};
}

nlohmann::json
handle_pre_key_olm_message(const std::string &sender,
                           const std::string &sender_key,
                           const mtx::events::msg::OlmCipherContent &content)
{
    nhlog::crypto()->warn(
      "Ignoring legacy pre-key Olm message from '{}' with sender key '{}' (message type {}); "
      "this flow is not migrated to the matrix-sdk backend yet",
      sender,
      sender_key,
      content.type);
    return {};
}

} // namespace olm
