// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <QApplication>
#include <QEventLoop>
#include <QTimer>

#include <mtx/events/messages/text.hpp>
#include <mtx/events/power_levels.hpp>
#include <mtx/events/spaces.hpp>
#include <mtx/responses/common.hpp>
#include <nlohmann/json.hpp>

#define private public
#include "cache/core/Cache_p.h"
#undef private

#include "cache/schema/CacheSchema.h"
#include "cache/schema/Codecs.h"
#include "cache/schema/RoomStore.h"
#include "cache/schema/RoomTimelineIndex.h"
#include "db/Catalog.h"
#include "db/ReadReceiptIndex.h"
#include "db/Scan.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/ThemeRegistry.h"
#include "TestEnvironment.h"

namespace {

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

template<typename Fn>
bool
expectNoThrow(Fn &&fn, std::string_view message)
{
    try {
        fn();
        return true;
    } catch (const std::exception &e) {
        std::cerr << "FAILED: " << message << " (" << e.what() << ")\n";
        return false;
    }
}

bool
containsName(const std::vector<std::string> &names, std::string_view needle)
{
    return std::find(names.begin(), names.end(), needle) != names.end();
}

bool
hasRoomStorePrefix(const std::vector<std::string> &names, std::string_view roomId)
{
    const auto prefix = std::string(roomId) + "/";
    return std::any_of(names.begin(), names.end(), [&prefix](const std::string &name) {
        return std::string_view(name).starts_with(prefix);
    });
}

std::string
serializeReceipt(std::string_view eventId, std::uint64_t timestamp, std::uint64_t eventIndex)
{
    nlohmann::json value;
    value["event_id"]    = eventId;
    value["timestamp"]   = timestamp;
    value["event_index"] = eventIndex;
    return value.dump();
}

mtx::events::RoomEvent<mtx::events::msg::Text>
makeTextEvent(const std::string &roomId,
              const std::string &eventId,
              const std::string &body,
              std::initializer_list<std::string_view> relationTargets = {})
{
    mtx::events::RoomEvent<mtx::events::msg::Text> event;
    event.type             = mtx::events::EventType::RoomMessage;
    event.sender           = "@tester:example.org";
    event.room_id          = roomId;
    event.event_id         = eventId;
    event.origin_server_ts = 1;
    event.content.body     = body;
    event.content.msgtype  = "m.text";

    for (const auto &target : relationTargets) {
        event.content.relations.relations.push_back(mtx::common::Relation{
          .rel_type = mtx::common::RelationType::Reference,
          .event_id = std::string(target),
        });
    }

    return event;
}

mtx::events::StateEvent<mtx::events::state::PowerLevels>
makePowerLevelsEvent(const std::string &roomId, const std::string &eventId)
{
    mtx::events::StateEvent<mtx::events::state::PowerLevels> event;
    event.type             = mtx::events::EventType::RoomPowerLevels;
    event.sender           = "@tester:example.org";
    event.room_id          = roomId;
    event.event_id         = eventId;
    event.origin_server_ts = 1;
    event.state_key        = "";
    event.content.users[event.sender] = mtx::events::state::Admin;
    return event;
}

mtx::events::StateEvent<mtx::events::state::space::Parent>
makeParentEvent(const std::string &roomId, const std::string &eventId, const std::string &parentSpaceId)
{
    mtx::events::StateEvent<mtx::events::state::space::Parent> event;
    event.type             = mtx::events::EventType::SpaceParent;
    event.sender           = "@tester:example.org";
    event.room_id          = roomId;
    event.event_id         = eventId;
    event.origin_server_ts = 1;
    event.state_key        = parentSpaceId;
    event.content.via      = std::vector<std::string>{"example.org"};
    return event;
}

mtx::events::StateEvent<mtx::events::state::space::Child>
makeChildEvent(const std::string &spaceId, const std::string &eventId, const std::string &childRoomId)
{
    mtx::events::StateEvent<mtx::events::state::space::Child> event;
    event.type             = mtx::events::EventType::SpaceChild;
    event.sender           = "@tester:example.org";
    event.room_id          = spaceId;
    event.event_id         = eventId;
    event.origin_server_ts = 1;
    event.state_key        = childRoomId;
    event.content.via      = std::vector<std::string>{"example.org"};
    return event;
}

std::unique_ptr<MatrixStore>
createStore(QStringView profile, QStringView userId)
{
    UserSettings::initialize(profile.toString());
    auto settings = UserSettings::instance();
    settings->setUsesFileSecretsProvider(true);
    settings->setSessionSnapshot(UserSettings::SessionSnapshot{
      .userId      = userId.toString(),
      .accessToken = QString{},
      .deviceId    = QStringLiteral("DEVICE"),
      .homeserver  = QStringLiteral("https://example.org"),
    });

    auto store = std::make_unique<MatrixStore>(userId.toString());
    QEventLoop loop;
    QTimer::singleShot(0, &loop, &QEventLoop::quit);
    loop.exec();

    return store;
}

bool
testIncompatibleCacheResetReopensCoreStores(MatrixStore &store)
{
    constexpr auto oldFormat = "2026.02.27";
    const std::string roomId = "!reset-room:example.org";

    {
        auto txn = store.beginTxn();
        cache::sync_state::putNextBatchToken(txn, store.db->syncState, "old-batch-token");
        cache::sync_state::putCacheFormatVersion(txn, store.db->syncState, oldFormat);
        cache::codec::putRoomInfo(txn, store.db->rooms, roomId, RoomInfo{});
        store.getEventsDb(txn, roomId);
        txn.commit();
    }

    bool ok = true;
    ok &= expect(store.runMigrations(),
                 "runMigrations resets an older cache format in place");
    ok &= expect(store.nextBatchToken().empty(),
                 "incompatible reset clears the saved next-batch token");

    {
        auto txn         = store.beginTxn(nullptr, db::TransactionFlags::ReadOnly);
        const auto names = store.storage().listStoreNames(txn);
        ok &= expect(containsName(names, db::catalog::globalName(db::catalog::GlobalDb::SyncState)),
                     "sync_state store remains available after reset");
        ok &= expect(containsName(names, db::catalog::globalName(db::catalog::GlobalDb::Rooms)),
                     "rooms store is reopened after reset");
        ok &= expect(!hasRoomStorePrefix(names, roomId),
                     "reset drops stale room-scoped stores");
    }

    ok &= expectNoThrow(
      [&] {
          auto txn = store.beginTxn();
          cache::codec::putRoomInfo(txn, store.db->rooms, roomId, RoomInfo{});
          txn.commit();
      },
      "reopened rooms handle accepts writes immediately after reset");

    {
        auto txn = store.beginTxn(nullptr, db::TransactionFlags::ReadOnly);
        std::string_view raw;
        ok &= expect(store.db->rooms.get(txn, roomId, raw),
                     "reopened rooms handle can be read in the same process after reset");
    }

    ok &= expectNoThrow([&] { store.deleteOldData(); },
                        "deleteOldData works on the first launch after an incompatible reset");

    return ok;
}

bool
testRoomRemovalDropsRoomScopedCache(MatrixStore &store)
{
    const std::string roomId           = "!room-remove:example.org";
    const std::string parentSpaceId    = "!parent-space:example.org";
    const std::string childRoomId      = "!child-room:example.org";
    const std::string receiptUserId    = "@alice:example.org";
    const std::string notificationEvent = "$room-event";
    const std::string inboundKey =
      R"({"room_id":"!room-remove:example.org","session_id":"SESSION"})";

    {
        auto txn = store.beginTxn();

        cache::codec::putRoomInfo(txn, store.db->rooms, roomId, RoomInfo{});
        cache::codec::putRoomInfo(txn, store.db->rooms, childRoomId, RoomInfo{});

        db::putReadReceiptValue(
          txn, store.db->readReceipts, roomId, receiptUserId, serializeReceipt("$old", 7, 1));

        store.db->spacesParents.put(txn, roomId, parentSpaceId);
        store.db->spacesChildren.put(txn, parentSpaceId, roomId);
        store.db->spacesChildren.put(txn, roomId, childRoomId);
        store.db->spacesParents.put(txn, childRoomId, roomId);

        store.db->invites.put(txn, roomId, "{}");
        store.db->encryptedRooms_.put(txn, roomId, "1");
        store.db->eventExpiryBgJob_.put(txn, roomId, "job");
        store.db->outboundMegolmSessions.put(txn, roomId, "session");
        store.db->inboundMegolmSessions.put(txn, inboundKey, "value");
        store.db->megolmSessionsData.put(txn, inboundKey, "value");

        auto eventsDb = store.getEventsDb(txn, roomId);
        room_store::put(
          txn, eventsDb, cache::schema::RoomDb::Events, roomId, notificationEvent, "{}");
        store.db->notifications.put(txn, notificationEvent, "1");

        auto eventOrderDb = store.getEventOrderDb(txn, roomId);
        auto eventToOrder = store.getEventToOrderDb(txn, roomId);
        auto messageToOrder = store.getMessageToOrderDb(txn, roomId);
        auto orderToMessage = store.getOrderToMessageDb(txn, roomId);
        auto pendingDb = store.getPendingMessagesDb(txn, roomId);
        auto relationsDb = store.getRelationsDb(txn, roomId);
        store.getInviteStatesDb(txn, roomId);
        store.getInviteMembersDb(txn, roomId);
        store.getStatesDb(txn, roomId);
        store.getStatesKeyDb(txn, roomId);
        store.getAccountDataDb(txn, roomId);
        store.getMembersDb(txn, roomId);

        room_store::put(
          txn, store.db->sharedRoomPlain, cache::schema::RoomDb::State, roomId, "m.room.name", "{}");
        db::putStateEventId(txn,
                            store.db->sharedRoomDupsort,
                            room_store::key(
                              cache::schema::RoomDb::StatesKey, roomId, "m.room.member"),
                            "@alice:example.org",
                            "$state");
        room_store::put(txn,
                        store.db->sharedRoomPlain,
                        cache::schema::RoomDb::Members,
                        roomId,
                        "@alice:example.org",
                        cache::codec::serializeMemberInfo(MemberInfo{"Alice", ""}));
        room_store::put(txn,
                        store.db->sharedRoomPlain,
                        cache::schema::RoomDb::InviteState,
                        roomId,
                        "m.room.topic",
                        "{}");
        room_store::put(txn,
                        store.db->sharedRoomPlain,
                        cache::schema::RoomDb::InviteMembers,
                        roomId,
                        "@bob:example.org",
                        cache::codec::serializeMemberInfo(MemberInfo{"Bob", ""}));
        room_store::put(txn,
                        store.db->sharedRoomPlain,
                        cache::schema::RoomDb::AccountData,
                        roomId,
                        "m.tag",
                        "{}");
        room_store::put(txn,
                        store.db->sharedRoomPlain,
                        cache::schema::RoomDb::AccountData,
                        "",
                        "global.keep",
                        R"({"type":"global.keep"})");
        room_timeline::putEventOrderMapping(txn,
                                            eventOrderDb,
                                            eventToOrder,
                                            roomId,
                                            10,
                                            notificationEvent,
                                            db::serializeOrderEntry(notificationEvent));
        room_timeline::putMessageOrderMapping(
          txn, orderToMessage, messageToOrder, roomId, 20, notificationEvent);
        pendingDb.put(
          txn, room_store::orderedIndexKey(cache::schema::RoomDb::Pending, roomId, 30), "txn-pending");
        relationsDb.put(
          txn, room_store::key(cache::schema::RoomDb::Related, roomId, "$target"), notificationEvent);

        txn.commit();
    }

    store.removeRoom(roomId);

    bool ok = true;
    auto txn = store.beginTxn(nullptr, db::TransactionFlags::ReadOnly);

    const auto names = store.storage().listStoreNames(txn);
    ok &= expect(!hasRoomStorePrefix(names, roomId),
                 "removeRoom drops every room-prefixed named store");

    std::string_view raw;
    ok &= expect(!store.db->rooms.get(txn, roomId, raw),
                 "removeRoom deletes the room info entry");
    ok &= expect(!store.db->invites.get(txn, roomId, raw),
                 "removeRoom deletes the invite entry");
    ok &= expect(!store.db->encryptedRooms_.get(txn, roomId, raw),
                 "removeRoom deletes encrypted-room metadata");
    ok &= expect(!store.db->eventExpiryBgJob_.get(txn, roomId, raw),
                 "removeRoom deletes event-expiry background state");
    ok &= expect(!store.db->outboundMegolmSessions.get(txn, roomId, raw),
                 "removeRoom deletes outbound megolm session state");
    ok &= expect(!store.db->inboundMegolmSessions.get(txn, inboundKey, raw),
                 "removeRoom deletes room-scoped inbound megolm sessions");
    ok &= expect(!store.db->megolmSessionsData.get(txn, inboundKey, raw),
                 "removeRoom deletes room-scoped megolm metadata");
    ok &= expect(!store.db->notifications.get(txn, notificationEvent, raw),
                 "removeRoom deletes room event notifications");
    ok &= expect(!db::getReadReceiptValue(txn, store.db->readReceipts, roomId, receiptUserId, raw),
                 "removeRoom deletes room read receipts");
    ok &= expect(room_store::countEntries(
                   txn, store.db->sharedRoomPlain, cache::schema::RoomDb::State, roomId) == 0,
                 "removeRoom deletes shared room state entries");
    ok &= expect(room_store::countEntries(
                   txn, store.db->sharedRoomDupsort, cache::schema::RoomDb::StatesKey, roomId) ==
                   0,
                 "removeRoom deletes shared room states_key entries");
    ok &= expect(room_store::countEntries(
                   txn, store.db->sharedRoomPlain, cache::schema::RoomDb::Members, roomId) == 0,
                 "removeRoom deletes shared room member entries");
    ok &= expect(room_store::countEntries(
                   txn,
                   store.db->sharedRoomPlain,
                   cache::schema::RoomDb::InviteState,
                   roomId) == 0,
                 "removeRoom deletes shared invite-state entries");
    ok &= expect(room_store::countEntries(
                   txn,
                   store.db->sharedRoomPlain,
                   cache::schema::RoomDb::InviteMembers,
                   roomId) == 0,
                 "removeRoom deletes shared invite-member entries");
    ok &= expect(room_store::countEntries(
                   txn,
                   store.db->sharedRoomPlain,
                   cache::schema::RoomDb::AccountData,
                   roomId) == 0,
                 "removeRoom deletes room-scoped shared account data entries");
    ok &= expect(room_store::countEntries(
                   txn, store.db->sharedRoomPlain, cache::schema::RoomDb::Events, roomId) == 0,
                 "removeRoom deletes shared room event payload entries");
    ok &= expect(room_store::countEntries(
                   txn, store.db->sharedRoomPlain, cache::schema::RoomDb::EventToOrder, roomId) == 0,
                 "removeRoom deletes shared room event-to-order entries");
    ok &= expect(room_store::countEntries(
                   txn,
                   store.db->sharedRoomPlain,
                   cache::schema::RoomDb::MessageToOrder,
                   roomId) == 0,
                 "removeRoom deletes shared room message-to-order entries");
    ok &= expect(room_store::countEntries(
                   txn, store.db->sharedRoomOrdered, cache::schema::RoomDb::EventOrder, roomId) == 0,
                 "removeRoom deletes shared room event-order entries");
    ok &= expect(room_store::countEntries(
                   txn,
                   store.db->sharedRoomOrdered,
                   cache::schema::RoomDb::OrderToMessage,
                   roomId) == 0,
                 "removeRoom deletes shared room order-to-message entries");
    ok &= expect(room_store::countEntries(
                   txn, store.db->sharedRoomOrdered, cache::schema::RoomDb::Pending, roomId) == 0,
                 "removeRoom deletes shared room pending entries");
    ok &= expect(room_store::countEntries(
                   txn, store.db->sharedRoomDupsort, cache::schema::RoomDb::Related, roomId) == 0,
                 "removeRoom deletes shared room relation entries");
    ok &= expect(room_store::countEntries(
                   txn, store.db->sharedRoomPlain, cache::schema::RoomDb::AccountData, "") == 1,
                 "removeRoom preserves global shared account data entries");

    ok &= expect(db::listDupValues(txn, store.db->spacesParents, roomId).empty(),
                 "removeRoom clears parent-space links for the removed room");
    ok &= expect(db::listDupValues(txn, store.db->spacesChildren, roomId).empty(),
                 "removeRoom clears child-room links for the removed room");

    const auto parentChildren = db::listDupValues(txn, store.db->spacesChildren, parentSpaceId);
    ok &= expect(std::find(parentChildren.begin(), parentChildren.end(), roomId) ==
                   parentChildren.end(),
                 "removeRoom clears reverse parent-space children links");

    const auto childParents = db::listDupValues(txn, store.db->spacesParents, childRoomId);
    ok &= expect(std::find(childParents.begin(), childParents.end(), roomId) == childParents.end(),
                 "removeRoom clears reverse child-room parent links");

    return ok;
}

bool
testReadReceiptsAdvanceAsCurrentState(MatrixStore &store)
{
    const std::string roomId      = "!receipts:example.org";
    const std::string userId      = "@alice:example.org";
    const std::string oldEventId  = "$old";
    const std::string newEventId  = "$new";
    const std::string nextEventId = "$next";

    {
        auto txn          = store.beginTxn();
        auto eventToOrder = store.getEventToOrderDb(txn, roomId);
        room_store::put(
          txn, eventToOrder, cache::schema::RoomDb::EventToOrder, roomId, oldEventId, db::toSv(10ULL));
        room_store::put(
          txn, eventToOrder, cache::schema::RoomDb::EventToOrder, roomId, newEventId, db::toSv(20ULL));
        room_store::put(
          txn, eventToOrder, cache::schema::RoomDb::EventToOrder, roomId, nextEventId, db::toSv(30ULL));
        txn.commit();
    }

    {
        auto txn = store.beginTxn();
        store.updateReadReceipt(
          txn, roomId, MatrixStore::Receipts{{oldEventId, {{userId, 100}}}});
        txn.commit();
    }
    {
        auto txn = store.beginTxn();
        store.updateReadReceipt(
          txn, roomId, MatrixStore::Receipts{{newEventId, {{userId, 200}}}});
        txn.commit();
    }

    bool ok = true;

    {
        auto txn = store.beginTxn(nullptr, db::TransactionFlags::ReadOnly);
        std::string_view raw;
        ok &= expect(db::getReadReceiptValue(txn, store.db->readReceipts, roomId, userId, raw),
                     "latest read receipt is stored by room and user");
        if (raw.empty()) {
            ok = false;
        } else {
            const auto parsed = nlohmann::json::parse(raw);
            ok &= expect(parsed.value("event_id", std::string()) == newEventId,
                         "advancing a read receipt overwrites the previous event marker");
            ok &= expect(parsed.value("timestamp", std::uint64_t{0}) == 200,
                         "advancing a read receipt preserves the newest timestamp");
            ok &= expect(parsed.value("event_index", std::uint64_t{0}) == 20,
                         "advancing a read receipt stores the newest event index");
        }
    }

    const auto oldReceipts =
      store.readReceipts(QString::fromStdString(oldEventId), QString::fromStdString(roomId));
    const auto newReceipts =
      store.readReceipts(QString::fromStdString(newEventId), QString::fromStdString(roomId));
    const auto nextReceipts =
      store.readReceipts(QString::fromStdString(nextEventId), QString::fromStdString(roomId));

    ok &= expect(oldReceipts.size() == 1 && oldReceipts.begin()->second == userId,
                 "readReceipts reports the user as read up to older events");
    ok &= expect(newReceipts.size() == 1 && newReceipts.begin()->second == userId,
                 "readReceipts reports the user on the current event marker");
    ok &= expect(nextReceipts.empty(),
                 "readReceipts does not report the user past their current marker");

    return ok;
}

bool
testLimitedSyncCleanupPreservesCurrentStatePayloads(MatrixStore &store)
{
    const std::string roomId             = "!limited:example.org";
    const std::string powerLevelsEventId = "$power-levels";
    const std::string parentEventId      = "$parent";
    const std::string parentSpaceId      = "!space:example.org";
    const std::string oldEventId         = "$old";
    const std::string oldRelatedEventId  = "$old-related";
    const std::string oldTargetEventId   = "$old-target";
    const std::string newEventId         = "$new";

    {
        auto txn          = store.beginTxn();
        auto eventsDb     = store.getEventsDb(txn, roomId);
        auto relationsDb  = store.getRelationsDb(txn, roomId);
        auto orderDb      = store.getEventOrderDb(txn, roomId);
        auto evToOrderDb  = store.getEventToOrderDb(txn, roomId);
        auto msg2orderDb  = store.getMessageToOrderDb(txn, roomId);
        auto order2msgDb  = store.getOrderToMessageDb(txn, roomId);
        auto pendingDb    = store.getPendingMessagesDb(txn, roomId);
        auto statesDb     = store.getStatesDb(txn, roomId);
        auto statesKeyDb  = store.getStatesKeyDb(txn, roomId);
        auto membersDb    = store.getMembersDb(txn, roomId);
        auto powerLevels  = makePowerLevelsEvent(roomId, powerLevelsEventId);
        auto parentEvent  = makeParentEvent(roomId, parentEventId, parentSpaceId);
        auto oldEvent     = makeTextEvent(roomId, oldEventId, "old timeline event");
        const std::string relationTarget = oldTargetEventId;
        auto oldRelated =
          makeTextEvent(roomId, oldRelatedEventId, "old related event", {relationTarget});

        cache::codec::putRoomInfo(txn, store.db->rooms, roomId, RoomInfo{});
        store.saveStateEvent(txn,
                             statesDb,
                             statesKeyDb,
                             membersDb,
                             eventsDb,
                             roomId,
                             mtx::events::collections::StateEvents{powerLevels});
        store.saveStateEvent(txn,
                             statesDb,
                             statesKeyDb,
                             membersDb,
                             eventsDb,
                             roomId,
                             mtx::events::collections::StateEvents{parentEvent});

        std::uint64_t eventIndex   = 10;
        std::uint64_t messageIndex = 10;
        room_timeline::appendEventOrderEntry(
          txn, orderDb, evToOrderDb, roomId, eventIndex, oldEventId, db::serializeOrderEntry(oldEventId));
        room_timeline::appendMessageOrderEntry(
          txn, order2msgDb, msg2orderDb, roomId, messageIndex, oldEventId);
        room_timeline::appendEventOrderEntry(txn,
                                             orderDb,
                                             evToOrderDb,
                                             roomId,
                                             eventIndex,
                                             oldRelatedEventId,
                                             db::serializeOrderEntry(oldRelatedEventId));
        room_timeline::appendMessageOrderEntry(
          txn, order2msgDb, msg2orderDb, roomId, messageIndex, oldRelatedEventId);

        room_store::put(
          txn, eventsDb, cache::schema::RoomDb::Events, roomId, oldEventId, nlohmann::json(oldEvent).dump());
        room_store::put(txn,
                        eventsDb,
                        cache::schema::RoomDb::Events,
                        roomId,
                        oldRelatedEventId,
                        nlohmann::json(oldRelated).dump());
        relationsDb.put(
          txn, room_store::key(cache::schema::RoomDb::Related, roomId, relationTarget), oldRelatedEventId);
        pendingDb.put(txn,
                      room_store::orderedIndexKey(cache::schema::RoomDb::Pending, roomId, 1),
                      "txn-old");

        txn.commit();
    }

    mtx::responses::Timeline limited;
    limited.limited    = true;
    limited.prev_batch = "fresh-batch";
    limited.events.push_back(
      mtx::events::collections::TimelineEvents{makeTextEvent(roomId, newEventId, "new event")});

    {
        auto txn      = store.beginTxn();
        auto eventsDb = store.getEventsDb(txn, roomId);
        store.saveTimelineMessages(txn, eventsDb, roomId, limited);
        txn.commit();
    }

    bool ok = true;
    auto txn = store.beginTxn(nullptr, db::TransactionFlags::ReadOnly);

    auto eventsDb    = store.getEventsDb(txn, roomId);
    auto relationsDb = store.getRelationsDb(txn, roomId);
    auto orderDb     = store.getEventOrderDb(txn, roomId);
    auto pendingDb   = store.getPendingMessagesDb(txn, roomId);

    std::string_view raw;
    ok &= expect(room_store::get(
                   txn, eventsDb, cache::schema::RoomDb::Events, roomId, powerLevelsEventId, raw),
                 "limited sync preserves empty-state-key event payloads in eventsDb");
    ok &= expect(
                   room_store::get(txn, eventsDb, cache::schema::RoomDb::Events, roomId, parentEventId, raw),
                 "limited sync preserves keyed state event payloads in eventsDb");
    ok &= expect(
                   !room_store::get(txn, eventsDb, cache::schema::RoomDb::Events, roomId, oldEventId, raw),
                 "limited sync deletes stale non-state event payloads");
    ok &= expect(!room_store::get(
                   txn, eventsDb, cache::schema::RoomDb::Events, roomId, oldRelatedEventId, raw),
                 "limited sync deletes stale related-event payloads");
    ok &= expect(room_store::get(
                   txn, eventsDb, cache::schema::RoomDb::Events, roomId, newEventId, raw),
                 "limited sync stores the new timeline event payload");

    ok &= expect(room_timeline::listOrderEntryEventIds(txn, orderDb, roomId) ==
                   std::vector<std::string>{newEventId},
                 "limited sync rebuilds the timeline order index from the new batch only");
    ok &= expect(
                   db::listDupValues(
                     txn, relationsDb, room_store::key(cache::schema::RoomDb::Related, roomId, oldTargetEventId))
                     .empty(),
                 "limited sync clears stale relation rows");
    ok &= expect(room_store::countEntries(
                   txn, pendingDb, cache::schema::RoomDb::Pending, roomId) == 0,
                 "limited sync clears stale pending-event entries");

    const auto powerLevels = store.getStateEvent<mtx::events::state::PowerLevels>(txn, roomId, "");
    ok &= expect(powerLevels && powerLevels->event_id == powerLevelsEventId,
                 "limited sync keeps current power-level state readable");

    const auto parentEvent =
      store.getStateEvent<mtx::events::state::space::Parent>(txn, roomId, parentSpaceId);
    ok &= expect(parentEvent && parentEvent->event_id == parentEventId,
                 "limited sync keeps keyed state readable through states_key indexes");

    return ok;
}

bool
testSharedTimelineReplacementKeepsOrderedIndexesConsistent(MatrixStore &store)
{
    const std::string roomId      = "!shared-replace:example.org";
    const std::string pendingId   = "mshared-replace";
    const std::string sentEventId = "$shared-replace";

    const std::uint64_t eventIndex   = 42;
    const std::uint64_t messageIndex = 24;

    {
        auto txn            = store.beginTxn();
        auto eventsDb       = store.getEventsDb(txn, roomId);
        auto eventOrderDb   = store.getEventOrderDb(txn, roomId);
        auto eventToOrderDb = store.getEventToOrderDb(txn, roomId);
        auto messageToOrder = store.getMessageToOrderDb(txn, roomId);
        auto orderToMessage = store.getOrderToMessageDb(txn, roomId);

        cache::codec::putRoomInfo(txn, store.db->rooms, roomId, RoomInfo{});

        const auto pendingEvent = makeTextEvent(roomId, pendingId, "pending event");
        room_store::put(txn,
                        eventsDb,
                        cache::schema::RoomDb::Events,
                        roomId,
                        pendingId,
                        nlohmann::json(pendingEvent).dump());
        room_timeline::putEventOrderMapping(txn,
                                            eventOrderDb,
                                            eventToOrderDb,
                                            roomId,
                                            eventIndex,
                                            pendingId,
                                            db::serializeOrderEntry(pendingId));
        room_timeline::putMessageOrderMapping(
          txn, orderToMessage, messageToOrder, roomId, messageIndex, pendingId);

        const auto sentEvent = makeTextEvent(roomId, sentEventId, "sent event");
        const auto replaced  = room_timeline::replaceTimelineEventId(txn,
                                                                    eventsDb,
                                                                    eventOrderDb,
                                                                    eventToOrderDb,
                                                                    messageToOrder,
                                                                    orderToMessage,
                                                                    roomId,
                                                                    pendingId,
                                                                    sentEventId,
                                                                    nlohmann::json(sentEvent).dump(),
                                                                    db::serializeOrderEntry(sentEventId));
        txn.commit();

        if (!replaced) {
            std::cerr << "FAILED: shared timeline replacement updates pending event id\n";
            return false;
        }
    }

    bool ok = true;
    ok &= expect(store.getEvent(roomId, sentEventId).has_value(),
                 "shared timeline replacement keeps sent event readable via MatrixStore");
    ok &= expect(!store.getEvent(roomId, pendingId).has_value(),
                 "shared timeline replacement removes pending event from MatrixStore lookup");
    ok &= expect(store.getTimelineIndex(roomId, sentEventId) == std::optional<std::uint64_t>{messageIndex},
                 "shared timeline replacement keeps message-order lookup on the sent event");
    ok &= expect(!store.getTimelineIndex(roomId, pendingId).has_value(),
                 "shared timeline replacement removes message-order lookup for the pending event");
    ok &= expect(store.getEventIndex(roomId, sentEventId) == std::optional<std::uint64_t>{eventIndex},
                 "shared timeline replacement keeps event-order lookup on the sent event");
    ok &= expect(!store.getEventIndex(roomId, pendingId).has_value(),
                 "shared timeline replacement removes event-order lookup for the pending event");

    {
        auto txn = store.beginTxn(nullptr, db::TransactionFlags::ReadOnly);

        auto eventsDb       = store.getEventsDb(txn, roomId);
        auto eventOrderDb   = store.getEventOrderDb(txn, roomId);
        auto orderToMessage = store.getOrderToMessageDb(txn, roomId);

        std::string_view raw;
        ok &= expect(!room_store::get(
                       txn, eventsDb, cache::schema::RoomDb::Events, roomId, pendingId, raw),
                     "shared timeline replacement removes pending event payload");
        ok &= expect(room_store::get(
                       txn, eventsDb, cache::schema::RoomDb::Events, roomId, sentEventId, raw),
                     "shared timeline replacement stores sent event payload");
        ok &= expect(room_timeline::timelineEventIdAtIndex(txn, orderToMessage, roomId, messageIndex) ==
                       std::optional<std::string>{sentEventId},
                     "shared timeline replacement rewrites the visible timeline slot");
        ok &= expect(room_timeline::listOrderEntryEventIds(txn, eventOrderDb, roomId) ==
                       std::vector<std::string>{sentEventId},
                     "shared timeline replacement rewrites the event-order entry");

        const auto legacyMessageKey =
          room_store::key(cache::schema::RoomDb::OrderToMessage, roomId, db::toSv(messageIndex));
        const auto correctMessageKey =
          room_store::orderedIndexKey(cache::schema::RoomDb::OrderToMessage, roomId, messageIndex);
        if (legacyMessageKey != correctMessageKey) {
            ok &= expect(!orderToMessage.get(txn, legacyMessageKey, raw),
                         "shared timeline replacement does not leave behind a legacy order_to_message key");
        }

        const auto legacyEventKey =
          room_store::key(cache::schema::RoomDb::EventOrder, roomId, db::toSv(eventIndex));
        const auto correctEventKey =
          room_store::orderedIndexKey(cache::schema::RoomDb::EventOrder, roomId, eventIndex);
        if (legacyEventKey != correctEventKey) {
            ok &= expect(!eventOrderDb.get(txn, legacyEventKey, raw),
                         "shared timeline replacement does not leave behind a legacy event_order key");
        }
    }

    return ok;
}

bool
testSpaceEdgesRebuildAcrossRoomAndSpaceRefreshes(MatrixStore &store)
{
    const std::string roomId        = "!space-room:example.org";
    const std::string spaceAId      = "!space-a:example.org";
    const std::string spaceBId      = "!space-b:example.org";
    const auto hasParent            = [&store, &roomId](std::string_view parentSpaceId) {
        const auto parents = store.getParentRoomIds(roomId);
        return containsName(parents, parentSpaceId);
    };
    const auto hasChild             = [&store, &roomId](const std::string &spaceId) {
        const auto children = store.getChildRoomIds(spaceId);
        return containsName(children, roomId);
    };

    mtx::responses::StateEvents state;

    state.events = {
      mtx::events::collections::StateEvents{makePowerLevelsEvent(spaceAId, "$space-a-pl")},
      mtx::events::collections::StateEvents{makeChildEvent(spaceAId, "$space-a-child", roomId)},
    };
    store.updateState(spaceAId, state, true);

    state.events = {
      mtx::events::collections::StateEvents{makePowerLevelsEvent(spaceBId, "$space-b-pl")},
    };
    store.updateState(spaceBId, state, true);

    state.events = {
      mtx::events::collections::StateEvents{makeParentEvent(roomId, "$room-parent-a", spaceAId)},
    };
    store.updateState(roomId, state, true);

    bool ok = true;
    ok &= expect(hasParent(spaceAId), "room update adds the authorized parent edge");
    ok &= expect(hasChild(spaceAId), "space child state adds the reverse child edge");

    state.events.clear();
    store.updateState(roomId, state, true);

    ok &= expect(hasParent(spaceAId),
                 "room wipe keeps the edge when a valid child link from the parent space remains");
    ok &= expect(hasChild(spaceAId),
                 "room wipe does not remove the reverse child edge that still comes from m.space.child");

    state.events = {
      mtx::events::collections::StateEvents{makeParentEvent(roomId, "$room-parent-b", spaceBId)},
    };
    store.updateState(roomId, state, true);

    ok &= expect(hasParent(spaceAId),
                 "reparenting keeps the existing child-derived edge until the parent space drops it");
    ok &= expect(hasParent(spaceBId),
                 "reparenting adds the newly authorized room-parent edge");
    ok &= expect(hasChild(spaceBId),
                 "reparenting updates the reverse child edge for the new parent space");

    state.events = {
      mtx::events::collections::StateEvents{makePowerLevelsEvent(spaceAId, "$space-a-pl-2")},
    };
    store.updateState(spaceAId, state, true);

    ok &= expect(!hasParent(spaceAId),
                 "space refresh removes the stale old parent edge once the child link is gone");
    ok &= expect(!hasChild(spaceAId),
                 "space refresh removes the reverse child edge once the child state is gone");
    ok &= expect(hasParent(spaceBId),
                 "space refresh leaves the new parent edge intact");
    ok &= expect(hasChild(spaceBId),
                 "space refresh leaves the new reverse child edge intact");

    return ok;
}

} // namespace

