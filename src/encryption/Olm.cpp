// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include <stdexcept>

#include <nlohmann/json.hpp>

#include "logging/Logging.h"

namespace {
constexpr auto OLM_ALGO = "m.olm.v1.curve25519-aes-sha2";
}

namespace olm {

void
from_json(const nlohmann::json &obj, OlmMessage &msg)
{
    if (obj.at("type") != "m.room.encrypted")
        throw std::invalid_argument("invalid type for olm message");

    if (obj.at("content").at("algorithm") != OLM_ALGO)
        throw std::invalid_argument("invalid algorithm for olm message");

    msg.sender     = obj.at("sender").get<std::string>();
    msg.sender_key = obj.at("content").at("sender_key").get<std::string>();
    msg.ciphertext = obj.at("content")
                       .at("ciphertext")
                       .get<std::map<std::string, mtx::events::msg::OlmCipherContent>>();
}

mtx::crypto::OlmClient *
client()
{
    return nullptr;
}

void
handle_to_device_messages(const std::vector<mtx::events::collections::DeviceEvents> &msgs)
{
    if (msgs.empty())
        return;

    nhlog::crypto()->warn(
      "Ignoring {} legacy to-device messages; this flow is not migrated to the matrix-sdk "
      "backend yet",
      msgs.size());
}

void
handle_olm_message(const OlmMessage &msg, const UserKeyCache &otherUserDeviceKeys)
{
    (void)otherUserDeviceKeys;
    nhlog::crypto()->warn(
      "Ignoring legacy Olm message from '{}' with sender key '{}'; this flow is not migrated "
      "to the matrix-sdk backend yet",
      msg.sender,
      msg.sender_key);
}

void
request_cross_signing_keys()
{
    nhlog::crypto()->warn(
      "Ignoring legacy cross-signing secret request; this flow is not migrated to the "
      "matrix-sdk backend yet");
}

} // namespace olm

#include "moc_Olm.cpp"
