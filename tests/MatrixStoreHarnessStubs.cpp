// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/core/Cache_p.h"

std::optional<mtx::events::collections::RoomAccountDataEvents>
MatrixStore::getAccountData(db::Transaction &, mtx::events::EventType, const std::string &)
{
    return std::nullopt;
}

QString
MatrixStore::getRoomName(db::Transaction &, db::Store &, db::Store &)
{
    return {};
}

QString
MatrixStore::getRoomTopic(db::Transaction &, db::Store &)
{
    return {};
}

QString
MatrixStore::getRoomAvatarUrl(db::Transaction &, db::Store &, db::Store &)
{
    return {};
}

QString
MatrixStore::getRoomVersion(db::Transaction &, db::Store &)
{
    return {};
}

bool
MatrixStore::getRoomIsSpace(db::Transaction &, db::Store &)
{
    return false;
}

bool
MatrixStore::getRoomIsTombstoned(db::Transaction &, db::Store &)
{
    return false;
}

void
MatrixStore::setEncryptedRoom(db::Transaction &, const std::string &)
{
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
