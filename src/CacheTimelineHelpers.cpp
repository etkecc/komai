// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

#include "EventAccessors.h"
#include "Logging.h"

#include <limits>
#include <nlohmann/json.hpp>

template<typename RelationCollection>
std::vector<std::string_view>
relationTargetEventIds(const RelationCollection &relations)
{
    std::vector<std::string_view> targets;
    targets.reserve(relations.size());
    for (const auto &relation : relations) {
        if (!relation.event_id.empty())
            targets.emplace_back(relation.event_id);
    }
    return targets;
}

void
Cache::replaceEvent(const std::string &room_id,
                    const std::string &event_id,
                    const mtx::events::collections::TimelineEvents &event)
{
    auto txn         = beginTxn();
    auto eventsDb    = getEventsDb(txn, room_id);
    auto relationsDb = getRelationsDb(txn, room_id);
    auto event_json  = mtx::accessors::serialize_event(event).dump();

    {
        eventsDb.del(txn, event_id);
        eventsDb.put(txn, event_id, event_json);
        const auto relationTargets =
          relationTargetEventIds(mtx::accessors::relations(event).relations);
        db::putDupValueForKeys(txn, relationsDb, relationTargets, event_id);
    }

    txn.commit();
}

void
Cache::saveTimelineMessages(db::Transaction &txn,
                            db::Store &eventsDb,
                            const std::string &room_id,
                            const mtx::responses::Timeline &res)
{
    if (res.events.empty())
        return;

    auto relationsDb = getRelationsDb(txn, room_id);

    auto orderDb     = getEventOrderDb(txn, room_id);
    auto evToOrderDb = getEventToOrderDb(txn, room_id);
    auto msg2orderDb = getMessageToOrderDb(txn, room_id);
    auto order2msgDb = getOrderToMessageDb(txn, room_id);
    auto pending     = getPendingMessagesDb(txn, room_id);

    if (res.limited) {
        orderDb.drop(txn, false);
        evToOrderDb.drop(txn, false);
        msg2orderDb.drop(txn, false);
        order2msgDb.drop(txn, false);
        pending.drop(txn, true);
    }

    using namespace mtx::events;
    using namespace mtx::events::state;

    uint64_t index = std::numeric_limits<uint64_t>::max() / 2;
    if (const auto lastOrder = db::lastOrderedIndex(txn, orderDb); lastOrder)
        index = *lastOrder;

    uint64_t msgIndex = std::numeric_limits<uint64_t>::max() / 2;
    if (const auto lastMessage = db::lastOrderedIndex(txn, order2msgDb); lastMessage)
        msgIndex = *lastMessage;

    bool first = true;
    for (const auto &e : res.events) {
        auto event  = mtx::accessors::serialize_event(e);
        auto txn_id = mtx::accessors::transaction_id(e);

        std::string event_id_val = event.value("event_id", "");
        if (event_id_val.empty()) {
            nhlog::db()->error("Event without id!");
            continue;
        }

        std::string_view event_id = event_id_val;

        const auto orderEntry = db::serializeOrderEntry(
          event_id_val,
          first && !res.prev_batch.empty() ? std::optional<std::string_view>(res.prev_batch)
                                           : std::nullopt);
        const auto eventJson = event.dump();

        if (!txn_id.empty() && db::replaceTimelineEventId(txn,
                                                          eventsDb,
                                                          orderDb,
                                                          evToOrderDb,
                                                          msg2orderDb,
                                                          order2msgDb,
                                                          txn_id,
                                                          event_id,
                                                          eventJson,
                                                          orderEntry)) {
            auto relations             = mtx::accessors::relations(e);
            const auto relationTargets = relationTargetEventIds(relations.relations);
            db::replaceDupValueForKeys(txn, relationsDb, relationTargets, txn_id, event_id);

            db::removePendingEntriesByTxnId(txn, pending, txn_id);
        } else if (auto redaction =
                     std::get_if<mtx::events::RedactionEvent<mtx::events::msg::Redaction>>(&e)) {
            if (redaction->redacts.empty())
                continue;

            // persist the first redaction in case this is a limited timeline and it is the first
            // event to not break pagination.
            if (first && res.limited) {
                first = false;

                nhlog::db()->debug("saving redaction '{}'", orderEntry);

                db::appendEventOrderEntry(txn, orderDb, evToOrderDb, index, event_id, orderEntry);
                eventsDb.put(txn, event_id, event.dump());
            }

            std::string_view oldEvent;
            bool success = eventsDb.get(txn, redaction->redacts, oldEvent);
            if (!success)
                continue;

            try {
                auto te = nlohmann::json::parse(std::string_view(oldEvent.data(), oldEvent.size()))
                            .get<mtx::events::collections::TimelineEvents>();

                // overwrite the content and add redation data
                std::visit(
                  [&redaction, &room_id, &txn, &eventsDb, this](auto &ev) {
                      ev.unsigned_data.redacted_because = *redaction;
                      ev.unsigned_data.redacted_by      = redaction->event_id;

                      if constexpr (isStateEvent_<decltype(ev)>) {
                          auto statesdb    = getStatesDb(txn, room_id);
                          auto stateskeydb = getStatesKeyDb(txn, room_id);
                          auto membersdb   = getMembersDb(txn, room_id);
                          mtx::events::StateEvent<mtx::events::msg::Redacted> redactedEvent;
                          redactedEvent.event_id  = ev.event_id;
                          redactedEvent.state_key = ev.state_key;
                          redactedEvent.type      = ev.type;
                          nhlog::db()->critical("Redacting: {}",
                                                nlohmann::json(redactedEvent).dump(2));

                          saveStateEvent(txn,
                                         statesdb,
                                         stateskeydb,
                                         membersdb,
                                         eventsDb,
                                         room_id,
                                         mtx::events::collections::StateEvents{redactedEvent});
                      }
                  },
                  te);
                event = mtx::accessors::serialize_event(te);
                event["content"].clear();

            } catch (std::exception &e) {
                nhlog::db()->error("Failed to parse message from cache {}", e.what());
                continue;
            }

            eventsDb.put(txn, redaction->redacts, event.dump());
            eventsDb.put(txn, redaction->event_id, nlohmann::json(*redaction).dump());
        } else {
            // This check protects against duplicates in the timeline. If the event_id
            // is already in the DB, we skip putting it (again) in ordered DBs, and only
            // update the event itself and its relations.
            std::string_view unused_read;
            if (!evToOrderDb.get(txn, event_id, unused_read)) {
                first = false;

                nhlog::db()->debug("saving '{}'", orderEntry);

                db::appendEventOrderEntry(txn, orderDb, evToOrderDb, index, event_id, orderEntry);

                // TODO(Nico): Allow blacklisting more event types in UI
                if (!isHiddenEvent(txn, e, room_id)) {
                    db::appendMessageOrderEntry(txn, order2msgDb, msg2orderDb, msgIndex, event_id);
                }
            } else {
                nhlog::db()->warn("duplicate event '{}'", orderEntry);
            }
            eventsDb.put(txn, event_id, eventJson);

            auto relations             = mtx::accessors::relations(e);
            const auto relationTargets = relationTargetEventIds(relations.relations);
            db::putDupValueForKeys(txn, relationsDb, relationTargets, event_id);
        }
    }
}
