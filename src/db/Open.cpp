// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Open.h"

#include "db/DbTypes.h"
#include "db/NamePolicy.h"

namespace db {

NamedStore
openNamedStore(Database &backend, DatabaseTxn &txn, std::string_view name, bool create)
{
    auto options = openOptionsForName(name);
    if (create)
        options.flags |= DbiFlags::Create;

    return backend.openDbi(txn, name, options);
}

NamedStore
openNamedDbi(Database &backend, DatabaseTxn &txn, std::string_view name, bool create)
{
    return openNamedStore(backend, txn, name, create);
}

NamedStore
openGlobalStore(Database &backend, DatabaseTxn &txn, catalog::GlobalDb db, bool create)
{
    auto options = openOptionsForGlobal(db);
    if (create)
        options.flags |= DbiFlags::Create;

    return backend.openDbi(txn, catalog::globalName(db), options);
}

NamedStore
openGlobalDbi(Database &backend, DatabaseTxn &txn, catalog::GlobalDb db, bool create)
{
    return openGlobalStore(backend, txn, db, create);
}

NamedStore
openRoomStore(Database &backend, DatabaseTxn &txn, std::string_view roomId, catalog::RoomDb db, bool create)
{
    auto options = openOptionsForRoom(db);
    if (create)
        options.flags |= DbiFlags::Create;

    return backend.openDbi(txn, catalog::roomName(roomId, db), options);
}

RoomStore
openRoomDbi(Database &backend, DatabaseTxn &txn, std::string_view roomId, catalog::RoomDb db, bool create)
{
    return openRoomStore(backend, txn, roomId, db, create);
}

} // namespace db
