// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

#include "db/Backend.h"

namespace db {

Dbi
openNamedDbi(Backend &backend, Txn &txn, std::string_view name, bool create = true);

} // namespace db
