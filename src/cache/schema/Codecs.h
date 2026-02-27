// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Komai Contributors

#pragma once

#include "db/MemberInfo.h"
#include "db/RoomInfo.h"

namespace cache::codec {

using db::getMemberInfo;
using db::getRoomInfo;
using db::parseMemberInfo;
using db::parseRoomInfo;
using db::putMemberInfo;
using db::putRoomInfo;
using db::serializeMemberInfo;
using db::serializeRoomInfo;

} // namespace cache::codec
