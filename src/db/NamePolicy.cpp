// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/NamePolicy.h"
#include "db/Catalog.h"

namespace db {

DbiOpenOptions
openOptionsForName(std::string_view dbName)
{
    DbiOpenOptions options{};

    if (catalog::hasRoomSuffix(dbName, catalog::RoomDb::EventOrder) ||
        catalog::hasRoomSuffix(dbName, catalog::RoomDb::OrderToMessage) ||
        catalog::hasRoomSuffix(dbName, catalog::RoomDb::Pending))
        options.flags |= DbiFlags::IntegerKey;

    if (catalog::hasRoomSuffix(dbName, catalog::RoomDb::Related) ||
        dbName == catalog::globalName(catalog::GlobalDb::SpacesChildren) ||
        dbName == catalog::globalName(catalog::GlobalDb::SpacesParents))
        options.flags |= DbiFlags::DupSort;

    if (catalog::hasRoomSuffix(dbName, catalog::RoomDb::StatesKey)) {
        options.flags |= DbiFlags::DupSort;
        options.dupsortComparator = DupsortComparator::StateKey;
    } else if (catalog::hasRoomSuffix(dbName, catalog::RoomDb::LegacyStateByKey)) {
        options.flags |= DbiFlags::DupSort;
        options.dupsortComparator = DupsortComparator::LegacyStateByKeyJson;
    }

    return options;
}

} // namespace db
