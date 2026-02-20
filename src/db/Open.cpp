// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Open.h"

#include <string>

#include "db/DbTypes.h"
#include "db/NamePolicy.h"

namespace db {

Dbi
openNamedDbi(Backend &backend, Txn &txn, std::string_view name, bool create)
{
    auto options = openOptionsForName(name);
    if (create)
        options.flags |= DbiFlags::Create;

    const std::string dbName{name};
    return backend.openDbi(txn, dbName.c_str(), options);
}

Dbi
openGlobalDbi(Backend &backend, Txn &txn, catalog::GlobalDb db, bool create)
{
    auto options = openOptionsForGlobal(db);
    if (create)
        options.flags |= DbiFlags::Create;

    const std::string dbName{catalog::globalName(db)};
    return backend.openDbi(txn, dbName.c_str(), options);
}

Dbi
openRoomDbi(Backend &backend, Txn &txn, std::string_view roomId, catalog::RoomDb db, bool create)
{
    auto options = openOptionsForRoom(db);
    if (create)
        options.flags |= DbiFlags::Create;

    const std::string dbName = catalog::roomName(roomId, db);
    return backend.openDbi(txn, dbName.c_str(), options);
}

} // namespace db
