// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/OrderEntry.h"

#include <nlohmann/json.hpp>

namespace db {

OrderEntry
parseOrderEntry(std::string_view value)
{
    try {
        const auto parsed = nlohmann::json::parse(value);

        OrderEntry entry;
        entry.hasPrevBatch = parsed.count("prev_batch") != 0;
        if (parsed.contains("prev_batch") && parsed["prev_batch"].is_string())
            entry.prevBatch = parsed["prev_batch"].get<std::string>();
        if (parsed.contains("event_id") && parsed["event_id"].is_string()) {
            const auto eventId = parsed["event_id"].get<std::string>();
            if (!eventId.empty())
                entry.eventId = eventId;
        }
        return entry;
    } catch (std::exception &) {
        // Work around legacy cache entries that stored raw event ids instead of JSON.
        OrderEntry entry;
        if (!value.empty())
            entry.eventId = std::string(value);
        return entry;
    }
}

} // namespace db
