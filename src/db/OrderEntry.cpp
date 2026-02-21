// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/OrderEntry.h"

#include <nlohmann/json.hpp>

#include "db/Json.h"

namespace db {

OrderEntry
parseOrderEntry(std::string_view value)
{
    nlohmann::json parsed;
    if (!db::parseJsonValue(value, parsed))
        // Work around legacy cache entries that stored raw event ids instead of JSON.
        return OrderEntry{.eventId      = value.empty() ? std::nullopt
                                                        : std::optional<std::string>(std::string(value)),
                          .prevBatch    = std::nullopt,
                          .hasPrevBatch = false};

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
}

std::string
serializeOrderEntry(const OrderEntry &entry)
{
    auto serialized = nlohmann::json::object();
    if (entry.eventId)
        serialized["event_id"] = *entry.eventId;

    if (entry.hasPrevBatch) {
        if (entry.prevBatch)
            serialized["prev_batch"] = *entry.prevBatch;
        else
            serialized["prev_batch"] = nullptr;
    } else if (entry.prevBatch) {
        serialized["prev_batch"] = *entry.prevBatch;
    }

    return serialized.dump();
}

std::string
serializeOrderEntry(std::string_view eventId, std::optional<std::string_view> prevBatch)
{
    OrderEntry entry;
    if (!eventId.empty())
        entry.eventId = std::string(eventId);

    if (prevBatch.has_value()) {
        entry.hasPrevBatch = true;
        entry.prevBatch    = std::string(*prevBatch);
    }

    return serializeOrderEntry(entry);
}

} // namespace db
