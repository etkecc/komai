// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/Json.h"
#include "db/Serde.h"
#include "db/storage/Core.h"

namespace db::storage {

using db::getJsonValue;
using db::parseJsonValue;
using db::putJsonValue;
using db::toSv;

} // namespace db::storage
