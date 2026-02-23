// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

#include "db/Backend.h"
#include "db/Catalog.h"

namespace db {

StoreOpenOptions
openOptionsForGlobal(catalog::GlobalDb db);

StoreOpenOptions
openOptionsForRoom(catalog::RoomDb db);

StoreOpenOptions
openOptionsForName(std::string_view dbName);

} // namespace db
