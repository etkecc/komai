// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <string_view>

#include <nlohmann/json.hpp>

#include "ChatPage.h"
#include <spdlog/logger.h>

#include "cache/api/CacheApiWrappers.h"
#include "db/Json.h"
#include "db/MegolmIndex.h"
#include "db/Serde.h"
#include "db/StorageApi.h"
#include "db/SyncState.h"
#include "encryption/Olm.h"

void
Cache::storeEventExpirationProgress(const std::string &room,
                                    const std::string &expirationSettings,
                                    const std::string &stopMarker)
{
    nlohmann::json j;
    j["s"] = expirationSettings;
    j["m"] = stopMarker;

    auto txn = beginTxn();
    db->eventExpiryBgJob_.put(txn, room, j.dump());
    txn.commit();
}

std::string
Cache::loadEventExpirationProgress(const std::string &room, const std::string &expirationSettings)

{
    try {
        auto txn = ro_txn(storage());
        std::string_view data;
        if (!db->eventExpiryBgJob_.get(txn, room, data))
            return "";

        auto j = nlohmann::json::parse(data);
        if (j.value("s", "") == expirationSettings)
            return j.value("m", "");
    } catch (...) {
        return "";
    }
    return "";
}

void
Cache::setEncryptedRoom(db::Transaction &txn, const std::string &room_id)
{
    cache::activeLoggers().db->info("mark room {} as encrypted", room_id);

    db->encryptedRooms_.put(txn, room_id, "0");
}

bool
Cache::isRoomEncrypted(const std::string &room_id)
{
    std::string_view unused;

    auto txn = ro_txn(storage());
    auto res = db->encryptedRooms_.get(txn, room_id, unused);

    return res;
}

std::optional<mtx::events::state::Encryption>
Cache::roomEncryptionSettings(const std::string &room_id)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    try {
        auto txn      = ro_txn(storage());
        auto statesdb = getStatesDb(txn, room_id);
        if (auto msg = db::getJsonValue<StateEvent<Encryption>>(
              txn, statesdb, to_string(mtx::events::EventType::RoomEncryption))) {
            return msg->content;
        }
    } catch (db::Error &) {
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.encryption event: {}", e.what());
    }

    return std::nullopt;
}

mtx::crypto::ExportedSessionKeys
Cache::exportSessionKeys()
{
    using namespace mtx::crypto;

    ExportedSessionKeys keys;

    auto txn = ro_txn(storage());
    db::forEachEntry(
      txn, db->inboundMegolmSessions, [&](std::string_view key, std::string_view value) {
          ExportedSession exported;
          MegolmSessionIndex index;

          auto saved_session = unpickle<InboundSessionObject>(std::string(value), pickle_secret_);

          try {
              if (!db::parseMegolmSessionKey(key, index.room_id, index.session_id)) {
                  cache::activeLoggers().db->critical(
                    "failed to export megolm session: invalid index key");
                  return true;
              }
          } catch (...) {
              cache::activeLoggers().db->critical(
                "failed to export megolm session: invalid index key");
              return true;
          }

          try {
              const auto key = db::megolmSessionKey(index.room_id, index.session_id);
              const auto data =
                db::getJsonValue<GroupSessionData>(txn, db->megolmSessionsData, key);
              if (!data)
                  return true;

              exported.sender_key = data->sender_key;
              if (!data->sender_claimed_ed25519_key.empty())
                  exported.sender_claimed_keys["ed25519"] = data->sender_claimed_ed25519_key;
              exported.forwarding_curve25519_key_chain = data->forwarding_curve25519_key_chain;
          } catch (const std::exception &e) {
              cache::activeLoggers().db->error("Failed to retrieve Megolm Session Data: {}",
                                               e.what());
              return true;
          }

          exported.room_id     = index.room_id;
          exported.session_id  = index.session_id;
          exported.session_key = export_session(saved_session.get(), -1);

          keys.sessions.push_back(exported);
          return true;
      });

    return keys;
}

void
Cache::importSessionKeys(const mtx::crypto::ExportedSessionKeys &keys)
{
    std::size_t importCount = 0;

    auto txn = beginTxn();
    for (const auto &s : keys.sessions) {
        MegolmSessionIndex index;
        index.room_id    = s.room_id;
        index.session_id = s.session_id;

        GroupSessionData data{};
        data.sender_key                      = s.sender_key;
        data.forwarding_curve25519_key_chain = s.forwarding_curve25519_key_chain;
        data.trusted                         = false;

        if (s.sender_claimed_keys.count("ed25519"))
            data.sender_claimed_ed25519_key = s.sender_claimed_keys.at("ed25519");

        try {
            auto exported_session = mtx::crypto::import_session(s.session_key);

            using namespace mtx::crypto;
            const auto pickled =
              pickle<InboundSessionObject>(exported_session.get(), pickle_secret_);

            std::string_view value;
            if (db::getInboundMegolmSessionValue(
                  txn, db->inboundMegolmSessions, index.room_id, index.session_id, value)) {
                auto oldSession =
                  unpickle<InboundSessionObject>(std::string(value), pickle_secret_);
                if (olm_inbound_group_session_first_known_index(exported_session.get()) >=
                    olm_inbound_group_session_first_known_index(oldSession.get())) {
                    cache::activeLoggers().crypto->warn(
                      "Not storing inbound session with newer or equal first known index");
                    continue;
                }
            }

            db::putInboundMegolmSessionValue(
              txn, db->inboundMegolmSessions, index.room_id, index.session_id, pickled);
            db::putMegolmSessionDataValue(txn,
                                          db->megolmSessionsData,
                                          index.room_id,
                                          index.session_id,
                                          nlohmann::json(data).dump());

            ChatPage::instance()->receivedSessionKey(index.room_id, index.session_id);
            importCount++;
        } catch (const mtx::crypto::olm_exception &e) {
            cache::activeLoggers().crypto->critical(
              "failed to import inbound megolm session {}: {}", index.session_id, e.what());
            continue;
        } catch (const db::Error &e) {
            cache::activeLoggers().crypto->critical(
              "failed to save inbound megolm session {}: {}", index.session_id, e.what());
            continue;
        }
    }
    txn.commit();

    cache::activeLoggers().crypto->info(
      "Imported {} out of {} keys", importCount, keys.sessions.size());
}
