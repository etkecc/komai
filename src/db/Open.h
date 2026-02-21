// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

#include "db/Backend.h"
#include "db/Catalog.h"

namespace db {

using Database     = Backend;
using DatabaseTxn  = Transaction;
using NamedStore   = Store;
using RoomStore    = Store;

NamedStore
openNamedStore(Database &backend, DatabaseTxn &txn, std::string_view name, bool create = true);

NamedStore
openGlobalStore(Database &backend, DatabaseTxn &txn, catalog::GlobalDb db, bool create = true);

RoomStore
openRoomStore(Database &backend, DatabaseTxn &txn, std::string_view roomId, catalog::RoomDb db, bool create = true);

} // namespace db
