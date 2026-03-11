// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace db {

class Txn;
class Dbi;

using Transaction = Txn;
using Store       = Dbi;

std::string
readReceiptKey(std::string_view roomId, std::string_view userId);

bool
getReadReceiptValue(Transaction &txn,
                    Store &readReceiptDb,
                    std::string_view roomId,
                    std::string_view userId,
                    std::string_view &value);

void
putReadReceiptValue(Transaction &txn,
                    Store &readReceiptDb,
                    std::string_view roomId,
                    std::string_view userId,
                    std::string_view value);

std::size_t
forEachReadReceiptInRoom(
  Transaction &txn,
  Store &readReceiptDb,
  std::string_view roomId,
  const std::function<bool(std::string_view userId, std::string_view value)> &callback);

} // namespace db
