// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Open.h"

#include <string>

#include "db/DbTypes.h"
#include "db/NamePolicy.h"

namespace db {

Dbi
openNamedDbi(Backend &backend, Txn &txn, std::string_view name, bool create)
{
    auto options = openOptionsForName(name);
    if (create)
        options.flags |= DbiFlags::Create;

    const std::string dbName{name};
    return backend.openDbi(txn, dbName.c_str(), options);
}

} // namespace db
