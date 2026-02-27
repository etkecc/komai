// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include "db/Backend.h"

namespace db::maintenance {

using Database    = db::Database;
using Transaction = db::Transaction;

bool
supportsCompaction(const Database &database) noexcept;

bool
supportsCompaction(const Database *database) noexcept;

bool
supportsCompaction(std::unique_ptr<Database> &database) noexcept;

bool
supportsCompaction(const std::unique_ptr<Database> &database) noexcept;

void
compact(Database &from, Database &to);

void
compact(Database *from, Database *to);

}
