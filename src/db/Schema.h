// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

#include "db/Backend.h"

namespace db {

bool
tryDropNamedStore(Database &database,
                  Transaction &txn,
                  std::string_view dbName,
                  std::string *error = nullptr) noexcept;

} // namespace db
