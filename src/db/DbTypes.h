// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace lmdb {
class env;
class txn;
class dbi;
}

namespace db {

using Env = lmdb::env;
using Txn = lmdb::txn;
using Dbi = lmdb::dbi;

} // namespace db
