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

#include <nlohmann/json.hpp>

#define private public
#include "cache/core/Cache_p.h"
#undef private

#include "cache/schema/CacheSchema.h"
#include "cache/schema/Codecs.h"
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

std::string
integerKey(std::uint64_t value)
{
    std::string key(sizeof(value), '\0');
    std::memcpy(key.data(), &value, sizeof(value));
    return key;
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
        eventsDb.put(txn, notificationEvent, "{}");
        store.db->notifications.put(txn, notificationEvent, "1");

        store.getEventOrderDb(txn, roomId);
        store.getEventToOrderDb(txn, roomId);
        store.getMessageToOrderDb(txn, roomId);
        store.getOrderToMessageDb(txn, roomId);
        store.getPendingMessagesDb(txn, roomId);
        store.getRelationsDb(txn, roomId);
        store.getInviteStatesDb(txn, roomId);
        store.getInviteMembersDb(txn, roomId);
        store.getStatesDb(txn, roomId);
        store.getStatesKeyDb(txn, roomId);
        store.getAccountDataDb(txn, roomId);
        store.getMembersDb(txn, roomId);

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
        eventToOrder.put(txn, oldEventId, integerKey(10));
        eventToOrder.put(txn, newEventId, integerKey(20));
        eventToOrder.put(txn, nextEventId, integerKey(30));
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
    return ok ? 0 : 1;
}
