// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/Backend.h"

namespace db {

void
compact(Backend &from, Backend &to);

} // namespace db
