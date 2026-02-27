// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"
#include "db/Json.h"
#include "db/MegolmIndex.h"
#include "db/Serde.h"
#include "db/StorageApi.h"
#include "db/SyncState.h"
#include "encryption/Olm.h"

//
// Session Management
//

void
Cache::saveInboundMegolmSession(const MegolmSessionIndex &index,
                                mtx::crypto::InboundGroupSessionPtr session,
                                const GroupSessionData &data)
{
    using namespace mtx::crypto;
    const auto pickled = pickle<InboundSessionObject>(session.get(), pickle_secret_);

    auto txn = beginTxn();

    std::string_view value;
    if (db::getInboundMegolmSessionValue(
          txn, db->inboundMegolmSessions, index.room_id, index.session_id, value)) {
        auto oldSession = unpickle<InboundSessionObject>(std::string(value), pickle_secret_);

        auto newIndex = olm_inbound_group_session_first_known_index(session.get());
        auto oldIndex = olm_inbound_group_session_first_known_index(oldSession.get());

        // merge trusted > untrusted
        // first known index minimum
        if (auto data = db::getJsonValue<GroupSessionData>(
              txn, db->megolmSessionsData, db::megolmSessionKey(index.room_id, index.session_id))) {
            auto oldData = std::move(*data);
            if (oldData.trusted && newIndex >= oldIndex) {
                cache::activeLoggers().crypto->warn(
                  "Not storing inbound session of lesser trust or bigger index.");
                return;
            }

            oldData.trusted = data->trusted || oldData.trusted;

            if (newIndex < oldIndex) {
                db::putInboundMegolmSessionValue(
                  txn, db->inboundMegolmSessions, index.room_id, index.session_id, pickled);
                oldData.message_index = newIndex;
            }

            db::putMegolmSessionDataValue(txn,
                                          db->megolmSessionsData,
                                          index.room_id,
                                          index.session_id,
                                          nlohmann::json(oldData).dump());
            txn.commit();
            return;
        }
    }

    db::putInboundMegolmSessionValue(
      txn, db->inboundMegolmSessions, index.room_id, index.session_id, pickled);
    db::putMegolmSessionDataValue(
      txn, db->megolmSessionsData, index.room_id, index.session_id, nlohmann::json(data).dump());
    txn.commit();
}

mtx::crypto::InboundGroupSessionPtr
Cache::getInboundMegolmSession(const MegolmSessionIndex &index)
{
    using namespace mtx::crypto;

    try {
        auto txn = ro_txn(storage());
        std::string_view value;

        if (db::getInboundMegolmSessionValue(
              txn, db->inboundMegolmSessions, index.room_id, index.session_id, value)) {
            auto session = unpickle<InboundSessionObject>(std::string(value), pickle_secret_);
            return session;
        }
    } catch (std::exception &e) {
        cache::activeLoggers().crypto->error("Failed to get inbound megolm session {}", e.what());
    }

    return nullptr;
}

bool
Cache::inboundMegolmSessionExists(const MegolmSessionIndex &index)
{
    using namespace mtx::crypto;

    try {
        auto txn = ro_txn(storage());
        std::string_view value;

        return db::getInboundMegolmSessionValue(
          txn, db->inboundMegolmSessions, index.room_id, index.session_id, value);
    } catch (std::exception &e) {
        cache::activeLoggers().crypto->error("Failed to get inbound megolm session {}", e.what());
    }

    return false;
}

void
Cache::updateOutboundMegolmSession(const std::string &room_id,
                                   const GroupSessionData &data_,
                                   mtx::crypto::OutboundGroupSessionPtr &ptr)
{
    using namespace mtx::crypto;

    if (!outboundMegolmSessionExists(room_id))
        return;

    GroupSessionData data = data_;
    data.message_index    = olm_outbound_group_session_message_index(ptr.get());
    MegolmSessionIndex index;
    index.room_id    = room_id;
    index.session_id = mtx::crypto::session_id(ptr.get());

    // Save the updated pickled data for the session.
    nlohmann::json j;
    j["session"] = pickle<OutboundSessionObject>(ptr.get(), pickle_secret_);

    auto txn = beginTxn();
    db->outboundMegolmSessions.put(txn, room_id, j.dump());
    db::putMegolmSessionDataValue(
      txn, db->megolmSessionsData, index.room_id, index.session_id, nlohmann::json(data).dump());
    txn.commit();
}

void
Cache::dropOutboundMegolmSession(const std::string &room_id)
{
    using namespace mtx::crypto;

    if (!outboundMegolmSessionExists(room_id))
        return;

    {
        auto txn = beginTxn();
        db->outboundMegolmSessions.del(txn, room_id);
        // don't delete session data, so that we can still share the session.
        txn.commit();
    }
}

void
Cache::saveOutboundMegolmSession(const std::string &room_id,
                                 const GroupSessionData &data_,
                                 mtx::crypto::OutboundGroupSessionPtr &session)
{
    using namespace mtx::crypto;
    const auto pickled = pickle<OutboundSessionObject>(session.get(), pickle_secret_);

    GroupSessionData data = data_;
    data.message_index    = olm_outbound_group_session_message_index(session.get());
    MegolmSessionIndex index;
    index.room_id    = room_id;
    index.session_id = mtx::crypto::session_id(session.get());

    nlohmann::json j;
    j["session"] = pickled;

    auto txn = beginTxn();
    db->outboundMegolmSessions.put(txn, room_id, j.dump());
    db::putMegolmSessionDataValue(
      txn, db->megolmSessionsData, index.room_id, index.session_id, nlohmann::json(data).dump());
    txn.commit();
}

bool
Cache::outboundMegolmSessionExists(const std::string &room_id) noexcept
{
    try {
        auto txn = ro_txn(storage());
        std::string_view value;
        return db->outboundMegolmSessions.get(txn, room_id, value);
    } catch (std::exception &e) {
        cache::activeLoggers().crypto->error("Failed to retrieve outbound Megolm Session: {}",
                                             e.what());
        return false;
    }
}

OutboundGroupSessionDataRef
Cache::getOutboundMegolmSession(const std::string &room_id)
{
    try {
        using namespace mtx::crypto;

        auto txn = ro_txn(storage());
        std::string_view value;
        db->outboundMegolmSessions.get(txn, room_id, value);

        auto obj = nlohmann::json::parse(value);

        OutboundGroupSessionDataRef ref{};
        ref.session =
          unpickle<OutboundSessionObject>(obj.at("session").get<std::string>(), pickle_secret_);

        MegolmSessionIndex index;
        index.room_id    = room_id;
        index.session_id = mtx::crypto::session_id(ref.session.get());

        if (auto data = db::getJsonValue<GroupSessionData>(
              txn, db->megolmSessionsData, db::megolmSessionKey(index.room_id, index.session_id))) {
            ref.data = std::move(*data);
        }

        return ref;
    } catch (std::exception &e) {
        cache::activeLoggers().crypto->error("Failed to retrieve outbound Megolm Session: {}",
                                             e.what());
        return {};
    }
}

std::optional<GroupSessionData>
Cache::getMegolmSessionData(const MegolmSessionIndex &index)
{
    try {
        using namespace mtx::crypto;

        auto txn = ro_txn(storage());
        if (auto data = db::getJsonValue<GroupSessionData>(
              txn, db->megolmSessionsData, db::megolmSessionKey(index.room_id, index.session_id))) {
            return data;
        }

        return std::nullopt;
    } catch (std::exception &e) {
        cache::activeLoggers().crypto->error("Failed to retrieve Megolm Session Data: {}",
                                             e.what());
        return std::nullopt;
    }
}
