// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/StorageApi.h"

namespace db {

void
compact(storage::Database &from, storage::Database &to);

} // namespace db
