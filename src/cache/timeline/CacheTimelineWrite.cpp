// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include "events/EventAccessors.h"
#include <spdlog/logger.h>

#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <unordered_set>

#include "cache/api/CacheApiContext.h"
#include "cache/schema/RoomStore.h"
#include "cache/schema/RoomTimelineIndex.h"
#include "db/Catalog.h"
#include "db/Scan.h"

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

std::unordered_set<std::string>
currentStateEventIds(db::Transaction &txn,
                     const std::string &roomId,
                     db::Store &statesDb,
                     db::Store &statesKeyDb)
{
    std::unordered_set<std::string> eventIds;

    cache::room_store::forEachEntry(
      txn,
      statesDb,
      cache::schema::RoomDb::State,
      roomId,
      [&eventIds](std::string_view /*eventType*/, std::string_view stateEventJson) {
          try {
              const auto event = nlohmann::json::parse(stateEventJson);
              if (const auto eventId = event.value("event_id", std::string{}); !eventId.empty())
                  eventIds.insert(eventId);
          } catch (const std::exception &e) {
              cache::activeLoggers().db->warn(
                "Failed to parse current state event during limited-sync cleanup: {}", e.what());
          }

          return true;
      });

    cache::room_store::forEachEntry(
      txn,
      statesKeyDb,
      cache::schema::RoomDb::StatesKey,
      roomId,
      [&eventIds](std::string_view /*eventType*/, std::string_view indexValue) {
          const auto eventId = db::catalog::splitStateEventIndexValue(indexValue).second;
          if (!eventId.empty())
              eventIds.emplace(eventId);
          return true;
      });

    return eventIds;
}

void
cleanupLimitedTimeline(db::Transaction &txn,
                       db::Store &eventsDb,
                       db::Store &relationsDb,
                       db::Store &eventOrderDb,
                       db::Store &eventToOrderDb,
                       db::Store &messageToOrderDb,
                       db::Store &orderToMessageDb,
                       db::Store &pendingDb,
                       db::Store &statesDb,
                       db::Store &statesKeyDb,
                       const std::string &roomId)
{
    const auto protectedEventIds = currentStateEventIds(txn, roomId, statesDb, statesKeyDb);
    const auto staleTimelineEventIds =
      room_timeline::listOrderEntryEventIds(txn, eventOrderDb, roomId);

    room_store::eraseEntries(txn, eventOrderDb, cache::schema::RoomDb::EventOrder, roomId);
    room_store::eraseEntries(txn, eventToOrderDb, cache::schema::RoomDb::EventToOrder, roomId);
    room_store::eraseEntries(txn, messageToOrderDb, cache::schema::RoomDb::MessageToOrder, roomId);
    room_store::eraseEntries(txn, orderToMessageDb, cache::schema::RoomDb::OrderToMessage, roomId);
    room_store::eraseEntries(txn, relationsDb, cache::schema::RoomDb::Related, roomId);
    room_store::eraseEntries(txn, pendingDb, cache::schema::RoomDb::Pending, roomId);

    for (const auto &eventId : staleTimelineEventIds) {
        if (!protectedEventIds.contains(eventId))
            room_store::del(txn, eventsDb, cache::schema::RoomDb::Events, roomId, eventId);
    }
}

void
MatrixStore::replaceEvent(const std::string &room_id,
                          const std::string &event_id,
                          const mtx::events::collections::TimelineEvents &event)
{
    auto txn         = beginTxn();
    auto eventsDb    = getEventsDb(txn, room_id);
    auto relationsDb = getRelationsDb(txn, room_id);
    auto event_json  = mtx::accessors::serialize_event(event).dump();

    {
        room_store::del(txn, eventsDb, cache::schema::RoomDb::Events, room_id, event_id);
        room_store::put(
          txn, eventsDb, cache::schema::RoomDb::Events, room_id, event_id, event_json);
        const auto relationTargets =
          relationTargetEventIds(mtx::accessors::relations(event).relations);
        room_timeline::rewriteRelationSourceReferences(
          txn, relationsDb, room_id, event_id, relationTargets);
    }

    txn.commit();
}

