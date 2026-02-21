// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace db {

struct OrderEntry
{
    std::optional<std::string> eventId;
    std::optional<std::string> prevBatch;
    bool hasPrevBatch = false;
};

OrderEntry
parseOrderEntry(std::string_view value);

} // namespace db
