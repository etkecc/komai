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
#include "cache/schema/CacheSchema.h"
#include "db/storage/Core.h"
#include "db/storage/Open.h"
#include "encryption/Olm.h"

MatrixStore::~MatrixStore() noexcept = default;

db::Database &
MatrixStore::storage()
{
    if (!db || !db->storage)
        throw std::runtime_error("Storage backend is not initialized");
    return *db->storage;
}

const db::Database &
MatrixStore::storage() const
{
    if (!db || !db->storage)
        throw std::runtime_error("Storage backend is not initialized");
    return *db->storage;
}

db::Transaction
MatrixStore::beginTxn(db::Transaction *parent, db::TransactionFlags flags)
{
    return db::beginTransaction(storage(),
                                parent,
                                flags == db::TransactionFlags::ReadOnly
                                  ? db::AccessMode::ReadOnly
                                  : db::AccessMode::ReadWrite);
}

bool
MatrixStore::isMapFullError(const std::exception &e) const noexcept
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
MatrixStore::getEventsDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(storage(), txn, room_id, cache::schema::RoomDb::Events);
}

db::Store
MatrixStore::getEventOrderDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(storage(), txn, room_id, cache::schema::RoomDb::EventOrder);
}

// inverse of EventOrderDb
db::Store
MatrixStore::getEventToOrderDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(
      storage(), txn, room_id, cache::schema::RoomDb::EventToOrder);
}

db::Store
MatrixStore::getMessageToOrderDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(
      storage(), txn, room_id, cache::schema::RoomDb::MessageToOrder);
}

db::Store
MatrixStore::getOrderToMessageDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(
      storage(), txn, room_id, cache::schema::RoomDb::OrderToMessage);
}

db::Store
MatrixStore::getPendingMessagesDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(storage(), txn, room_id, cache::schema::RoomDb::Pending);
}

db::Store
MatrixStore::getRelationsDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(storage(), txn, room_id, cache::schema::RoomDb::Related);
}

db::Store
MatrixStore::getInviteStatesDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(
      storage(), txn, room_id, cache::schema::RoomDb::InviteState);
}

db::Store
MatrixStore::getInviteMembersDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(
      storage(), txn, room_id, cache::schema::RoomDb::InviteMembers);
}

db::Store
MatrixStore::getStatesDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(storage(), txn, room_id, cache::schema::RoomDb::State);
}

db::Store
MatrixStore::getStatesKeyDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(storage(), txn, room_id, cache::schema::RoomDb::StatesKey);
}

db::Store
MatrixStore::getAccountDataDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(
      storage(), txn, room_id, cache::schema::RoomDb::AccountData);
}

db::Store
MatrixStore::getMembersDb(db::Transaction &txn, const std::string &room_id)
{
    return cache::schema::openRoomStore(storage(), txn, room_id, cache::schema::RoomDb::Members);
}

db::Store
MatrixStore::getUserKeysDb(db::Transaction &txn)
{
    return cache::schema::openGlobalStore(storage(), txn, cache::schema::GlobalDb::UserKeys);
}

db::Store
MatrixStore::getVerificationDb(db::Transaction &txn)
{
    return cache::schema::openGlobalStore(storage(), txn, cache::schema::GlobalDb::Verified);
}

QString
MatrixStore::getDisplayName(const mtx::events::StateEvent<mtx::events::state::Member> &event)
{
    if (!event.content.display_name.empty())
        return QString::fromStdString(event.content.display_name);

    return QString::fromStdString(event.state_key);
}