int
main(int argc, char **argv)
{
    test_env::ScopedTestHome testHome{QStringLiteral("komai-matrix-store-test")};
    if (!testHome.isValid()) {
        std::cerr << "FAILED: test home environment can be created\n";
        return 1;
    }
    if (!testHome.isIsolated()) {
        std::cerr << "FAILED: test home environment is isolated\n";
        return 1;
    }

    test_env::EnvVarOverride forcedSecretService{
      "KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY", QByteArrayLiteral("unavailable")};

    QApplication app(argc, argv);
    ThemeRegistry::initialize();

    auto store = createStore(QStringLiteral("matrix-store-shared"),
                             QStringLiteral("@tester:example.org"));
    if (!store) {
        std::cerr << "FAILED: could not construct shared MatrixStore harness\n";
        return 1;
    }

    bool ok = true;
    ok &= testIncompatibleCacheResetReopensCoreStores(*store);
    ok &= testRoomRemovalDropsRoomScopedCache(*store);
    ok &= testReadReceiptsAdvanceAsCurrentState(*store);
    ok &= testLimitedSyncCleanupPreservesCurrentStatePayloads(*store);
    ok &= testSharedTimelineReplacementKeepsOrderedIndexesConsistent(*store);
    ok &= testSpaceEdgesRebuildAcrossRoomAndSpaceRefreshes(*store);
    return ok ? 0 : 1;
}
