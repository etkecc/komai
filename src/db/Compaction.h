// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/Backend.h"

namespace db {

void
compact(Database &from, Database &to);

} // namespace db
