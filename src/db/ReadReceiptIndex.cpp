// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/ReadReceiptIndex.h"

#include "db/DbTypes.h"
#include "db/Scan.h"

namespace {

std::string
compositeReadReceiptKey(std::string_view roomId, std::string_view userId)
{
    std::string combined(roomId.size() + 1 + userId.size(), '\0');
    combined.replace(0, roomId.size(), roomId);
    combined.replace(roomId.size() + 1, userId.size(), userId);
    return combined;
}

std::pair<std::string_view, std::string_view>
splitReadReceiptKey(std::string_view key) noexcept
{
    const auto separator = key.find('\0');
    return std::pair(key.substr(0, separator), key.substr(separator + 1));
}

} // namespace

namespace db {

std::string
readReceiptKey(std::string_view roomId, std::string_view userId)
{
    return compositeReadReceiptKey(roomId, userId);
}

bool
getReadReceiptValue(Txn &txn,
                    Store &readReceiptDb,
                    std::string_view roomId,
                    std::string_view userId,
                    std::string_view &value)
{
    return readReceiptDb.get(txn, compositeReadReceiptKey(roomId, userId), value);
}

void
putReadReceiptValue(Txn &txn,
                    Store &readReceiptDb,
                    std::string_view roomId,
                    std::string_view userId,
                    std::string_view value)
{
    readReceiptDb.put(txn, compositeReadReceiptKey(roomId, userId), value);
}

std::size_t
forEachReadReceiptInRoom(
  Txn &txn,
  Store &readReceiptDb,
  std::string_view roomId,
  const std::function<bool(std::string_view userId, std::string_view value)> &callback)
{
    std::size_t count = 0;
    const auto prefix = compositeReadReceiptKey(roomId, "");
    forEachEntryWithPrefix(txn,
                           readReceiptDb,
                           prefix,
                           [&callback, &count](std::string_view key, std::string_view value) {
                               ++count;
                               return callback(splitReadReceiptKey(key).second, value);
                           });
    return count;
}

} // namespace db
