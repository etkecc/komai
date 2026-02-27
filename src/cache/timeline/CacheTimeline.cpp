// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/core/Cache.h"
#include "cache/core/Cache_p.h"

#include <limits>
#include <optional>
#include <string_view>

#include "EventAccessors.h"
#include <nlohmann/json.hpp>

uint64_t
Cache::saveOldMessages(const std::string &room_id, const mtx::responses::Messages &res)
{
    auto txn         = beginTxn();
    auto eventsDb    = getEventsDb(txn, room_id);
    auto relationsDb = getRelationsDb(txn, room_id);

    auto orderDb     = getEventOrderDb(txn, room_id);
    auto evToOrderDb = getEventToOrderDb(txn, room_id);
    auto msg2orderDb = getMessageToOrderDb(txn, room_id);
    auto order2msgDb = getOrderToMessageDb(txn, room_id);

    uint64_t index = std::numeric_limits<uint64_t>::max() / 2;
    if (const auto firstOrder = db::firstOrderedIndex(txn, orderDb); firstOrder)
        index = *firstOrder;

    uint64_t msgIndex = std::numeric_limits<uint64_t>::max() / 2;
    if (const auto firstMessage = db::firstOrderedIndex(txn, order2msgDb); firstMessage)
        msgIndex = *firstMessage;

    if (res.chunk.empty()) {
        if (db::setOrderEntryPrevBatch(txn, orderDb, index, res.end)) {
            txn.commit();
        }
        return msgIndex;
    }

    std::string event_id_val;
    for (const auto &e : res.chunk) {
        if (std::holds_alternative<mtx::events::RedactionEvent<mtx::events::msg::Redaction>>(e))
            continue;

        auto event                = mtx::accessors::serialize_event(e);
        event_id_val              = event["event_id"].get<std::string>();
        std::string_view event_id = event_id_val;

        // This check protects against duplicates in the timeline. If the event_id is
        // already in the DB, we skip putting it (again) in ordered DBs, and only update the
        // event itself and its relations.
        std::string_view unused_read;
        if (!evToOrderDb.get(txn, event_id, unused_read)) {
            db::prependEventOrderEntry(
              txn, orderDb, evToOrderDb, index, event_id, db::serializeOrderEntry(event_id));

            // TODO(Nico): Allow blacklisting more event types in UI
            if (!isHiddenEvent(txn, e, room_id)) {
                db::prependMessageOrderEntry(txn, order2msgDb, msg2orderDb, msgIndex, event_id);
            }
        }
        eventsDb.put(txn, event_id, event.dump());

        auto relations = mtx::accessors::relations(e);
        std::vector<std::string_view> relationTargets;
        relationTargets.reserve(relations.relations.size());
        for (const auto &relation : relations.relations) {
            if (!relation.event_id.empty())
                relationTargets.emplace_back(relation.event_id);
        }
        db::putDupValueForKeys(txn, relationsDb, relationTargets, event_id);
    }

    if (!event_id_val.empty()) {
        db::putOrderEntry(txn, orderDb, index, event_id_val, res.end);
    } else if (!res.chunk.empty()) {
        // to not break pagination, even if all events are redactions we try to persist something in
        // the batch.

        event_id_val = mtx::accessors::event_id(res.chunk.back());

        auto event = mtx::accessors::serialize_event(res.chunk.back()).dump();
        eventsDb.put(txn, event_id_val, event);
        db::prependEventOrderEntry(txn,
                                   orderDb,
                                   evToOrderDb,
                                   index,
                                   event_id_val,
                                   db::serializeOrderEntry(event_id_val, res.end));
    }

    txn.commit();

    return msgIndex;
}
