// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

#include "db/Backend.h"
#include "db/Catalog.h"

namespace db {

Dbi
openNamedDbi(Backend &backend, Txn &txn, std::string_view name, bool create = true);

Dbi
openGlobalDbi(Backend &backend, Txn &txn, catalog::GlobalDb db, bool create = true);

Dbi
openRoomDbi(Backend &backend,
            Txn &txn,
            std::string_view roomId,
            catalog::RoomDb db,
            bool create = true);

} // namespace db
