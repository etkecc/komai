// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

#include "db/Backend.h"
#include "db/Catalog.h"

namespace db {

DbiOpenOptions
openOptionsForGlobal(catalog::GlobalDb db);

DbiOpenOptions
openOptionsForRoom(catalog::RoomDb db);

DbiOpenOptions
openOptionsForName(std::string_view dbName);

} // namespace db
