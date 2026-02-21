// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

namespace db {

class Txn;
class Dbi;

using Transaction = Txn;
using Store = Dbi;

std::string
readReceiptKey(std::string_view eventId, std::string_view roomId);

bool
getReadReceiptValue(Transaction &txn,
                    Store &readReceiptDb,
                    std::string_view eventId,
                    std::string_view roomId,
                    std::string_view &value);

void
putReadReceiptValue(Transaction &txn,
                    Store &readReceiptDb,
                    std::string_view eventId,
                    std::string_view roomId,
                    std::string_view value);

} // namespace db
