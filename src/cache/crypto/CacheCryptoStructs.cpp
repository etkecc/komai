// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/crypto/CacheCryptoStructs.h"

#include <nlohmann/json.hpp>

#include <mtx/events/encrypted.hpp>

MegolmSessionIndex::MegolmSessionIndex(std::string room_id_, const mtx::events::msg::Encrypted &e)
  : room_id(std::move(room_id_))
  , session_id(e.session_id)
{
}

void
to_json(nlohmann::json &j, const UserKeyCache &info)
{
    j["device_keys"]        = info.device_keys;
    j["seen_device_keys"]   = info.seen_device_keys;
    j["seen_device_ids"]    = info.seen_device_ids;
    j["master_keys"]        = info.master_keys;
    j["master_key_changed"] = info.master_key_changed;
    j["user_signing_keys"]  = info.user_signing_keys;
    j["self_signing_keys"]  = info.self_signing_keys;
    j["updated_at"]         = info.updated_at;
    j["last_changed"]       = info.last_changed;
}

void
from_json(const nlohmann::json &j, UserKeyCache &info)
{
    info.device_keys = j.value("device_keys", std::map<std::string, mtx::crypto::DeviceKeys>{});
    info.seen_device_keys   = j.value("seen_device_keys", std::set<std::string>{});
    info.seen_device_ids    = j.value("seen_device_ids", std::set<std::string>{});
    info.master_keys        = j.value("master_keys", mtx::crypto::CrossSigningKeys{});
    info.master_key_changed = j.value("master_key_changed", false);
    info.user_signing_keys  = j.value("user_signing_keys", mtx::crypto::CrossSigningKeys{});
    info.self_signing_keys  = j.value("self_signing_keys", mtx::crypto::CrossSigningKeys{});
    info.updated_at         = j.value("updated_at", "");
    info.last_changed       = j.value("last_changed", "");
}

void
to_json(nlohmann::json &j, const VerificationCache &info)
{
    j["device_verified"] = info.device_verified;
    j["device_blocked"]  = info.device_blocked;
}

void
from_json(const nlohmann::json &j, VerificationCache &info)
{
    info.device_verified = j.at("device_verified").get<std::set<std::string>>();
    info.device_blocked  = j.at("device_blocked").get<std::set<std::string>>();
}

void
to_json(nlohmann::json &j, const OnlineBackupVersion &info)
{
    j["v"] = info.version;
    j["a"] = info.algorithm;
}

void
from_json(const nlohmann::json &j, OnlineBackupVersion &info)
{
    info.version   = j.at("v").get<std::string>();
    info.algorithm = j.at("a").get<std::string>();
}

void
to_json(nlohmann::json &obj, const DeviceKeysToMsgIndex &msg)
{
    obj["deviceids"] = msg.deviceids;
}

void
from_json(const nlohmann::json &obj, DeviceKeysToMsgIndex &msg)
{
    msg.deviceids = obj.at("deviceids").get<decltype(msg.deviceids)>();
}

void
to_json(nlohmann::json &obj, const SharedWithUsers &msg)
{
    obj["keys"] = msg.keys;
}

void
from_json(const nlohmann::json &obj, SharedWithUsers &msg)
{
    msg.keys = obj.at("keys").get<std::map<std::string, DeviceKeysToMsgIndex>>();
}

void
to_json(nlohmann::json &obj, const GroupSessionData &msg)
{
    obj["message_index"] = msg.message_index;
    obj["ts"]            = msg.timestamp;
    obj["trust"]         = msg.trusted;

    obj["sender_key"]                      = msg.sender_key;
    obj["sender_claimed_ed25519_key"]      = msg.sender_claimed_ed25519_key;
    obj["forwarding_curve25519_key_chain"] = msg.forwarding_curve25519_key_chain;

    obj["currently"] = msg.currently;

    obj["indices"] = msg.indices;
}

void
from_json(const nlohmann::json &obj, GroupSessionData &msg)
{
    msg.message_index = obj.at("message_index").get<uint32_t>();
    msg.timestamp     = obj.value("ts", 0ULL);
    msg.trusted       = obj.value("trust", true);

    msg.sender_key                 = obj.value("sender_key", "");
    msg.sender_claimed_ed25519_key = obj.value("sender_claimed_ed25519_key", "");
    msg.forwarding_curve25519_key_chain =
      obj.value("forwarding_curve25519_key_chain", std::vector<std::string>{});

    msg.currently = obj.value("currently", SharedWithUsers{});

    msg.indices = obj.value("indices", std::map<uint32_t, std::string>());
}

void
to_json(nlohmann::json &obj, const DevicePublicKeys &msg)
{
    obj["ed25519"]    = msg.ed25519;
    obj["curve25519"] = msg.curve25519;
}

void
from_json(const nlohmann::json &obj, DevicePublicKeys &msg)
{
    msg.ed25519    = obj.at("ed25519").get<std::string>();
    msg.curve25519 = obj.at("curve25519").get<std::string>();
}

void
to_json(nlohmann::json &obj, const StoredOlmSession &msg)
{
    obj["ts"] = msg.last_message_ts;
    obj["s"]  = msg.pickled_session;
}

void
from_json(const nlohmann::json &obj, StoredOlmSession &msg)
{
    msg.last_message_ts = obj.at("ts").get<uint64_t>();
    msg.pickled_session = obj.at("s").get<std::string>();
}
