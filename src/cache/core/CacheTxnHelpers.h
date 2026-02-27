// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/StorageApi.h"

struct RO_txn
{
    ~RO_txn() { txn.reset(); }
    operator db::Transaction &() noexcept { return txn; }

    db::Transaction &txn;
};

RO_txn
ro_txn(db::Database &storage);
