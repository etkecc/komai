// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/DupIndex.h"
#include "db/Scan.h"
#include "db/storage/Core.h"

namespace db::storage {

using db::eraseEntriesIf;
using db::firstEntry;
using db::forEachDupValue;
using db::forEachEntry;
using db::forEachEntryFromKey;
using db::forEachEntryWithPrefix;
using db::forEachUniqueKey;
using db::lastEntry;
using db::listDupValues;
using db::listEntries;
using db::listKeys;
using db::listUniqueKeys;
using db::putDupValueForKeys;
using db::replaceDupValueForKeys;
using db::ScanDirection;

} // namespace db::storage

namespace db {

using storage::ScanDirection;

} // namespace db
