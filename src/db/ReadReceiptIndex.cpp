// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/ReadReceiptIndex.h"

#include <nlohmann/json.hpp>

#include "db/DbTypes.h"

namespace {

std::string
jsonReadReceiptKey(std::string_view eventId, std::string_view roomId)
{
    nlohmann::json key;
    key["event_id"] = eventId;
    key["room_id"]  = roomId;
    return key.dump();
}

} // namespace

namespace db {

std::string
readReceiptKey(std::string_view eventId, std::string_view roomId)
{
    return jsonReadReceiptKey(eventId, roomId);
}

bool
getReadReceiptValue(Txn &txn,
                    Store &readReceiptDb,
                    std::string_view eventId,
                    std::string_view roomId,
                    std::string_view &value)
{
    return readReceiptDb.get(txn, jsonReadReceiptKey(eventId, roomId), value);
}

void
putReadReceiptValue(Txn &txn,
                    Store &readReceiptDb,
                    std::string_view eventId,
                    std::string_view roomId,
                    std::string_view value)
{
    readReceiptDb.put(txn, jsonReadReceiptKey(eventId, roomId), value);
}

} // namespace db
