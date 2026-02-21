// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/NamePolicy.h"
#include "db/Catalog.h"

namespace db {

StoreOpenOptions
openOptionsForGlobal(catalog::GlobalDb db)
{
    StoreOpenOptions options{};

    if (db == catalog::GlobalDb::SpacesChildren || db == catalog::GlobalDb::SpacesParents)
        options.flags |= StoreFlags::DupSort;

    return options;
}

StoreOpenOptions
openOptionsForRoom(catalog::RoomDb db)
{
    StoreOpenOptions options{};

    if (db == catalog::RoomDb::EventOrder || db == catalog::RoomDb::OrderToMessage ||
        db == catalog::RoomDb::Pending)
        options.flags |= StoreFlags::IntegerKey;

    if (db == catalog::RoomDb::Related) {
        options.flags |= StoreFlags::DupSort;
    } else if (db == catalog::RoomDb::StatesKey) {
        options.flags |= StoreFlags::DupSort;
        options.dupsortComparator = DupsortComparator::StateKey;
    } else if (db == catalog::RoomDb::LegacyStateByKey) {
        options.flags |= StoreFlags::DupSort;
        options.dupsortComparator = DupsortComparator::LegacyStateByKeyJson;
    }

    return options;
}

StoreOpenOptions
openOptionsForName(std::string_view dbName)
{
    StoreOpenOptions options{};

    if (catalog::hasRoomSuffix(dbName, catalog::RoomDb::EventOrder))
        options = openOptionsForRoom(catalog::RoomDb::EventOrder);
    else if (catalog::hasRoomSuffix(dbName, catalog::RoomDb::OrderToMessage))
        options = openOptionsForRoom(catalog::RoomDb::OrderToMessage);
    else if (catalog::hasRoomSuffix(dbName, catalog::RoomDb::Pending))
        options = openOptionsForRoom(catalog::RoomDb::Pending);
    else if (catalog::hasRoomSuffix(dbName, catalog::RoomDb::Related))
        options = openOptionsForRoom(catalog::RoomDb::Related);
    else if (catalog::hasRoomSuffix(dbName, catalog::RoomDb::StatesKey))
        options = openOptionsForRoom(catalog::RoomDb::StatesKey);
    else if (catalog::hasRoomSuffix(dbName, catalog::RoomDb::LegacyStateByKey))
        options = openOptionsForRoom(catalog::RoomDb::LegacyStateByKey);
    else if (dbName == catalog::globalName(catalog::GlobalDb::SpacesChildren))
        options = openOptionsForGlobal(catalog::GlobalDb::SpacesChildren);
    else if (dbName == catalog::globalName(catalog::GlobalDb::SpacesParents))
        options = openOptionsForGlobal(catalog::GlobalDb::SpacesParents);

    return options;
}

} // namespace db
