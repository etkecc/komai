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
    using namespace mtx::events;

    nhlog::crypto()->debug("sending key request: sender_key {}, session_id {}",
                           e.content.sender_key,
                           e.content.session_id);

    mtx::events::msg::KeyRequest request;
    request.action = cancel ? mtx::events::msg::RequestAction::Cancellation
                            : mtx::events::msg::RequestAction::Request;

    request.algorithm            = MEGOLM_ALGO;
    request.room_id              = e.room_id;
    request.sender_key           = e.content.sender_key;
    request.session_id           = e.content.session_id;
    request.request_id           = request_id;
    request.requesting_device_id = http::client()->device_id();

    nhlog::crypto()->debug("m.room_key_request: {}", nlohmann::json(request).dump(2));

    std::map<mtx::identifiers::User, std::map<std::string, decltype(request)>> body;
    body[mtx::identifiers::parse<mtx::identifiers::User>(e.sender)]["*"] = request;
    body[http::client()->user_id()]["*"]                                 = request;

    http::client()->send_to_device(
      http::client()->generate_txn_id(), body, [e](mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->warn("failed to send "
                                 "send_to_device "
                                 "message: {}",
                                 err->matrix_error.error);
          }

          nhlog::net()->info(
            "m.room_key_request sent to {}:{} and your own devices", e.sender, e.content.device_id);
      });
}

void
handle_key_request_message(const mtx::events::DeviceEvent<mtx::events::msg::KeyRequest> &req)
{
    if (req.content.algorithm != MEGOLM_ALGO) {
        nhlog::crypto()->debug("ignoring key request {} with invalid algorithm: {}",
                               req.content.request_id,
                               req.content.algorithm);
        return;
    }

    // Check that the requested session_id and the one we have saved match.
    MegolmSessionIndex index{};
    index.room_id    = req.content.room_id;
    index.session_id = req.content.session_id;

    // Check if we have the keys for the requested session.
    auto sessionData = cache::getMegolmSessionData(index);
    if (!sessionData) {
        nhlog::crypto()->warn("requested session not found in room: {}", req.content.room_id);
        return;
    }

    // Check if we were the sender of the session being requested (unless it is actually us
    // requesting the session).
    if (req.sender != http::client()->user_id().to_string() &&
        sessionData->sender_key != olm::client()->identity_keys().curve25519) {
        nhlog::crypto()->debug(
          "ignoring key request {} because we did not create the requested session: "
          "\nrequested({}) ours({})",
          req.content.request_id,
          sessionData->sender_key,
          olm::client()->identity_keys().curve25519);
        return;
    }

    const auto session = cache::getInboundMegolmSession(index);
    if (!session) {
        nhlog::crypto()->warn("No session with id {} in db", req.content.session_id);
        return;
    }

    if (!cache::isRoomMember(req.sender, req.content.room_id)) {
        nhlog::crypto()->warn("user {} that requested the session key is not member of the room {}",
                              req.sender,
                              req.content.room_id);
        return;
    }

    // check if device is verified
    auto verificationStatus = cache::verificationStatus(req.sender);
    bool verifiedDevice     = false;
    if (verificationStatus &&
        // Share keys, if the option to share with trusted users is enabled or with yourself
        (ChatPage::instance()->userSettings()->encryptionKeySharingShareWithTrusted() ||
         req.sender == http::client()->user_id().to_string())) {
        for (const auto &dev : verificationStatus->verified_devices) {
            if (dev == req.content.requesting_device_id) {
                verifiedDevice = true;
                nhlog::crypto()->debug("Verified device: {}", dev);
                break;
            }
        }
    }

    bool shouldSeeKeys    = false;
    uint32_t minimumIndex = -1;
    if (sessionData->currently.keys.count(req.sender)) {
        if (sessionData->currently.keys.at(req.sender)
              .deviceids.count(req.content.requesting_device_id)) {
            shouldSeeKeys = true;
            minimumIndex  = sessionData->currently.keys.at(req.sender)
                             .deviceids.at(req.content.requesting_device_id);
        }
    }

    if (!verifiedDevice && !shouldSeeKeys) {
        nhlog::crypto()->debug("ignoring key request for room {}", req.content.room_id);
        return;
    }

    if (verifiedDevice) {
        // share the minimum index we have
        minimumIndex = -1;
    }

    try {
        auto session_key = mtx::crypto::export_session(session.get(), minimumIndex);

        //
        // Prepare the m.room_key event.
        //
        mtx::events::msg::ForwardedRoomKey forward_key{};
        forward_key.algorithm   = MEGOLM_ALGO;
        forward_key.room_id     = index.room_id;
        forward_key.session_id  = index.session_id;
        forward_key.session_key = session_key;
        forward_key.sender_key  = sessionData->sender_key;

        // TODO(Nico): Figure out if this is correct
        forward_key.sender_claimed_ed25519_key      = sessionData->sender_claimed_ed25519_key;
        forward_key.forwarding_curve25519_key_chain = sessionData->forwarding_curve25519_key_chain;

        send_megolm_key_to_device(req.sender, req.content.requesting_device_id, forward_key);
    } catch (std::exception &e) {
        nhlog::crypto()->error("Failed to forward session key: {}", e.what());
    }
}

void
send_megolm_key_to_device(const std::string &user_id,
                          const std::string &device_id,
                          const mtx::events::msg::ForwardedRoomKey &payload)
{
    mtx::events::DeviceEvent<mtx::events::msg::ForwardedRoomKey> room_key;
    room_key.content = payload;
    room_key.type    = mtx::events::EventType::ForwardedRoomKey;

    std::map<std::string, std::vector<std::string>> targets;
    targets[user_id] = {device_id};
    send_encrypted_to_device_messages(targets, room_key);
    nhlog::crypto()->debug("Forwarded key to {}:{}", user_id, device_id);
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
