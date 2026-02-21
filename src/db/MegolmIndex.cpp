// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/MegolmIndex.h"

#include <exception>

#include <nlohmann/json.hpp>

#include "db/DbTypes.h"

namespace {

std::string
jsonSessionKey(std::string_view roomId, std::string_view sessionId)
{
    nlohmann::json key;
    key["room_id"]    = roomId;
    key["session_id"] = sessionId;
    return key.dump();
}

} // namespace

namespace db {

std::string
megolmSessionKey(std::string_view roomId, std::string_view sessionId)
{
    return jsonSessionKey(roomId, sessionId);
}

bool
parseMegolmSessionKey(std::string_view key, std::string &roomId, std::string &sessionId) noexcept
{
    try {
        const auto parsed = nlohmann::json::parse(key);
        if (!parsed.contains("room_id") || !parsed.contains("session_id") ||
            !parsed.at("room_id").is_string() || !parsed.at("session_id").is_string()) {
            return false;
        }

        roomId    = parsed.at("room_id").get<std::string>();
        sessionId = parsed.at("session_id").get<std::string>();
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

bool
getMegolmSessionDataValue(Txn &txn,
                          Dbi &megolmSessionDataDb,
                          std::string_view roomId,
                          std::string_view sessionId,
                          std::string_view &value)
{
    return megolmSessionDataDb.get(txn, jsonSessionKey(roomId, sessionId), value);
}

void
putMegolmSessionDataValue(Txn &txn,
                          Dbi &megolmSessionDataDb,
                          std::string_view roomId,
                          std::string_view sessionId,
                          std::string_view value)
{
    megolmSessionDataDb.put(txn, jsonSessionKey(roomId, sessionId), value);
}

bool
getInboundMegolmSessionValue(Txn &txn,
                             Dbi &inboundMegolmSessionDb,
                             std::string_view roomId,
                             std::string_view sessionId,
                             std::string_view &value)
{
    return inboundMegolmSessionDb.get(txn, jsonSessionKey(roomId, sessionId), value);
}

void
putInboundMegolmSessionValue(Txn &txn,
                             Dbi &inboundMegolmSessionDb,
                             std::string_view roomId,
                             std::string_view sessionId,
                             std::string_view value)
{
    inboundMegolmSessionDb.put(txn, jsonSessionKey(roomId, sessionId), value);
}

} // namespace db
