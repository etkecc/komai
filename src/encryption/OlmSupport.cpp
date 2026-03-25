// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include <nlohmann/json.hpp>

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {
constexpr auto MEGOLM_ALGO = "m.megolm.v1.aes-sha2";
constexpr auto OLM_ALGO    = "m.olm.v1.curve25519-aes-sha2";
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

mtx::events::msg::Encrypted
encrypt_group_message_with_session(mtx::crypto::OutboundGroupSessionPtr &session,
                                   const std::string &device_id,
                                   nlohmann::json body)
{
    using namespace mtx::events;

    // relations shouldn't be encrypted...
    mtx::common::Relations relations = mtx::common::parse_relations(body["content"]);

    auto payload = olm::client()->encrypt_group_message(session.get(), body.dump());

    // Prepare the m.room.encrypted event.
    msg::Encrypted data;
    data.ciphertext = std::string((char *)payload.data(), payload.size());
    data.sender_key = olm::client()->identity_keys().curve25519;
    data.session_id = mtx::crypto::session_id(session.get());
    data.device_id  = device_id;
    data.algorithm  = MEGOLM_ALGO;
    data.relations  = relations;

    return data;
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
    try {
        if (!cache::inboundMegolmSessionExists(index)) {
            return {DecryptionErrorCode::MissingSession, std::nullopt, std::nullopt};
        }
    } catch (const std::exception &e) {
        return {DecryptionErrorCode::DbError, e.what(), std::nullopt};
    }

    std::string msg_str;
    try {
        auto session = cache::getInboundMegolmSession(index);
        if (!session) {
            return {DecryptionErrorCode::MissingSession, std::nullopt, std::nullopt};
        }

        auto sessionData = cache::getMegolmSessionData(index).value_or(GroupSessionData{});

        auto res = olm::client()->decrypt_group_message(session.get(), event.content.ciphertext);
        msg_str  = std::string((char *)res.data.data(), res.data.size());

        if (!event.event_id.empty() && event.event_id[0] == '$') {
            auto oldIdx = sessionData.indices.find(res.message_index);
            if (oldIdx != sessionData.indices.end()) {
                if (oldIdx->second != event.event_id)
                    return {DecryptionErrorCode::ReplayAttack, std::nullopt, std::nullopt};
            } else if (!dont_write_db) {
                sessionData.indices[res.message_index] = event.event_id;
                cache::saveInboundMegolmSession(index, std::move(session), sessionData);
            }
        }
    } catch (const mtx::crypto::olm_exception &e) {
        if (e.error_code() == mtx::crypto::OlmErrorCode::OLM_UNKNOWN_MESSAGE_INDEX)
            return {DecryptionErrorCode::MissingSessionIndex, e.what(), std::nullopt};
        return {DecryptionErrorCode::DecryptionFailed, e.what(), std::nullopt};
    } catch (const std::exception &e) {
        return {DecryptionErrorCode::DbError, e.what(), std::nullopt};
    }

    try {
        // Add missing fields for the event.
        nlohmann::json body      = nlohmann::json::parse(msg_str);
        body["event_id"]         = event.event_id;
        body["sender"]           = event.sender;
        body["origin_server_ts"] = event.origin_server_ts;
        body["unsigned"]         = event.unsigned_data;

        mtx::events::collections::TimelineEvents te =
          body.get<mtx::events::collections::TimelineEvents>();

        // relations are unencrypted in content...
        mtx::accessors::set_relations(te, std::move(event.content.relations));

        return {DecryptionErrorCode::NoError, std::nullopt, std::move(te)};
    } catch (std::exception &e) {
        return {DecryptionErrorCode::ParsingFailed, e.what(), std::nullopt};
    }
}

crypto::Trust
calculate_trust(const std::string &user_id,
                const std::string &room_id,
                const mtx::events::msg::Encrypted &event)
{
    auto index               = MegolmSessionIndex(room_id, event);
    auto megolmData          = cache::getMegolmSessionData(index);
    crypto::Trust trustlevel = crypto::Trust::MessageUnverified;

    try {
        auto session = cache::getInboundMegolmSession(index);
        if (!session) {
            return trustlevel;
        }

        olm::client()->decrypt_group_message(session.get(), event.ciphertext);
    } catch (const mtx::crypto::olm_exception &e) {
        return trustlevel;
    } catch (const std::exception &e) {
        return trustlevel;
    }

    auto status = cache::verificationStatus(user_id);

    if (megolmData && megolmData->trusted && status &&
        status->verified_device_keys.count(megolmData->sender_key)) {
        trustlevel = status->verified_device_keys.at(megolmData->sender_key);
    }

    return trustlevel;
}

} // namespace olm
