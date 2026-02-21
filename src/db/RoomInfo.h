// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

struct RoomInfo;

namespace db {

class Txn;
class Dbi;

std::string
serializeRoomInfo(const RoomInfo &info);

RoomInfo
parseRoomInfo(std::string_view value);

bool
getRoomInfo(Txn &txn, Dbi &roomInfoDb, std::string_view roomId, RoomInfo &info);

std::optional<RoomInfo>
getRoomInfo(Txn &txn, Dbi &roomInfoDb, std::string_view roomId);

void
putRoomInfo(Txn &txn, Dbi &roomInfoDb, std::string_view roomId, const RoomInfo &info);

} // namespace db
