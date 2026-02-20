// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

#include "db/Backend.h"

namespace db {

DbiOpenOptions
openOptionsForName(std::string_view dbName);

} // namespace db