void
MatrixStore::saveTimelineMessages(db::Transaction &txn,
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
        auto statesDb    = getStatesDb(txn, room_id);
        auto statesKeyDb = getStatesKeyDb(txn, room_id);
        cleanupLimitedTimeline(txn,
                               eventsDb,
                               relationsDb,
                               orderDb,
                               evToOrderDb,
                               msg2orderDb,
                               order2msgDb,
                               pending,
                               statesDb,
                               statesKeyDb,
                               room_id);
    }

    using namespace mtx::events;
    using namespace mtx::events::state;

    uint64_t index = std::numeric_limits<uint64_t>::max() / 2;
    if (const auto lastOrder =
          room_timeline::lastOrderedIndex(txn, orderDb, cache::schema::RoomDb::EventOrder, room_id);
        lastOrder)
        index = *lastOrder;

    uint64_t msgIndex = std::numeric_limits<uint64_t>::max() / 2;
    if (const auto lastMessage = room_timeline::lastOrderedIndex(
          txn, order2msgDb, cache::schema::RoomDb::OrderToMessage, room_id);
        lastMessage)
        msgIndex = *lastMessage;

    bool first = true;
    for (const auto &e : res.events) {
        auto event  = mtx::accessors::serialize_event(e);
        auto txn_id = mtx::accessors::transaction_id(e);

        std::string event_id_val = event.value("event_id", "");
        if (event_id_val.empty()) {
            cache::activeLoggers().db->error("Event without id!");
            continue;
        }

        std::string_view event_id = event_id_val;

        const auto orderEntry = db::serializeOrderEntry(
          event_id_val,
          first && !res.prev_batch.empty() ? std::optional<std::string_view>(res.prev_batch)
                                           : std::nullopt);
        const auto eventJson       = event.dump();
        const auto relationTargets = relationTargetEventIds(mtx::accessors::relations(e).relations);

        if (!txn_id.empty() && room_timeline::replaceTimelineEventId(txn,
                                                                     eventsDb,
                                                                     orderDb,
                                                                     evToOrderDb,
                                                                     msg2orderDb,
                                                                     order2msgDb,
                                                                     room_id,
                                                                     txn_id,
                                                                     event_id,
                                                                     eventJson,
                                                                     orderEntry)) {
            room_timeline::removeRelationSourceReferences(txn, relationsDb, room_id, txn_id);
            room_timeline::rewriteRelationSourceReferences(
              txn, relationsDb, room_id, event_id, relationTargets);

            room_timeline::removePendingEntriesByTxnId(txn, pending, room_id, txn_id);
        } else if (auto redaction =
                     std::get_if<mtx::events::RedactionEvent<mtx::events::msg::Redaction>>(&e)) {
            if (redaction->redacts.empty())
                continue;

            // persist the first redaction in case this is a limited timeline and it is the first
            // event to not break pagination.
            if (first && res.limited) {
                first = false;

                cache::activeLoggers().db->debug("saving redaction '{}'", orderEntry);

                room_timeline::appendEventOrderEntry(
                  txn, orderDb, evToOrderDb, room_id, index, event_id, orderEntry);
                room_store::put(
                  txn, eventsDb, cache::schema::RoomDb::Events, room_id, event_id, event.dump());
            }

            std::string_view oldEvent;
            bool success = room_store::get(
              txn, eventsDb, cache::schema::RoomDb::Events, room_id, redaction->redacts, oldEvent);
            if (!success)
                continue;

            try {
                auto te = nlohmann::json::parse(std::string_view(oldEvent.data(), oldEvent.size()))
                            .get<mtx::events::collections::TimelineEvents>();
                std::set<std::string> redactedSpacesWithUpdates;
                std::set<std::string> redactedRoomsWithUpdates;

                // overwrite the content and add redation data
                std::visit(
                  [&redaction,
                   &room_id,
                   &txn,
                   &eventsDb,
                   &redactedSpacesWithUpdates,
                   &redactedRoomsWithUpdates,
                   this](auto &ev) {
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
                          cache::activeLoggers().db->critical(
                            "Redacting: {}", nlohmann::json(redactedEvent).dump(2));

                          saveStateEvent(txn,
                                         statesdb,
                                         stateskeydb,
                                         membersdb,
                                         eventsDb,
                                         room_id,
                                         mtx::events::collections::StateEvents{redactedEvent});

                          if (ev.type == mtx::events::EventType::SpaceChild)
                              redactedSpacesWithUpdates.insert(room_id);
                          else if (ev.type == mtx::events::EventType::SpaceParent)
                              redactedRoomsWithUpdates.insert(room_id);
                      }
                  },
                  te);
                if (!redactedSpacesWithUpdates.empty() || !redactedRoomsWithUpdates.empty()) {
                    updateSpaces(
                      txn, redactedSpacesWithUpdates, std::move(redactedRoomsWithUpdates));
                }
                event = mtx::accessors::serialize_event(te);
                event["content"].clear();

            } catch (std::exception &e) {
                cache::activeLoggers().db->error("Failed to parse message from cache {}", e.what());
                continue;
            }

            room_store::put(txn,
                            eventsDb,
                            cache::schema::RoomDb::Events,
                            room_id,
                            redaction->redacts,
                            event.dump());
            room_store::put(txn,
                            eventsDb,
                            cache::schema::RoomDb::Events,
                            room_id,
                            redaction->event_id,
                            nlohmann::json(*redaction).dump());
        } else {
            // This check protects against duplicates in the timeline. If the event_id
            // is already in the DB, we skip putting it (again) in ordered DBs, and only
            // update the event itself and its relations.
            std::string_view unused_read;
            const bool hasExistingEvent = room_store::get(txn,
                                                          evToOrderDb,
                                                          cache::schema::RoomDb::EventToOrder,
                                                          room_id,
                                                          event_id,
                                                          unused_read);
            if (!hasExistingEvent) {
                first = false;
                cache::activeLoggers().db->debug("saving '{}'", orderEntry);

                room_timeline::appendEventOrderEntry(
                  txn, orderDb, evToOrderDb, room_id, index, event_id, orderEntry);

                // TODO(Nico): Allow blacklisting more event types in UI
                if (!isHiddenEvent(txn, e, room_id)) {
                    room_timeline::appendMessageOrderEntry(
                      txn, order2msgDb, msg2orderDb, room_id, msgIndex, event_id);
                }
            } else {
                cache::activeLoggers().db->warn("duplicate event '{}'", orderEntry);
            }
            room_store::put(
              txn, eventsDb, cache::schema::RoomDb::Events, room_id, event_id, eventJson);
            if (hasExistingEvent) {
                room_timeline::rewriteRelationSourceReferences(
                  txn, relationsDb, room_id, event_id, relationTargets);
            } else {
                for (const auto &targetEventId : relationTargets) {
                    if (targetEventId.empty())
                        continue;
                    relationsDb.put(
                      txn,
                      room_store::key(cache::schema::RoomDb::Related, room_id, targetEventId),
                      event_id);
                }
            }
        }
    }
}
