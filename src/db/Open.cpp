// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Open.h"

#include "db/DbTypes.h"
#include "db/NamePolicy.h"

namespace db {

Dbi
openNamedDbi(Backend &backend, Txn &txn, std::string_view name, bool create)
{
    auto options = openOptionsForName(name);
    if (create)
        options.flags |= DbiFlags::Create;

    return backend.openDbi(txn, name, options);
}

Dbi
openGlobalDbi(Backend &backend, Txn &txn, catalog::GlobalDb db, bool create)
{
    auto options = openOptionsForGlobal(db);
    if (create)
        options.flags |= DbiFlags::Create;

    return backend.openDbi(txn, catalog::globalName(db), options);
}

Dbi
openRoomDbi(Backend &backend, Txn &txn, std::string_view roomId, catalog::RoomDb db, bool create)
{
    auto options = openOptionsForRoom(db);
    if (create)
        options.flags |= DbiFlags::Create;

    return backend.openDbi(txn, catalog::roomName(roomId, db), options);
}

} // namespace db
