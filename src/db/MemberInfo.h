// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

struct MemberInfo;

namespace db {

class Txn;
class Dbi;

std::string
serializeMemberInfo(const MemberInfo &info);

MemberInfo
parseMemberInfo(std::string_view value);

bool
getMemberInfo(Txn &txn, Dbi &membersDb, std::string_view userId, MemberInfo &info);

std::optional<MemberInfo>
getMemberInfo(Txn &txn, Dbi &membersDb, std::string_view userId);

void
putMemberInfo(Txn &txn, Dbi &membersDb, std::string_view userId, const MemberInfo &info);

} // namespace db
