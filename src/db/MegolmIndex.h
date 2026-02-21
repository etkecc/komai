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
using Store       = Dbi;

std::string
megolmSessionKey(std::string_view roomId, std::string_view sessionId);

bool
parseMegolmSessionKey(std::string_view key, std::string &roomId, std::string &sessionId) noexcept;

bool
getMegolmSessionDataValue(Transaction &txn,
                          Store &megolmSessionDataDb,
                          std::string_view roomId,
                          std::string_view sessionId,
                          std::string_view &value);

void
putMegolmSessionDataValue(Transaction &txn,
                          Store &megolmSessionDataDb,
                          std::string_view roomId,
                          std::string_view sessionId,
                          std::string_view value);

bool
getInboundMegolmSessionValue(Transaction &txn,
                             Store &inboundMegolmSessionDb,
                             std::string_view roomId,
                             std::string_view sessionId,
                             std::string_view &value);

void
putInboundMegolmSessionValue(Transaction &txn,
                             Store &inboundMegolmSessionDb,
                             std::string_view roomId,
                             std::string_view sessionId,
                             std::string_view value);

} // namespace db
