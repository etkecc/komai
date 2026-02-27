// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <algorithm>
#include <stdexcept>

#include "EventAccessors.h"
#include "Logging.h"
#include "db/StorageApi.h"
#include "encryption/Olm.h"

Cache::~Cache() noexcept = default;

db::Database &
Cache::storage()
{
    if (!db || !db->storage)
        throw std::runtime_error("Storage backend is not initialized");
    return *db->storage;
}

const db::Database &
Cache::storage() const
{
    if (!db || !db->storage)
        throw std::runtime_error("Storage backend is not initialized");
    return *db->storage;
}

db::Transaction
Cache::beginTxn(db::Transaction *parent, db::TransactionFlags flags)
{
    return db::beginTransaction(storage(),
                                parent,
                                flags == db::TransactionFlags::ReadOnly
                                  ? db::AccessMode::ReadOnly
                                  : db::AccessMode::ReadWrite);
}

bool
Cache::isMapFullError(const std::exception &e) const noexcept
{
    const auto *storageError = dynamic_cast<const db::Error *>(&e);
    return storageError && storageError->kind() == db::ErrorKind::MapFull;
}

RO_txn
ro_txn(db::Database &storage)
{
    thread_local db::Transaction txn = db::beginReadTransaction(storage);
    thread_local int reuse_counter   = 0;

    if (reuse_counter >= 100 || !db::ownsTransaction(storage, txn)) {
        txn.abort();
        txn           = db::beginReadTransaction(storage);
        reuse_counter = 0;
    } else if (reuse_counter > 0) {
        try {
            txn.renew();
        } catch (...) {
            txn.abort();
            txn           = db::beginReadTransaction(storage);
            reuse_counter = 0;
        }
    }
    reuse_counter++;

    return RO_txn{txn};
}

db::Store
Cache::getEventsDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::Events);
}

db::Store
Cache::getEventOrderDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::EventOrder);
}

// inverse of EventOrderDb
db::Store
Cache::getEventToOrderDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::EventToOrder);
}

db::Store
Cache::getMessageToOrderDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::MessageToOrder);
}

db::Store
Cache::getOrderToMessageDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::OrderToMessage);
}

db::Store
Cache::getPendingMessagesDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::Pending);
}

db::Store
Cache::getRelationsDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::Related);
}

db::Store
Cache::getInviteStatesDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::InviteState);
}

db::Store
Cache::getInviteMembersDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::InviteMembers);
}

db::Store
Cache::getStatesDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::State);
}

db::Store
Cache::getStatesKeyDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::StatesKey);
}

db::Store
Cache::getAccountDataDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::AccountData);
}

db::Store
Cache::getMembersDb(db::Transaction &txn, const std::string &room_id)
{
    return db::openRoomStore(storage(), txn, room_id, db::catalog::RoomDb::Members);
}

db::Store
Cache::getUserKeysDb(db::Transaction &txn)
{
    return db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::UserKeys);
}

db::Store
Cache::getVerificationDb(db::Transaction &txn)
{
    return db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::Verified);
}

QString
Cache::getDisplayName(const mtx::events::StateEvent<mtx::events::state::Member> &event)
{
    if (!event.content.display_name.empty())
        return QString::fromStdString(event.content.display_name);

    return QString::fromStdString(event.state_key);
}
