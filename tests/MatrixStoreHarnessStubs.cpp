// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/core/Cache_p.h"

#include "db/Scan.h"

std::optional<mtx::events::collections::RoomAccountDataEvents>
MatrixStore::getAccountData(db::Transaction &, mtx::events::EventType, const std::string &)
{
    return std::nullopt;
}

std::optional<std::string>
MatrixStore::getAccountDataByType(db::Transaction &, const std::string &, const std::string &)
{
    return std::nullopt;
}

VerificationStatus
MatrixStore::verificationStatus(const std::string &)
{
    return {};
}

void
MatrixStore::updateUserKeys(const std::string &, const mtx::responses::QueryKeys &)
{
}

std::optional<UserKeyCache>
MatrixStore::userKeys(const std::string &)
{
    return std::nullopt;
}

std::vector<std::string>
MatrixStore::joinedRooms()
{
    return {};
}

void
MatrixStore::markUserKeysOutOfDate(db::Transaction &,
                                   db::Store &,
                                   const std::vector<std::string> &,
                                   const std::string &)
{
}

std::vector<std::string>
MatrixStore::getRoomIds(db::Transaction &txn)
{
    std::vector<std::string> rooms;
    db::forEachUniqueKey(txn, db->rooms, [&rooms](std::string_view roomId) {
        rooms.emplace_back(roomId);
        return true;
    });
    return rooms;
}
