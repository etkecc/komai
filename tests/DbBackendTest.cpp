// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QTemporaryDir>
#include <QByteArray>

#include <nlohmann/json.hpp>

#include "CacheStructs.h"
#include "CacheCryptoStructs.h"
#include "db/Backend.h"
#include "db/Catalog.h"
#include "db/Compaction.h"
#include "db/DbTypes.h"
#include "db/Json.h"
#include "db/DupIndex.h"
#include "db/MegolmIndex.h"
#include "db/MemberInfo.h"
#include "db/NamePolicy.h"
#include "db/Open.h"
#include "db/OlmSessionIndex.h"
#include "db/OrderEntry.h"
#include "db/ReadReceiptIndex.h"
#include "db/RoomInfo.h"
#include "db/Scan.h"
#include "db/Schema.h"
#include "db/StateIndex.h"
#include "db/SyncState.h"
#include "db/TimelineIndex.h"

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
expectDbError(Fn &&fn, std::string_view message)
{
    try {
        fn();
    } catch (const db::Error &) {
        return true;
    } catch (const std::exception &e) {
        std::cerr << "FAILED: " << message << " (unexpected exception: " << e.what() << ")\n";
        return false;
    }

    std::cerr << "FAILED: " << message << " (no exception)\n";
    return false;
}

std::string
compositeStateValue(std::string_view stateKey, std::string_view eventId)
{
    std::string value;
    value.reserve(stateKey.size() + 1 + eventId.size());
    value.append(stateKey);
    value.push_back('\0');
    value.append(eventId);
    return value;
}

std::string_view
stateKeyFromComposite(std::string_view value)
{
    return value.substr(0, value.find('\0'));
}

std::string
integerKey(std::uint64_t value)
{
    std::string key(sizeof(value), '\0');
    std::memcpy(key.data(), &value, sizeof(value));
    return key;
}

std::uint64_t
readIntegerKey(std::string_view key)
{
    std::uint64_t value = 0;
    std::memcpy(&value, key.data(), std::min(key.size(), sizeof(value)));
    return value;
}

bool
containsName(const std::vector<std::string> &names, std::string_view needle)
{
    return std::find(names.begin(), names.end(), needle) != names.end();
}

struct EnvVarGuard
{
    explicit EnvVarGuard(const char *name)
      : name_(name)
      , original_(qgetenv(name))
      , hadOriginal_(qEnvironmentVariableIsSet(name))
    {
    }

    ~EnvVarGuard()
    {
        if (hadOriginal_)
            qputenv(name_, original_);
        else
            qunsetenv(name_);
    }

    void set(std::string_view value) const
    {
        qputenv(name_, QByteArray(value.data(), static_cast<int>(value.size())));
    }

    void unset() const { qunsetenv(name_); }

    const char *name_;
    QByteArray original_;
    bool hadOriginal_;
};

db::DbiOpenOptions
openOptions(db::DbiFlags flags = db::DbiFlags::None,
            std::optional<db::DupsortComparator> comparator = std::nullopt)
{
    return db::DbiOpenOptions{
      .flags             = flags,
      .dupsortComparator = comparator,
    };
}

bool
testNamePolicy()
{
    bool ok = true;

    const auto roomEventOrder = db::openOptionsForRoom(db::catalog::RoomDb::EventOrder);
    ok &= expect(db::hasFlag(roomEventOrder.flags, db::DbiFlags::IntegerKey),
                 "typed name policy sets IntegerKey for RoomDb::EventOrder");

    const auto roomStatesKey = db::openOptionsForRoom(db::catalog::RoomDb::StatesKey);
    ok &= expect(db::hasFlag(roomStatesKey.flags, db::DbiFlags::DupSort),
                 "typed name policy sets DupSort for RoomDb::StatesKey");
    ok &= expect(roomStatesKey.dupsortComparator.has_value() &&
                   *roomStatesKey.dupsortComparator == db::DupsortComparator::StateKey,
                 "typed name policy sets StateKey comparator for RoomDb::StatesKey");

    const auto globalSpaces = db::openOptionsForGlobal(db::catalog::GlobalDb::SpacesChildren);
    ok &= expect(db::hasFlag(globalSpaces.flags, db::DbiFlags::DupSort),
                 "typed name policy sets DupSort for GlobalDb::SpacesChildren");

    const auto roomOrder =
      db::openOptionsForName(db::catalog::roomName("!room:example", db::catalog::RoomDb::EventOrder));
    ok &= expect(db::hasFlag(roomOrder.flags, db::DbiFlags::IntegerKey),
                 "name policy sets IntegerKey for /event_order");

    const auto relation =
      db::openOptionsForName(db::catalog::roomName("!room:example", db::catalog::RoomDb::Related));
    ok &= expect(db::hasFlag(relation.flags, db::DbiFlags::DupSort),
                 "name policy sets DupSort for /related");

    const auto stateKey =
      db::openOptionsForName(db::catalog::roomName("!room:example", db::catalog::RoomDb::StatesKey));
    ok &= expect(db::hasFlag(stateKey.flags, db::DbiFlags::DupSort),
                 "name policy sets DupSort for /states_key");
    ok &= expect(stateKey.dupsortComparator.has_value() &&
                   *stateKey.dupsortComparator == db::DupsortComparator::StateKey,
                 "name policy sets StateKey comparator for /states_key");

    const auto legacyStateKey = db::openOptionsForName(
      db::catalog::roomName("!room:example", db::catalog::RoomDb::LegacyStateByKey));
    ok &= expect(legacyStateKey.dupsortComparator.has_value() &&
                   *legacyStateKey.dupsortComparator ==
                     db::DupsortComparator::LegacyStateByKeyJson,
                 "name policy sets legacy comparator for /state_by_key");

    const auto topLevelSpace =
      db::openOptionsForName(db::catalog::globalName(db::catalog::GlobalDb::SpacesChildren));
    ok &= expect(db::hasFlag(topLevelSpace.flags, db::DbiFlags::DupSort),
                 "name policy sets DupSort for top-level space_children");

    const auto simple = db::openOptionsForName(db::catalog::globalName(db::catalog::GlobalDb::Rooms));
    ok &= expect(simple.flags == db::DbiFlags::None && !simple.dupsortComparator.has_value(),
                 "name policy leaves simple db names unflagged");

    return ok;
}

bool
testCatalog()
{
    bool ok = true;

    ok &= expect(db::catalog::globalName(db::catalog::GlobalDb::Rooms) == "rooms",
                 "catalog returns rooms global name");

    const auto eventsName = db::catalog::roomName("!room:example", db::catalog::RoomDb::Events);
    ok &= expect(eventsName == "!room:example/events", "catalog builds room db names");
    ok &= expect(db::catalog::hasRoomSuffix(eventsName, db::catalog::RoomDb::Events),
                 "catalog detects matching room suffix");
    ok &= expect(!db::catalog::hasRoomSuffix(eventsName, db::catalog::RoomDb::Members),
                 "catalog detects non-matching room suffix");

    ok &= expect(db::catalog::syncStateKey(db::catalog::SyncStateKey::NextBatch) == "next_batch",
                 "catalog returns sync-state key names");
    ok &= expect(db::catalog::syncStateSecretKey("pickle_secret") == "secret.pickle_secret",
                 "catalog builds sync-state secret key names");

    const auto mentionsName =
      db::catalog::roomName("!room:example", db::catalog::RoomDb::LegacyMentions);
    ok &= expect(mentionsName == "!room:example/mentions",
                 "catalog builds legacy mentions room db names");

    ok &= expect(db::catalog::legacyOlmSessionsPrefixV1() == "olm_sessions/",
                 "catalog exposes legacy olm v1 prefix");
    ok &= expect(db::catalog::legacyOlmSessionsPrefixV2() == "olm_sessions.v2/",
                 "catalog exposes legacy olm v2 prefix");

    ok &= expect(db::catalog::isLegacyOlmShardV1("olm_sessions/curve"),
                 "catalog detects legacy olm v1 shard");
    ok &= expect(db::catalog::isLegacyOlmShardV2("olm_sessions.v2/curve"),
                 "catalog detects legacy olm v2 shard");

    ok &= expect(db::catalog::legacyOlmShardV2NameFromV1("olm_sessions/curve") ==
                   "olm_sessions.v2/curve",
                 "catalog converts legacy olm v1 shard names to v2");

    const auto curve = db::catalog::legacyOlmCurveFromV2Name("olm_sessions.v2/curve");
    ok &= expect(curve.has_value() && *curve == "curve",
                 "catalog extracts curve id from legacy olm v2 shard names");
    ok &= expect(!db::catalog::legacyOlmCurveFromV2Name("not-olm/curve").has_value(),
                 "catalog rejects non-olm db names for v2 curve extraction");

    const auto olmKey = db::catalog::olmSessionKey("curve", "session");
    ok &= expect(olmKey == std::string("curve\0session", 13),
                 "catalog builds v3 olm composite session keys");

    const auto [splitCurve, splitSession] = db::catalog::splitOlmSessionKey(olmKey);
    ok &= expect(splitCurve == "curve", "catalog splits v3 olm key curve part");
    ok &= expect(splitSession == "session", "catalog splits v3 olm key session part");

    const auto stateIndex = db::catalog::stateEventIndexValue("state-key", "$event");
    ok &= expect(stateIndex == std::string("state-key\0$event", 16),
                 "catalog builds state-event index composite values");

    const auto [splitStateKey, splitEventId] = db::catalog::splitStateEventIndexValue(stateIndex);
    ok &= expect(splitStateKey == "state-key", "catalog splits state-event index state key");
    ok &= expect(splitEventId == "$event", "catalog splits state-event index event id");

    const auto [plainStateKey, plainEventId] =
      db::catalog::splitStateEventIndexValue("plain-state-key");
    ok &= expect(plainStateKey == "plain-state-key",
                 "catalog split preserves plain values with no separator");
    ok &= expect(plainEventId.empty(), "catalog split returns empty event id when separator missing");

    return ok;
}

bool
testSchemaHelpers()
{
    bool ok = true;

    const auto roomDbs = db::roomDbsForFullResync();
    ok &= expect(!roomDbs.empty(), "schema helper exposes non-empty full-resync room db list");
    ok &= expect(std::find(roomDbs.begin(), roomDbs.end(), db::catalog::RoomDb::Events) != roomDbs.end(),
                 "schema helper list includes RoomDb::Events");
    ok &= expect(std::find(roomDbs.begin(), roomDbs.end(), db::catalog::RoomDb::LegacyStateByKey) !=
                   roomDbs.end(),
                 "schema helper list includes RoomDb::LegacyStateByKey");

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 128;
    backend->open("", options);

    const auto roomId    = std::string("!room:example");
    const auto eventsDbi = db::catalog::roomName(roomId, db::catalog::RoomDb::Events);

    {
        auto txn   = backend->beginTxn();
        auto events = db::openRoomDbi(*backend, txn, roomId, db::catalog::RoomDb::Events);
        ok &= expect(events.put(txn, "$event", "{}"), "schema helper setup creates room events db");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn();
        std::string error;
        ok &= expect(db::tryDropNamedDbi(*backend, txn, eventsDbi, &error),
                     "schema helper drops existing named db");
        ok &= expect(error.empty(), "schema helper keeps error empty on successful drop");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn();
        std::string error;
        ok &= expect(!db::tryDropNamedDbi(*backend, txn, eventsDbi, &error),
                     "schema helper reports false when named db is missing");
        ok &= expect(!error.empty(), "schema helper reports error string when drop fails");
    }

    const auto legacyRoom = std::string("!legacy:example");
    {
        auto txn    = backend->beginTxn();
        auto legacy = db::openRoomDbi(*backend, txn, legacyRoom, db::catalog::RoomDb::LegacyStateByKey);
        ok &= expect(legacy.put(txn, "m.room.member", R"({"key":"@alice:example","id":"$member"})"),
                     "schema helper setup inserts legacy state-by-key payload");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn();
        std::string error;
        const bool migrated = db::migrateLegacyStateByKeyToStatesKey(*backend, txn, legacyRoom, &error);
        ok &= expect(migrated, "schema helper migrates legacy state-by-key db");
        ok &= expect(error.empty(), "schema helper leaves error empty on state-by-key success");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto statesKey =
          db::openRoomDbi(*backend, txn, legacyRoom, db::catalog::RoomDb::StatesKey, false);
        std::string_view value;
        ok &= expect(statesKey.get(txn, "m.room.member", value),
                     "schema helper migrates state-by-key entry into states_key db");
        const auto [stateKey, eventId] = db::catalog::splitStateEventIndexValue(value);
        ok &= expect(stateKey == "@alice:example",
                     "schema helper preserves migrated state key in states_key payload");
        ok &= expect(eventId == "$member",
                     "schema helper preserves migrated event id in states_key payload");

        ok &= expectDbError(
          [&] { db::openRoomDbi(*backend, txn, legacyRoom, db::catalog::RoomDb::LegacyStateByKey, false); },
          "schema helper drops legacy state-by-key db after migration");
    }

    const auto brokenRoom = std::string("!broken:example");
    {
        auto txn    = backend->beginTxn();
        auto legacy = db::openRoomDbi(*backend, txn, brokenRoom, db::catalog::RoomDb::LegacyStateByKey);
        ok &= expect(legacy.put(txn, "m.room.member", "{not-json"),
                     "schema helper setup inserts invalid legacy payload");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn();
        std::string error;
        ok &= expect(!db::migrateLegacyStateByKeyToStatesKey(*backend, txn, brokenRoom, &error),
                     "schema helper reports false for invalid state-by-key payload");
        ok &= expect(!error.empty(), "schema helper provides error text on state-by-key failure");
    }

    const auto legacyMegolmKey = std::string(R"({"room_id":"!room:example","sender_key":"curve","session_id":"sid"})");
    const auto migratedMegolmKey = std::string(R"({"room_id":"!room:example","session_id":"sid"})");
    {
        auto txn     = backend->beginTxn();
        auto inbound = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::InboundMegolmSessions);
        auto outbound =
          db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::OutboundMegolmSessions);
        auto data = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::MegolmSessionsData);

        ok &= expect(inbound.put(txn, legacyMegolmKey, "pickle"),
                     "schema helper setup inserts legacy inbound megolm session");
        ok &= expect(data.put(txn, legacyMegolmKey, R"({"ts":1})"),
                     "schema helper setup inserts legacy megolm metadata");
        ok &= expect(outbound.put(txn, "old", "outbound"),
                     "schema helper setup inserts outbound megolm session");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn();
        std::string error;
        ok &= expect(db::migrateLegacyMegolmSessionIndexes(*backend, txn, &error),
                     "schema helper migrates legacy megolm index keys");
        ok &= expect(error.empty(), "schema helper leaves error empty on megolm index migration");
        txn.commit();
    }

    {
        auto txn      = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto inbound  = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::InboundMegolmSessions, false);
        auto outbound =
          db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::OutboundMegolmSessions, false);
        auto data = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::MegolmSessionsData, false);

        std::string_view value;
        ok &= expect(inbound.get(txn, migratedMegolmKey, value),
                     "schema helper writes migrated inbound megolm session");
        ok &= expect(value == "pickle", "schema helper preserves inbound megolm pickle payload");
        ok &= expect(!inbound.get(txn, legacyMegolmKey, value),
                     "schema helper removes legacy inbound megolm key shape");

        ok &= expect(data.get(txn, migratedMegolmKey, value),
                     "schema helper writes migrated megolm metadata");
        const auto parsedData = nlohmann::json::parse(value);
        ok &= expect(parsedData.value("sender_key", "") == "curve",
                     "schema helper moves sender_key into megolm metadata payload");

        ok &= expect(outbound.size(txn) == 0,
                     "schema helper clears outbound megolm sessions during migration");
    }

    {
        auto txn     = backend->beginTxn();
        auto inbound = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::InboundMegolmSessions);
        ok &= expect(inbound.put(txn, "{bad-json", "pickle"),
                     "schema helper setup inserts invalid megolm key payload");
        std::string error;
        ok &= expect(!db::migrateLegacyMegolmSessionIndexes(*backend, txn, &error),
                     "schema helper reports false for invalid megolm key payload");
        ok &= expect(!error.empty(), "schema helper provides error text on megolm migration failure");
    }

    backend->close();
    return ok;
}

bool
testLegacyOlmMigrationHelpers()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 64;
    backend->open("", options);
    const std::string v2Payload = R"({"s":"pickle-2","ts":7})";

    const auto v1Name = std::string("olm_sessions/curve-1");
    {
        auto txn  = backend->beginTxn();
        auto oldV1 = db::openNamedDbi(*backend, txn, v1Name);
        ok &= expect(oldV1.put(txn, "sess-ok", "pickle-ok"), "legacy olm v1 setup puts printable value");
        ok &= expect(oldV1.put(txn, "sess-bad", std::string("bad\1value", 9)),
                     "legacy olm v1 setup puts non-printable value");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn();
        db::migrateLegacyOlmShardsV1ToV2(*backend, txn);
        txn.commit();
    }

    const auto v2Name = db::catalog::legacyOlmShardV2NameFromV1(v1Name);
    {
        auto txn    = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto newV2  = db::openNamedDbi(*backend, txn, v2Name, false);
        std::string_view value;
        ok &= expect(newV2.get(txn, "sess-ok", value), "legacy olm v1->v2 migration keeps printable session");
        ok &= expect(value.size() > 0, "legacy olm v1->v2 migration stores non-empty payload");
        ok &= expect(!newV2.get(txn, "sess-bad", value),
                     "legacy olm v1->v2 migration drops non-printable sessions");

        ok &= expectDbError([&] { db::openNamedDbi(*backend, txn, v1Name, false); },
                            "legacy olm v1 db is dropped after migration");
    }

    {
        auto txn     = backend->beginTxn();
        auto olmV2Db = db::openNamedDbi(*backend, txn, "olm_sessions.v2/curve-2");
        ok &= expect(olmV2Db.put(txn, "sess-2", v2Payload),
                     "legacy olm v2 setup puts migrated-style payload");

        auto unified = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::OlmSessions);
        ok &= expect(db::migrateLegacyOlmShardsV2ToUnified(*backend, txn, unified),
                     "legacy olm v2->v3 helper reports migration happened");
        txn.commit();
    }

    {
        auto txn      = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto unified  = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::OlmSessions, false);
        std::string_view value;
        ok &= expect(unified.get(txn, db::catalog::olmSessionKey("curve-2", "sess-2"), value),
                     "legacy olm v2->v3 helper migrates into unified db");
        ok &= expect(value.size() > 0, "legacy olm v2->v3 helper preserves non-empty payload");

        ok &= expectDbError([&] { db::openNamedDbi(*backend, txn, "olm_sessions.v2/curve-2", false); },
                            "legacy olm v2 shard is dropped after migration");
    }

    {
        auto txn     = backend->beginTxn();
        auto unified = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::OlmSessions);
        ok &= expect(!db::migrateLegacyOlmShardsV2ToUnified(*backend, txn, unified),
                     "legacy olm v2->v3 helper reports no-op when no shards exist");
        txn.commit();
    }

    backend->close();
    return ok;
}

bool
testCursorAndOrderingContract(db::Backend &backend, std::string_view backendId)
{
    bool ok = true;

    auto testName = [&](std::string_view name) {
        return std::string(backendId) + ": " + std::string(name);
    };

    const auto dupDbName = std::string(backendId) + "_dupsort_contract";
    {
        auto txn = backend.beginTxn();
        auto dbi = backend.openDbi(
          txn, dupDbName, openOptions(db::DbiFlags::Create | db::DbiFlags::DupSort));
        ok &= expect(dbi.put(txn, "k", "b"), testName("dupsort put #1"));
        ok &= expect(dbi.put(txn, "k", "a"), testName("dupsort put #2"));
        ok &= expect(dbi.put(txn, "k", "c"), testName("dupsort put #3"));
        ok &= expect(dbi.put(txn, "z", "zz"), testName("dupsort put #4"));
        txn.commit();
    }

    {
        auto txn = backend.beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto dbi = backend.openDbi(txn, dupDbName, openOptions(db::DbiFlags::DupSort));

        auto cursor = db::Cursor::open(txn, dbi);
        std::string_view key = "k", value;
        ok &= expect(cursor.get(key, value, db::CursorOp::Set), testName("cursor Set"));
        ok &= expect(key == "k", testName("cursor Set key"));
        ok &= expect(value == "a", testName("cursor Set returns first sorted dup value"));

        ok &= expect(cursor.get(key, value, db::CursorOp::NextDup), testName("cursor NextDup #1"));
        ok &= expect(value == "b", testName("cursor NextDup #1 value"));
        ok &= expect(cursor.get(key, value, db::CursorOp::NextDup), testName("cursor NextDup #2"));
        ok &= expect(value == "c", testName("cursor NextDup #2 value"));
        ok &= expect(!cursor.get(key, value, db::CursorOp::NextDup),
                     testName("cursor NextDup at end returns false"));

        ok &= expect(cursor.get(key, value, db::CursorOp::Set), testName("cursor Set before NextNoDup"));
        ok &= expect(cursor.get(key, value, db::CursorOp::NextNoDup), testName("cursor NextNoDup"));
        ok &= expect(key == "z", testName("cursor NextNoDup key"));
        ok &= expect(value == "zz", testName("cursor NextNoDup value"));

        key = "m";
        ok &= expect(cursor.get(key, value, db::CursorOp::SetRange), testName("cursor SetRange"));
        ok &= expect(key == "z", testName("cursor SetRange key"));
    }

    const auto intDbName = std::string(backendId) + "_integer_key_contract";
    {
        auto txn = backend.beginTxn();
        auto dbi = backend.openDbi(
          txn, intDbName, openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        ok &= expect(dbi.put(txn, integerKey(5), "five"), testName("integer-key put #1"));
        ok &= expect(dbi.put(txn, integerKey(1), "one"), testName("integer-key put #2"));
        ok &= expect(dbi.put(txn, integerKey(3), "three"), testName("integer-key put #3"));
        txn.commit();
    }

    {
        auto txn = backend.beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto dbi = backend.openDbi(txn, intDbName, openOptions(db::DbiFlags::IntegerKey));

        auto cursor = db::Cursor::open(txn, dbi);
        std::string_view key, value;
        ok &= expect(cursor.get(key, value, db::CursorOp::First), testName("integer-key cursor First"));
        ok &= expect(readIntegerKey(key) == 1, testName("integer-key first key is smallest"));

        ok &= expect(cursor.get(key, value, db::CursorOp::Next), testName("integer-key cursor Next #1"));
        ok &= expect(readIntegerKey(key) == 3, testName("integer-key second key"));

        ok &= expect(cursor.get(key, value, db::CursorOp::Next), testName("integer-key cursor Next #2"));
        ok &= expect(readIntegerKey(key) == 5, testName("integer-key third key"));
    }

    return ok;
}

bool
testOpenHelpers()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
    backend->open("", options);

    {
        auto txn = backend->beginTxn();
        auto dbi =
          db::openRoomDbi(*backend, txn, "!room:example", db::catalog::RoomDb::EventOrder);
        ok &= expect(dbi.put(txn, integerKey(7), "seven"), "openRoomDbi puts integer key #1");
        ok &= expect(dbi.put(txn, integerKey(1), "one"), "openRoomDbi puts integer key #2");
        ok &= expect(dbi.put(txn, integerKey(4), "four"), "openRoomDbi puts integer key #3");

        auto spaces = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::SpacesChildren);
        ok &= expect(spaces.put(txn, "space", "child-z"), "openGlobalDbi dupsort put #1");
        ok &= expect(spaces.put(txn, "space", "child-a"), "openGlobalDbi dupsort put #2");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto dbi =
          db::openRoomDbi(*backend, txn, "!room:example", db::catalog::RoomDb::EventOrder, false);
        auto cursor = db::Cursor::open(txn, dbi);

        std::string_view key, value;
        ok &= expect(cursor.get(key, value, db::CursorOp::First), "openRoomDbi cursor first");
        ok &= expect(readIntegerKey(key) == 1,
                     "openRoomDbi applies IntegerKey policy for /event_order");

        auto spaces = db::openGlobalDbi(*backend,
                                        txn,
                                        db::catalog::GlobalDb::SpacesChildren,
                                        false);
        auto spacesCursor = db::Cursor::open(txn, spaces);
        std::string_view spacesKey = "space", spacesValue;
        ok &= expect(spacesCursor.get(spacesKey, spacesValue, db::CursorOp::Set),
                     "openGlobalDbi cursor Set on DupSort db");
        ok &= expect(spacesValue == "child-a", "openGlobalDbi applies DupSort policy");
    }

    backend->close();
    return ok;
}

bool
testCompactionHelper()
{
    bool ok = true;

    auto from                  = db::createBackend(db::kMemoryBackendId);
    auto to                    = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
    from->open("", options);
    to->open("", options);

    {
        auto txn      = from->beginTxn();
        auto intDb    = db::openRoomDbi(*from, txn, "!room:example", db::catalog::RoomDb::EventOrder);
        auto dupsortDb = db::openGlobalDbi(*from, txn, db::catalog::GlobalDb::SpacesChildren);

        ok &= expect(intDb.put(txn, integerKey(9), "nine"), "compaction source integer put #1");
        ok &= expect(intDb.put(txn, integerKey(2), "two"), "compaction source integer put #2");
        ok &= expect(dupsortDb.put(txn, "space", "child-z"), "compaction source dupsort put #1");
        ok &= expect(dupsortDb.put(txn, "space", "child-a"), "compaction source dupsort put #2");
        txn.commit();
    }

    db::compact(*from, *to);

    {
        auto txn      = to->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto intDb    = db::openRoomDbi(*to, txn, "!room:example", db::catalog::RoomDb::EventOrder, false);
        auto dupsortDb = db::openGlobalDbi(*to, txn, db::catalog::GlobalDb::SpacesChildren, false);

        auto intCursor = db::Cursor::open(txn, intDb);
        std::string_view key, value;
        ok &= expect(intCursor.get(key, value, db::CursorOp::First),
                     "compaction destination integer cursor first");
        ok &= expect(readIntegerKey(key) == 2, "compaction preserves IntegerKey policy");

        auto dupCursor = db::Cursor::open(txn, dupsortDb);
        std::string_view dupKey = "space", dupValue;
        ok &= expect(dupCursor.get(dupKey, dupValue, db::CursorOp::Set),
                     "compaction destination dupsort cursor set");
        ok &= expect(dupValue == "child-a", "compaction preserves DupSort policy");
    }

    from->close();
    to->close();
    return ok;
}

bool
testStateIndexHelper()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
    backend->open("", options);

    {
        auto txn = backend->beginTxn();
        auto statesKeyDb =
          db::openRoomDbi(*backend, txn, "!room:example", db::catalog::RoomDb::StatesKey);
        ok &= expect(statesKeyDb.put(
                       txn, "m.room.member", db::catalog::stateEventIndexValue("zeta", "$event-z")),
                     "state index helper setup writes member index entry #1");
        ok &= expect(statesKeyDb.put(
                       txn, "m.room.member", db::catalog::stateEventIndexValue("alpha", "$event-a")),
                     "state index helper setup writes member index entry #2");
        ok &= expect(statesKeyDb.put(txn, "m.room.member", "broken-value-without-separator"),
                     "state index helper setup writes malformed member index entry");
        ok &= expect(statesKeyDb.put(
                       txn, "m.room.topic", db::catalog::stateEventIndexValue("", "$topic")),
                     "state index helper setup writes topic index entry");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto statesKeyDb =
          db::openRoomDbi(*backend, txn, "!room:example", db::catalog::RoomDb::StatesKey, false);

        const auto memberEventId =
          db::findStateEventId(txn, statesKeyDb, "m.room.member", "alpha");
        ok &= expect(memberEventId.has_value(),
                     "state index helper finds event id by event type + state key");
        ok &= expect(memberEventId.value_or("") == "$event-a",
                     "state index helper returns matching event id");

        const auto missingState = db::findStateEventId(txn, statesKeyDb, "m.room.member", "missing");
        ok &= expect(!missingState.has_value(),
                     "state index helper returns no value for missing state key");

        const auto malformedState =
          db::findStateEventId(txn, statesKeyDb, "m.room.member", "broken-value-without-separator");
        ok &= expect(!malformedState.has_value(),
                     "state index helper ignores malformed entries with no event id");

        const auto memberIds = db::listStateEventIds(txn, statesKeyDb, "m.room.member");
        ok &= expect(memberIds.size() == 2,
                     "state index helper lists only entries with valid event ids");
        ok &= expect(memberIds.size() >= 2 && memberIds[0] == "$event-a",
                     "state index helper listing preserves state-key ordering #1");
        ok &= expect(memberIds.size() >= 2 && memberIds[1] == "$event-z",
                     "state index helper listing preserves state-key ordering #2");

        const auto topicIds = db::listStateEventIds(txn, statesKeyDb, "m.room.topic");
        ok &= expect(topicIds.size() == 1 && topicIds.front() == "$topic",
                     "state index helper lists event ids for different type");
    }

    {
        auto txn = backend->beginTxn();
        auto statesKeyDb =
          db::openRoomDbi(*backend, txn, "!room:example", db::catalog::RoomDb::StatesKey, false);

        ok &= expect(db::removeStateEventId(
                       txn, statesKeyDb, "m.room.member", "alpha", "$event-a"),
                     "state index helper removes exact state index entry");

        db::putStateEventId(txn, statesKeyDb, "m.room.member", "beta", "$event-b");
        db::putStateEventId(txn, statesKeyDb, "m.room.member", "beta", "$event-b");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto statesKeyDb =
          db::openRoomDbi(*backend, txn, "!room:example", db::catalog::RoomDb::StatesKey, false);

        const auto memberIds = db::listStateEventIds(txn, statesKeyDb, "m.room.member");
        ok &= expect(memberIds.size() == 2,
                     "state index helper write API keeps state index set deduplicated");
        ok &= expect(memberIds.size() >= 2 && memberIds[0] == "$event-b",
                     "state index helper write API keeps state-key ordering after replace #1");
        ok &= expect(memberIds.size() >= 2 && memberIds[1] == "$event-z",
                     "state index helper write API keeps state-key ordering after replace #2");
    }

    backend->close();
    return ok;
}

bool
testSyncStateHelper()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
    backend->open("", options);

    {
        auto txn = backend->beginTxn();
        auto syncStateDb = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::SyncState);

        db::putSyncStateValue(txn, syncStateDb, db::catalog::SyncStateKey::NextBatch, "batch-1");
        db::putSyncStateSecretValue(txn, syncStateDb, "pickle_secret", "encrypted-pickle");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto syncStateDb =
          db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::SyncState, false);

        std::string_view nextBatch;
        ok &= expect(db::getSyncStateValue(
                       txn, syncStateDb, db::catalog::SyncStateKey::NextBatch, nextBatch),
                     "sync state helper reads enum-keyed value");
        ok &= expect(nextBatch == "batch-1", "sync state helper returns enum-keyed value");

        const auto nextBatchCopy =
          db::getSyncStateValue(txn, syncStateDb, db::catalog::SyncStateKey::NextBatch);
        ok &= expect(nextBatchCopy.has_value() && *nextBatchCopy == "batch-1",
                     "sync state helper optional getter returns enum-keyed value");

        std::string_view secretValue;
        ok &= expect(db::getSyncStateSecretValue(txn, syncStateDb, "pickle_secret", secretValue),
                     "sync state helper reads secret-keyed value");
        ok &= expect(secretValue == "encrypted-pickle",
                     "sync state helper returns secret-keyed value");

        const auto secretCopy = db::getSyncStateSecretValue(txn, syncStateDb, "pickle_secret");
        ok &= expect(secretCopy.has_value() && *secretCopy == "encrypted-pickle",
                     "sync state helper optional getter returns secret-keyed value");
    }

    {
        auto txn = backend->beginTxn();
        auto syncStateDb = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::SyncState, false);

        ok &= expect(db::removeSyncStateValue(txn, syncStateDb, db::catalog::SyncStateKey::NextBatch),
                     "sync state helper removes enum-keyed value");
        ok &= expect(db::removeSyncStateSecretValue(txn, syncStateDb, "pickle_secret"),
                     "sync state helper removes secret-keyed value");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto syncStateDb =
          db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::SyncState, false);

        ok &= expect(
          !db::getSyncStateValue(txn, syncStateDb, db::catalog::SyncStateKey::NextBatch).has_value(),
          "sync state helper reports missing enum-keyed value");
        ok &= expect(!db::getSyncStateSecretValue(txn, syncStateDb, "pickle_secret").has_value(),
                     "sync state helper reports missing secret-keyed value");
    }

    backend->close();
    return ok;
}

bool
testMegolmIndexHelper()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
    backend->open("", options);

    {
        auto txn      = backend->beginTxn();
        auto inboundDb = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::InboundMegolmSessions);
        auto dataDb    = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::MegolmSessionsData);

        db::putInboundMegolmSessionValue(
          txn, inboundDb, "!room-a:example", "sess-1", "pickled-1");
        db::putMegolmSessionDataValue(
          txn, dataDb, "!room-a:example", "sess-1", R"({"message_index":1})");
        txn.commit();
    }

    {
        auto txn      = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto inboundDb = db::openGlobalDbi(
          *backend, txn, db::catalog::GlobalDb::InboundMegolmSessions, false);
        auto dataDb =
          db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::MegolmSessionsData, false);

        std::string_view value;
        ok &= expect(db::getInboundMegolmSessionValue(
                       txn, inboundDb, "!room-a:example", "sess-1", value),
                     "megolm index helper reads inbound session payload");
        ok &= expect(value == "pickled-1",
                     "megolm index helper returns inbound session payload");

        ok &= expect(
          db::getMegolmSessionDataValue(txn, dataDb, "!room-a:example", "sess-1", value),
          "megolm index helper reads megolm session data payload");
        ok &= expect(value == R"({"message_index":1})",
                     "megolm index helper returns megolm session data payload");

        ok &= expect(!db::getInboundMegolmSessionValue(
                       txn, inboundDb, "!room-a:example", "missing", value),
                     "megolm index helper reports missing inbound payload");
    }

    {
        const auto key = db::megolmSessionKey("!room-a:example", "sess-1");
        std::string roomId;
        std::string sessionId;
        ok &= expect(db::parseMegolmSessionKey(key, roomId, sessionId),
                     "megolm index helper parses serialized key");
        ok &= expect(roomId == "!room-a:example" && sessionId == "sess-1",
                     "megolm index helper preserves parsed room/session ids");
        ok &= expect(!db::parseMegolmSessionKey("not-json", roomId, sessionId),
                     "megolm index helper rejects malformed key");
    }

    backend->close();
    return ok;
}

bool
testReadReceiptIndexHelper()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
    backend->open("", options);

    {
        auto txn  = backend->beginTxn();
        auto dbi  = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::ReadReceipts);
        db::putReadReceiptValue(
          txn, dbi, "$event-1", "!room:example", R"({"@alice:example.org":123})");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto dbi = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::ReadReceipts, false);

        std::string_view value;
        ok &= expect(
          db::getReadReceiptValue(txn, dbi, "$event-1", "!room:example", value),
          "read receipt helper reads event+room keyed payload");
        ok &= expect(value == R"({"@alice:example.org":123})",
                     "read receipt helper returns event+room keyed payload");

        ok &= expect(
          !db::getReadReceiptValue(txn, dbi, "$event-2", "!room:example", value),
          "read receipt helper reports missing event+room key");
    }

    {
        const auto key = db::readReceiptKey("$event-1", "!room:example");
        ok &= expect(key.find("\"event_id\":\"$event-1\"") != std::string::npos &&
                       key.find("\"room_id\":\"!room:example\"") != std::string::npos,
                     "read receipt helper serializes event/room key fields");
    }

    backend->close();
    return ok;
}

bool
testRoomInfoHelper()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
    backend->open("", options);

    RoomInfo info;
    info.name                             = "Room";
    info.topic                            = "Topic";
    info.avatar_url                       = "mxc://example/avatar";
    info.version                          = "11";
    info.is_invite                        = false;
    info.is_space                         = true;
    info.is_tombstoned                    = false;
    info.member_count                     = 42;
    info.approximate_last_modification_ts = 1234567890123ULL;
    info.notification_count               = 9;
    info.highlight_count                  = 2;
    info.tags                             = {"m.favourite", "u.work"};

    {
        auto txn = backend->beginTxn();
        auto dbi = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::Rooms);
        db::putRoomInfo(txn, dbi, "!room:example", info);
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto dbi = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::Rooms, false);

        RoomInfo loaded;
        ok &= expect(db::getRoomInfo(txn, dbi, "!room:example", loaded),
                     "room info helper reads room info by id");
        ok &= expect(loaded.name == "Room" && loaded.topic == "Topic" &&
                       loaded.avatar_url == "mxc://example/avatar" && loaded.version == "11",
                     "room info helper preserves core room fields");
        ok &= expect(loaded.is_space && !loaded.is_invite && !loaded.is_tombstoned,
                     "room info helper preserves boolean room flags");
        ok &= expect(loaded.tags.size() == 2 && loaded.tags[0] == "m.favourite" &&
                       loaded.tags[1] == "u.work",
                     "room info helper preserves room tags");

        const auto optionalLoaded = db::getRoomInfo(txn, dbi, "!room:example");
        ok &= expect(optionalLoaded.has_value() && optionalLoaded->notification_count == 9 &&
                       optionalLoaded->highlight_count == 2,
                     "room info helper optional getter preserves notification counters");
        ok &= expect(!db::getRoomInfo(txn, dbi, "!missing:example").has_value(),
                     "room info helper optional getter reports missing room");
    }

    {
        const auto serialized = db::serializeRoomInfo(info);
        const auto parsed     = db::parseRoomInfo(serialized);
        ok &= expect(parsed.name == info.name && parsed.topic == info.topic &&
                           parsed.approximate_last_modification_ts ==
                             info.approximate_last_modification_ts,
                     "room info helper parse/serialize roundtrip preserves key fields");
    }

    {
        bool parseError = false;
        try {
            (void)db::parseRoomInfo("{bad-json");
        } catch (const std::exception &) {
            parseError = true;
        }
        ok &= expect(parseError, "room info helper propagates on malformed payload");
    }

    backend->close();
    return ok;
}

bool
testMemberInfoHelper()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
    backend->open("", options);

    MemberInfo info{
      .name       = "Alice",
      .avatar_url = "mxc://example/alice",
      .inviter    = "@bob:example.org",
      .reason     = "Join us",
      .is_direct  = true,
    };

    {
        auto txn = backend->beginTxn();
        auto dbi = backend->openDbi(txn, "members", openOptions(db::DbiFlags::Create));
        db::putMemberInfo(txn, dbi, "@alice:example.org", info);
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto dbi = backend->openDbi(txn, "members");

        MemberInfo loaded;
        ok &= expect(db::getMemberInfo(txn, dbi, "@alice:example.org", loaded),
                     "member info helper reads user member record");
        ok &= expect(loaded.name == "Alice" && loaded.avatar_url == "mxc://example/alice" &&
                       loaded.inviter == "@bob:example.org" && loaded.reason == "Join us" &&
                       loaded.is_direct,
                     "member info helper preserves all member fields");

        const auto optionalLoaded = db::getMemberInfo(txn, dbi, "@alice:example.org");
        ok &= expect(optionalLoaded.has_value() && optionalLoaded->name == "Alice",
                     "member info helper optional getter returns member record");
        ok &= expect(!db::getMemberInfo(txn, dbi, "@missing:example.org").has_value(),
                     "member info helper optional getter reports missing user");
    }

    {
        const auto serialized = db::serializeMemberInfo(info);
        const auto parsed     = db::parseMemberInfo(serialized);
        ok &= expect(parsed.name == info.name && parsed.avatar_url == info.avatar_url &&
                       parsed.inviter == info.inviter && parsed.reason == info.reason &&
                       parsed.is_direct == info.is_direct,
                     "member info helper parse/serialize roundtrip preserves fields");
    }

    {
        bool parseError = false;
        try {
            (void)db::parseMemberInfo("{bad-json");
        } catch (const std::exception &) {
            parseError = true;
        }
        ok &= expect(parseError, "member info helper propagates on malformed payload");
    }

    backend->close();
    return ok;
}

bool
testJsonHelpers()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 16;
    backend->open("", options);

    std::vector<int> numbers{1, 2, 3, 5};

    {
        auto txn   = backend->beginTxn();
        auto jsonDb = backend->openDbi(txn, "json", openOptions(db::DbiFlags::Create));
        db::putJsonValue(txn, jsonDb, "numbers", numbers);
        txn.commit();
    }

    {
        auto txn   = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto jsonDb = backend->openDbi(txn, "json");

        const auto parsed = db::getJsonValue<std::vector<int>>(txn, jsonDb, "numbers");
        ok &= expect(parsed.has_value(), "json helper returns value for existing key");
        ok &= expect(*parsed == numbers, "json helper decodes stored vector values");

        std::vector<int> restored;
        ok &= expect(db::getJsonValue(txn, jsonDb, "numbers", restored),
                     "json helper output overload returns value");
        ok &= expect(restored == numbers, "json helper output overload preserves vector values");

        const auto missing = db::getJsonValue<std::vector<int>>(txn, jsonDb, "missing");
        ok &= expect(!missing.has_value(), "json helper returns empty optional for missing key");

        const auto missingOutResult = db::getJsonValue(txn, jsonDb, "missing", restored);
        ok &= expect(!missingOutResult, "json helper out-parameter returns false for missing key");
    }

    {
        auto txn    = backend->beginTxn();
        auto jsonDb = backend->openDbi(txn, "json", openOptions(db::DbiFlags::Create));
        jsonDb.put(txn, "bad", "{bad-json");
        txn.commit();
    }

    {
        auto txn   = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto jsonDb = backend->openDbi(txn, "json");

        bool parseError = false;
        try {
            (void)db::getJsonValue<std::vector<int>>(txn, jsonDb, "bad");
        } catch (const nlohmann::json::parse_error &) {
            parseError = true;
        } catch (const std::exception &e) {
            std::cerr << "FAILED: json helper does not propagate parse_error as expected ("
                      << e.what() << ")\n";
        }

        ok &= expect(parseError, "json helper propagates parse_error for malformed payloads");
    }

    {
        const auto goodVec = db::parseJsonValue<std::vector<int>>(R"([1,2,3])");
        ok &= expect(goodVec.has_value() && *goodVec == std::vector<int>({1, 2, 3}),
                     "json parse helper parses valid json payloads");

        const auto malformedVec = db::parseJsonValue<std::vector<int>>("{bad-json");
        ok &= expect(!malformedVec.has_value(),
                     "json parse helper returns empty optional for malformed payload");

        std::vector<int> parsed;
        ok &= expect(db::parseJsonValue<std::vector<int>>(R"([4,5])", parsed) &&
                         parsed == std::vector<int>({4, 5}),
                     "json parse helper output overload parses valid payload");

        ok &= expect(!db::parseJsonValue<std::vector<int>>(R"({"oops":true})", parsed),
                     "json parse helper output overload returns false for malformed payload");
    }

    backend->close();
    return ok;
}

bool
testCacheCryptoHelpers()
{
    bool ok = true;

    UserKeyCache userKeys;
    userKeys.updated_at         = "up";
    userKeys.last_changed       = "lc";
    userKeys.master_key_changed = true;
    userKeys.seen_device_ids.insert("deviceA");
    userKeys.seen_device_keys.insert("keyA");

    const auto userKeysSerialized = nlohmann::json(userKeys).dump();
    const auto userKeysParsed    = nlohmann::json::parse(userKeysSerialized).get<UserKeyCache>();
    ok &= expect(userKeysParsed.updated_at == userKeys.updated_at &&
                   userKeysParsed.last_changed == userKeys.last_changed &&
                   userKeysParsed.master_key_changed == userKeys.master_key_changed &&
                   userKeysParsed.seen_device_ids == userKeys.seen_device_ids &&
                   userKeysParsed.seen_device_keys == userKeys.seen_device_keys,
                 "cache crypto helper preserves UserKeyCache fields");

    VerificationCache verification;
    verification.device_verified = {"a"};
    verification.device_blocked  = {"b"};
    const auto verificationSerialized = nlohmann::json(verification).dump();
    const auto verificationParsed =
      nlohmann::json::parse(verificationSerialized).get<VerificationCache>();
    ok &= expect(verificationParsed.device_verified == verification.device_verified &&
                   verificationParsed.device_blocked == verification.device_blocked,
                 "cache crypto helper preserves VerificationCache fields");

    OnlineBackupVersion backup;
    backup.version   = "1";
    backup.algorithm = "m.megolm_backup.v1.curve25519-aes-sha2";
    const auto backupSerialized = nlohmann::json(backup).dump();
    const auto backupParsed     = nlohmann::json::parse(backupSerialized).get<OnlineBackupVersion>();
    ok &= expect(backupParsed.version == backup.version &&
                   backupParsed.algorithm == backup.algorithm,
                 "cache crypto helper preserves OnlineBackupVersion fields");

    DeviceKeysToMsgIndex keys;
    keys.deviceids = {{"ed25519:key1", 7}, {"curve25519:key2", 12}};
    const auto keysSerialized = nlohmann::json(keys).dump();
    const auto keysParsed     = nlohmann::json::parse(keysSerialized).get<DeviceKeysToMsgIndex>();
    ok &= expect(keysParsed.deviceids == keys.deviceids,
                 "cache crypto helper preserves DeviceKeysToMsgIndex fields");

    SharedWithUsers recipients;
    DeviceKeysToMsgIndex keysForBob;
    keysForBob.deviceids = {{"ed25519:key3", 1}};
    recipients.keys = {
      {"@alice:example.org", keys},
      {"@bob:example.org", keysForBob},
    };
    const auto recipientsSerialized = nlohmann::json(recipients).dump();
    const auto recipientsParsed =
      nlohmann::json::parse(recipientsSerialized).get<SharedWithUsers>();
    ok &= expect(recipientsParsed.keys.size() == recipients.keys.size(),
                 "cache crypto helper preserves SharedWithUsers map size");

    GroupSessionData sessionData;
    sessionData.message_index = 42;
    sessionData.timestamp     = 123456789;
    sessionData.trusted       = true;
    sessionData.sender_key    = "s";
    sessionData.sender_claimed_ed25519_key =
      "mXCnK9qFj8qY..."; // ensure value is non-empty for parser roundtrip coverage
    sessionData.forwarding_curve25519_key_chain = {"fwd-1", "fwd-2"};
    sessionData.currently.keys                = recipients.keys;
    sessionData.indices                      = {{1, "evt-1"}, {2, "evt-2"}};
    const auto sessionSerialized = nlohmann::json(sessionData).dump();
    const auto sessionParsed    = nlohmann::json::parse(sessionSerialized).get<GroupSessionData>();
    ok &= expect(sessionParsed.message_index == sessionData.message_index &&
                   sessionParsed.timestamp == sessionData.timestamp &&
                   sessionParsed.trusted == sessionData.trusted &&
                   sessionParsed.sender_key == sessionData.sender_key &&
                   sessionParsed.indices == sessionData.indices &&
                   sessionParsed.currently.keys == sessionData.currently.keys,
                 "cache crypto helper preserves GroupSessionData fields");

    DevicePublicKeys pubKey{"ed", "curve"};
    const auto pubSerialized = nlohmann::json(pubKey).dump();
    const auto pubParsed     = nlohmann::json::parse(pubSerialized).get<DevicePublicKeys>();
    ok &= expect(pubParsed.ed25519 == pubKey.ed25519 && pubParsed.curve25519 == pubKey.curve25519,
                 "cache crypto helper preserves DevicePublicKeys fields");

    StoredOlmSession olmSession;
    olmSession.last_message_ts = 100;
    olmSession.pickled_session = "pickled";
    const auto olmSerialized = nlohmann::json(olmSession).dump();
    const auto olmParsed     = nlohmann::json::parse(olmSerialized).get<StoredOlmSession>();
    ok &= expect(olmParsed.last_message_ts == olmSession.last_message_ts &&
                   olmParsed.pickled_session == olmSession.pickled_session,
                 "cache crypto helper preserves StoredOlmSession fields");

    return ok;
}

bool
testOlmSessionIndexHelper()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
    backend->open("", options);

    {
        auto txn = backend->beginTxn();
        auto olmSessionsDb = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::OlmSessions);

        db::putOlmSessionValue(txn, olmSessionsDb, "curve-a", "sess-2", R"({"ts":2})");
        db::putOlmSessionValue(txn, olmSessionsDb, "curve-a", "sess-1", R"({"ts":1})");
        db::putOlmSessionValue(txn, olmSessionsDb, "curve-b", "sess-0", R"({"ts":0})");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto olmSessionsDb =
          db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::OlmSessions, false);

        std::string_view value;
        ok &= expect(db::getOlmSessionValue(txn, olmSessionsDb, "curve-a", "sess-1", value),
                     "olm session helper reads curve+session entry");
        ok &= expect(value == R"({"ts":1})", "olm session helper returns expected value");

        ok &= expect(!db::getOlmSessionValue(txn, olmSessionsDb, "curve-a", "missing", value),
                     "olm session helper reports missing session id");

        const auto ids = db::listOlmSessionIds(txn, olmSessionsDb, "curve-a");
        ok &= expect(ids.size() == 2, "olm session helper lists session ids per curve");
        ok &= expect(ids.size() >= 2 && ids[0] == "sess-1",
                     "olm session helper list preserves key ordering #1");
        ok &= expect(ids.size() >= 2 && ids[1] == "sess-2",
                     "olm session helper list preserves key ordering #2");

        std::vector<std::string> seen;
        const auto iterated = db::forEachOlmSessionForCurve(
          txn, olmSessionsDb, "curve-a", [&seen](std::string_view sessionId, std::string_view) {
              seen.emplace_back(sessionId);
              return seen.size() < 2;
          });
        ok &= expect(iterated == 2, "olm session helper counts iterated sessions");
        ok &= expect(seen.size() == 2 && seen[0] == "sess-1" && seen[1] == "sess-2",
                     "olm session helper callback exposes ordered session ids");
    }

    backend->close();
    return ok;
}

bool
testDupIndexHelper()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
    backend->open("", options);

    {
        auto txn = backend->beginTxn();
        auto spaces = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::SpacesChildren);
        ok &= expect(spaces.put(txn, "space", "child-z"),
                     "dup index helper setup writes duplicate value #1");
        ok &= expect(spaces.put(txn, "space", "child-a"),
                     "dup index helper setup writes duplicate value #2");
        ok &= expect(spaces.put(txn, "space", ""),
                     "dup index helper setup writes empty duplicate value");
        ok &= expect(spaces.put(txn, "other", "child-other"),
                     "dup index helper setup writes non-matching key");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto spaces =
          db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::SpacesChildren, false);

        const auto spaceValues = db::listDupValues(txn, spaces, "space");
        ok &= expect(spaceValues.size() == 3,
                     "dup index helper lists all duplicate values for key");
        ok &= expect(spaceValues.size() >= 3 && spaceValues[0].empty(),
                     "dup index helper keeps comparator order #1");
        ok &= expect(spaceValues.size() >= 3 && spaceValues[1] == "child-a",
                     "dup index helper keeps comparator order #2");
        ok &= expect(spaceValues.size() >= 3 && spaceValues[2] == "child-z",
                     "dup index helper keeps comparator order #3");

        const auto missing = db::listDupValues(txn, spaces, "missing");
        ok &= expect(missing.empty(), "dup index helper returns empty list for missing key");

        std::vector<std::string> iteratedValues;
        db::forEachDupValue(txn, spaces, "space", [&iteratedValues](std::string_view value) {
            iteratedValues.emplace_back(value);
            return iteratedValues.size() < 2;
        });
        ok &= expect(iteratedValues.size() == 2,
                     "dup index helper supports callback iteration with early stop");
        ok &= expect(iteratedValues.size() >= 2 && iteratedValues[0].empty(),
                     "dup index callback iteration preserves comparator order #1");
        ok &= expect(iteratedValues.size() >= 2 && iteratedValues[1] == "child-a",
                     "dup index callback iteration preserves comparator order #2");

        bool missingVisited = false;
        db::forEachDupValue(txn, spaces, "missing", [&missingVisited](std::string_view) {
            missingVisited = true;
            return true;
        });
        ok &= expect(!missingVisited, "dup index callback iteration skips missing key");
    }

    {
        auto txn = backend->beginTxn();
        auto spaces = db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::SpacesChildren);

        const std::vector<std::string_view> keys = {"alpha", "beta", "", "alpha"};
        const auto written = db::putDupValueForKeys(txn, spaces, keys, "child-bulk");
        ok &= expect(written == 3, "dup index helper putDupValueForKeys writes non-empty keys");

        const auto rewritten =
          db::replaceDupValueForKeys(txn, spaces, keys, "child-bulk", "child-remap");
        ok &= expect(rewritten == 3,
                     "dup index helper replaceDupValueForKeys rewrites values for non-empty keys");

        const auto rewrittenNoop =
          db::replaceDupValueForKeys(txn, spaces, keys, "child-remap", "child-remap");
        ok &= expect(rewrittenNoop == 0,
                     "dup index helper replaceDupValueForKeys no-ops for same old/new value");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto spaces =
          db::openGlobalDbi(*backend, txn, db::catalog::GlobalDb::SpacesChildren, false);

        const auto alphaValues = db::listDupValues(txn, spaces, "alpha");
        ok &= expect(alphaValues.size() == 2,
                     "dup index helper bulk write keeps duplicate values for repeated keys");
        ok &= expect(alphaValues.size() >= 2 && alphaValues[0] == "child-remap" &&
                       alphaValues[1] == "child-remap",
                     "dup index helper replaceDupValueForKeys rewrites repeated-key values");

        const auto betaValues = db::listDupValues(txn, spaces, "beta");
        ok &= expect(betaValues.size() == 1 && betaValues[0] == "child-remap",
                     "dup index helper replaceDupValueForKeys rewrites single-key value");
    }

    backend->close();
    return ok;
}

bool
testScanHelper()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
    backend->open("", options);

    {
        auto txn  = backend->beginTxn();
        auto main = backend->openDbi(txn, "main", openOptions(db::DbiFlags::Create));
        auto dup  = backend->openDbi(
          txn, "dup", openOptions(db::DbiFlags::Create | db::DbiFlags::DupSort));

        ok &= expect(main.put(txn, "b", "b"), "scan helper setup inserts main entry #1");
        ok &= expect(main.put(txn, "a", "a"), "scan helper setup inserts main entry #2");
        ok &= expect(dup.put(txn, "k", "v2"), "scan helper setup inserts dup entry #1");
        ok &= expect(dup.put(txn, "k", "v1"), "scan helper setup inserts dup entry #2");
        ok &= expect(dup.put(txn, "z", "v3"), "scan helper setup inserts dup entry #3");
        txn.commit();
    }

    {
        auto txn  = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto main = backend->openDbi(txn, "main");
        auto dup  = backend->openDbi(txn, "dup");

        const auto mainKeys = db::listKeys(txn, main);
        ok &= expect(mainKeys.size() == 2, "scan helper lists all keys in simple db");
        ok &= expect(mainKeys.size() >= 2 && mainKeys[0] == "a",
                     "scan helper keeps key order in simple db #1");
        ok &= expect(mainKeys.size() >= 2 && mainKeys[1] == "b",
                     "scan helper keeps key order in simple db #2");

        const auto uniqueMainKeys = db::listUniqueKeys(txn, main);
        ok &= expect(uniqueMainKeys.size() == 2, "scan helper lists unique keys in simple db");
        ok &= expect(uniqueMainKeys.size() >= 2 && uniqueMainKeys[0] == "a",
                     "scan helper unique-key iteration keeps simple db order #1");
        ok &= expect(uniqueMainKeys.size() >= 2 && uniqueMainKeys[1] == "b",
                     "scan helper unique-key iteration keeps simple db order #2");

        const auto dupKeys = db::listKeys(txn, dup);
        ok &= expect(dupKeys.size() == 3, "scan helper lists key entries in dupsort db");
        ok &= expect(dupKeys.size() >= 3 && dupKeys[0] == "k",
                     "scan helper includes duplicate key instances #1");
        ok &= expect(dupKeys.size() >= 3 && dupKeys[1] == "k",
                     "scan helper includes duplicate key instances #2");
        ok &= expect(dupKeys.size() >= 3 && dupKeys[2] == "z",
                     "scan helper includes subsequent keys in dupsort db");

        const auto uniqueDupKeys = db::listUniqueKeys(txn, dup);
        ok &= expect(uniqueDupKeys.size() == 2,
                     "scan helper lists unique keys in dupsort db");
        ok &= expect(uniqueDupKeys.size() >= 2 && uniqueDupKeys[0] == "k",
                     "scan helper unique-key iteration preserves order #1");
        ok &= expect(uniqueDupKeys.size() >= 2 && uniqueDupKeys[1] == "z",
                     "scan helper unique-key iteration preserves order #2");

        const auto mainEntries = db::listEntries(txn, main);
        ok &= expect(mainEntries.size() == 2, "scan helper lists all key/value entries in simple db");
        ok &= expect(mainEntries.size() >= 2 && mainEntries[0].first == "a" &&
                       mainEntries[0].second == "a",
                     "scan helper preserves simple entry order/value #1");
        ok &= expect(mainEntries.size() >= 2 && mainEntries[1].first == "b" &&
                       mainEntries[1].second == "b",
                     "scan helper preserves simple entry order/value #2");

        const auto dupEntries = db::listEntries(txn, dup);
        ok &= expect(dupEntries.size() == 3, "scan helper lists key/value entries in dupsort db");
        ok &= expect(dupEntries.size() >= 3 && dupEntries[0].first == "k" &&
                       dupEntries[0].second == "v1",
                     "scan helper preserves dupsort entry order/value #1");
        ok &= expect(dupEntries.size() >= 3 && dupEntries[1].first == "k" &&
                       dupEntries[1].second == "v2",
                     "scan helper preserves dupsort entry order/value #2");
        ok &= expect(dupEntries.size() >= 3 && dupEntries[2].first == "z" &&
                       dupEntries[2].second == "v3",
                     "scan helper preserves dupsort entry order/value #3");

        const auto pagedEntries = db::listEntries(txn, dup, 1, 1);
        ok &= expect(pagedEntries.size() == 1, "scan helper supports paged entry iteration");
        ok &= expect(pagedEntries.size() >= 1 && pagedEntries[0].first == "k" &&
                       pagedEntries[0].second == "v2",
                     "scan helper paged iteration preserves entry ordering");

        const auto first = db::firstEntry(txn, dup);
        ok &= expect(first.has_value(), "scan helper returns first entry");
        ok &= expect(first.has_value() && first->first == "k" && first->second == "v1",
                     "scan helper first entry matches expected order");

        const auto last = db::lastEntry(txn, dup);
        ok &= expect(last.has_value(), "scan helper returns last entry");
        ok &= expect(last.has_value() && last->first == "z" && last->second == "v3",
                     "scan helper last entry matches expected order");

        std::vector<std::string> forEachValues;
        db::forEachEntry(
          txn, dup, [&forEachValues](std::string_view key, std::string_view value) {
              forEachValues.emplace_back(std::string(key) + "=" + std::string(value));
              return forEachValues.size() < 2;
          });
        ok &= expect(forEachValues.size() == 2, "scan helper forEachEntry supports early stop");
        ok &= expect(forEachValues.size() >= 2 && forEachValues[0] == "k=v1",
                     "scan helper forEachEntry preserves iteration order #1");
        ok &= expect(forEachValues.size() >= 2 && forEachValues[1] == "k=v2",
                     "scan helper forEachEntry preserves iteration order #2");

        std::vector<std::string> uniqueForEachKeys;
        db::forEachUniqueKey(txn, dup, [&uniqueForEachKeys](std::string_view key) {
            uniqueForEachKeys.emplace_back(key);
            return uniqueForEachKeys.size() < 2;
        });
        ok &= expect(uniqueForEachKeys.size() == 2,
                     "scan helper forEachUniqueKey supports early stop");
        ok &= expect(uniqueForEachKeys.size() >= 2 && uniqueForEachKeys[0] == "k",
                     "scan helper forEachUniqueKey preserves order #1");
        ok &= expect(uniqueForEachKeys.size() >= 2 && uniqueForEachKeys[1] == "z",
                     "scan helper forEachUniqueKey preserves order #2");

        std::vector<std::string> pagedForEachValues;
        db::forEachEntry(txn,
                         dup,
                         1,
                         1,
                         [&pagedForEachValues](std::string_view key, std::string_view value) {
                             pagedForEachValues.emplace_back(std::string(key) + "=" +
                                                             std::string(value));
                             return true;
                         });
        ok &= expect(pagedForEachValues.size() == 1,
                     "scan helper paged forEachEntry returns requested slice");
        ok &= expect(pagedForEachValues.size() >= 1 && pagedForEachValues[0] == "k=v2",
                     "scan helper paged forEachEntry preserves entry ordering");

        std::vector<std::string> fromKeyForward;
        db::forEachEntryFromKey(txn,
                                dup,
                                "k",
                                db::ScanDirection::Forward,
                                [&fromKeyForward](std::string_view key, std::string_view value) {
                                    fromKeyForward.emplace_back(std::string(key) + "=" +
                                                                std::string(value));
                                    return true;
                                });
        ok &= expect(fromKeyForward.size() == 3,
                     "scan helper forEachEntryFromKey iterates forward from key");
        ok &= expect(fromKeyForward.size() >= 1 && fromKeyForward[0] == "k=v1",
                     "scan helper forEachEntryFromKey forward order #1");

        std::vector<std::string> fromKeyBackward;
        db::forEachEntryFromKey(txn,
                                dup,
                                "z",
                                db::ScanDirection::Backward,
                                [&fromKeyBackward](std::string_view key, std::string_view value) {
                                    fromKeyBackward.emplace_back(std::string(key) + "=" +
                                                                 std::string(value));
                                    return true;
                                });
        ok &= expect(fromKeyBackward.size() == 3,
                     "scan helper forEachEntryFromKey iterates backward from key");
        ok &= expect(fromKeyBackward.size() >= 1 && fromKeyBackward[0] == "z=v3",
                     "scan helper forEachEntryFromKey backward order #1");

        std::vector<std::string> prefixValues;
        db::forEachEntryWithPrefix(
          txn, dup, "k", [&prefixValues](std::string_view key, std::string_view value) {
              prefixValues.emplace_back(std::string(key) + "=" + std::string(value));
              return true;
          });
        ok &= expect(prefixValues.size() == 2,
                     "scan helper forEachEntryWithPrefix iterates matching prefix");
        ok &= expect(prefixValues.size() >= 2 && prefixValues[0] == "k=v1",
                     "scan helper forEachEntryWithPrefix preserves order #1");
        ok &= expect(prefixValues.size() >= 2 && prefixValues[1] == "k=v2",
                     "scan helper forEachEntryWithPrefix preserves order #2");
    }

    {
        auto txn = backend->beginTxn();
        auto main = backend->openDbi(txn, "main");
        auto dup = backend->openDbi(txn, "dup");

        const auto removedNone = db::eraseEntriesIf(
          txn,
          main,
          0,
          0,
          [](std::string_view /*key*/, std::string_view /*value*/) { return true; });
        ok &= expect(removedNone == 0, "scan helper eraseEntriesIf supports empty paged erase");

        const auto removedMain = db::eraseEntriesIf(
          txn,
          main,
          0,
          1,
          [](std::string_view key, std::string_view /*value*/) { return key == "a"; });
        ok &= expect(removedMain == 1, "scan helper eraseEntriesIf removes from simple db");

        const auto removed = db::eraseEntriesIf(txn,
                                                dup,
                                                [](std::string_view key, std::string_view /*value*/) {
                                                    return key == "k";
                                                });
        ok &= expect(removed == 2, "scan helper eraseEntriesIf removes matching entries");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto main = backend->openDbi(txn, "main");
        auto dup = backend->openDbi(txn, "dup");

        const auto remainingMain = db::listEntries(txn, main);
        ok &= expect(remainingMain.size() == 1,
                     "scan helper eraseEntriesIf leaves non-matching simple-db entries");
        ok &= expect(remainingMain.size() >= 1 && remainingMain[0].first == "b" &&
                       remainingMain[0].second == "b",
                     "scan helper eraseEntriesIf preserves remaining simple-db value");

        const auto remaining = db::listEntries(txn, dup);
        ok &= expect(remaining.size() == 1, "scan helper eraseEntriesIf leaves non-matching entries");
        ok &= expect(remaining.size() >= 1 && remaining[0].first == "z" && remaining[0].second == "v3",
                     "scan helper eraseEntriesIf preserves non-matching entry values");
    }

    backend->close();
    return ok;
}

bool
testOrderEntryHelper()
{
    bool ok = true;

    const auto full = db::parseOrderEntry(R"({"event_id":"$event","prev_batch":"batch"})");
    ok &= expect(full.eventId.has_value() && *full.eventId == "$event",
                 "order-entry helper parses event_id");
    ok &= expect(full.hasPrevBatch, "order-entry helper detects prev_batch");
    ok &= expect(full.prevBatch.has_value() && *full.prevBatch == "batch",
                 "order-entry helper parses prev_batch string value");

    const auto emptyEventId = db::parseOrderEntry(R"({"event_id":"","prev_batch":"batch"})");
    ok &= expect(!emptyEventId.eventId.has_value(),
                 "order-entry helper ignores empty event_id");
    ok &= expect(emptyEventId.hasPrevBatch,
                 "order-entry helper preserves prev_batch detection with empty event_id");
    ok &= expect(emptyEventId.prevBatch.has_value() && *emptyEventId.prevBatch == "batch",
                 "order-entry helper preserves prev_batch string with empty event_id");

    const auto legacy = db::parseOrderEntry("$legacy-event");
    ok &= expect(legacy.eventId.has_value() && *legacy.eventId == "$legacy-event",
                 "order-entry helper falls back to legacy raw event-id format");
    ok &= expect(!legacy.hasPrevBatch,
                 "order-entry helper legacy fallback has no prev_batch");
    ok &= expect(!legacy.prevBatch.has_value(),
                 "order-entry helper legacy fallback has no prev_batch value");

    const auto serialized = db::serializeOrderEntry("$new-event", "next-batch");
    const auto roundtrip  = db::parseOrderEntry(serialized);
    ok &= expect(roundtrip.eventId.has_value() && *roundtrip.eventId == "$new-event",
                 "order-entry helper roundtrips serialized event_id");
    ok &= expect(roundtrip.hasPrevBatch && roundtrip.prevBatch.has_value() &&
                   *roundtrip.prevBatch == "next-batch",
                 "order-entry helper roundtrips serialized prev_batch");

    db::OrderEntry noPrev{};
    noPrev.eventId = "$event-only";
    const auto serializedNoPrev = db::serializeOrderEntry(noPrev);
    const auto parsedNoPrev     = db::parseOrderEntry(serializedNoPrev);
    ok &= expect(parsedNoPrev.eventId.has_value() && *parsedNoPrev.eventId == "$event-only",
                 "order-entry helper serializes event-only entry");
    ok &= expect(!parsedNoPrev.hasPrevBatch && !parsedNoPrev.prevBatch.has_value(),
                 "order-entry helper keeps event-only entry without prev_batch");

    return ok;
}

bool
testTimelineIndexHelper()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 128;
    backend->open("", options);

    {
        auto txn = backend->beginTxn();
        auto messageToOrderDb =
          backend->openDbi(txn, "message_to_order", openOptions(db::DbiFlags::Create));
        auto eventOrderDb = backend->openDbi(
          txn, "event_order", openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        auto orderToMessageDb = backend->openDbi(
          txn, "order_to_message", openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        auto eventsDb    = backend->openDbi(txn, "events", openOptions(db::DbiFlags::Create));
        auto relationsDb = backend->openDbi(
          txn, "relations", openOptions(db::DbiFlags::Create | db::DbiFlags::DupSort));
        auto eventToOrderDb =
          backend->openDbi(txn, "event_to_order", openOptions(db::DbiFlags::Create));

        const auto messageOrder = integerKey(7);
        ok &= expect(messageToOrderDb.put(txn, "$event", messageOrder),
                     "timeline index helper setup writes message_to_order mapping");
        ok &= expect(orderToMessageDb.put(txn, messageOrder, "$event"),
                     "timeline index helper setup writes order_to_message mapping");
        ok &= expect(eventsDb.put(txn, "$event", R"({"event_id":"$event"})"),
                     "timeline index helper setup writes event payload");
        ok &= expect(relationsDb.put(txn, "$event", "$related-a"),
                     "timeline index helper setup writes relation key mapping #1");
        ok &= expect(relationsDb.put(txn, "$event", "$related-b"),
                     "timeline index helper setup writes relation key mapping #2");
        ok &= expect(relationsDb.put(txn, "$other", "$event"),
                     "timeline index helper setup writes non-target relation mapping");
        ok &= expect(eventToOrderDb.put(txn, "$event", integerKey(42)),
                     "timeline index helper setup writes event_to_order mapping");

        ok &= expect(eventsDb.put(txn, "$txn", R"({"event_id":"$txn"})"),
                     "timeline index helper setup writes txn event payload");
        ok &= expect(eventOrderDb.put(txn, integerKey(99), db::serializeOrderEntry("$txn")),
                     "timeline index helper setup writes txn order-entry mapping");
        ok &= expect(eventToOrderDb.put(txn, "$txn", integerKey(99)),
                     "timeline index helper setup writes txn event_to_order mapping");
        ok &= expect(messageToOrderDb.put(txn, "$txn", integerKey(100)),
                     "timeline index helper setup writes txn message_to_order mapping");
        ok &= expect(orderToMessageDb.put(txn, integerKey(100), "$txn"),
                     "timeline index helper setup writes txn order_to_message mapping");

        ok &= expect(!db::replaceTimelineEventId(txn,
                                                 eventsDb,
                                                 eventOrderDb,
                                                 eventToOrderDb,
                                                 messageToOrderDb,
                                                 orderToMessageDb,
                                                 "",
                                                 "$event",
                                                 R"({"event_id":"$event"})",
                                                 db::serializeOrderEntry("$event")),
                     "timeline index helper replaceTimelineEventId rejects empty source id");
        ok &= expect(!db::replaceTimelineEventId(txn,
                                                 eventsDb,
                                                 eventOrderDb,
                                                 eventToOrderDb,
                                                 messageToOrderDb,
                                                 orderToMessageDb,
                                                 "$event",
                                                 "",
                                                 R"({"event_id":"$event"})",
                                                 db::serializeOrderEntry("$event")),
                     "timeline index helper replaceTimelineEventId rejects empty target id");
        ok &= expect(db::replaceTimelineEventId(txn,
                                                eventsDb,
                                                eventOrderDb,
                                                eventToOrderDb,
                                                messageToOrderDb,
                                                orderToMessageDb,
                                                "$event",
                                                "$event",
                                                R"({"event_id":"$event","edited":1})",
                                                db::serializeOrderEntry("$event")),
                     "timeline index helper replaceTimelineEventId supports same-id updates");
        std::string_view value;
        ok &= expect(eventsDb.get(txn, "$event", value),
                     "timeline index helper replaceTimelineEventId keeps payload for same-id update");
        ok &= expect(nlohmann::json::parse(value).value("edited", 0) == 1,
                     "timeline index helper replaceTimelineEventId writes same-id updated payload");

        ok &= expect(!db::replaceTimelineEventId(txn,
                                                 eventsDb,
                                                 eventOrderDb,
                                                 eventToOrderDb,
                                                 messageToOrderDb,
                                                 orderToMessageDb,
                                                 "$missing",
                                                 "$missing-new",
                                                 R"({"event_id":"$missing-new"})",
                                                 db::serializeOrderEntry("$missing-new")),
                     "timeline index helper replaceTimelineEventId is false for missing source id");
        ok &= expect(db::replaceTimelineEventId(txn,
                                                eventsDb,
                                                eventOrderDb,
                                                eventToOrderDb,
                                                messageToOrderDb,
                                                orderToMessageDb,
                                                "$txn",
                                                "$event-remapped",
                                                R"({"event_id":"$event-remapped"})",
                                                db::serializeOrderEntry("$event-remapped")),
                     "timeline index helper replaceTimelineEventId remaps timeline references");

        ok &= expect(!eventsDb.get(txn, "$txn", value),
                     "timeline index helper replaceTimelineEventId removes old event payload");
        ok &= expect(eventsDb.get(txn, "$event-remapped", value),
                     "timeline index helper replaceTimelineEventId writes new event payload");
        ok &= expect(!eventToOrderDb.get(txn, "$txn", value),
                     "timeline index helper replaceTimelineEventId removes old event_to_order mapping");
        ok &= expect(eventToOrderDb.get(txn, "$event-remapped", value),
                     "timeline index helper replaceTimelineEventId writes new event_to_order mapping");
        ok &= expect(value == integerKey(99),
                     "timeline index helper replaceTimelineEventId preserves event-order key");

        ok &= expect(!messageToOrderDb.get(txn, "$txn", value),
                     "timeline index helper replaceTimelineEventId removes old message_to_order mapping");
        ok &= expect(messageToOrderDb.get(txn, "$event-remapped", value),
                     "timeline index helper replaceTimelineEventId writes new message_to_order mapping");
        ok &= expect(value == integerKey(100),
                     "timeline index helper replaceTimelineEventId preserves message-order key");

        ok &= expect(orderToMessageDb.get(txn, integerKey(100), value),
                     "timeline index helper replaceTimelineEventId updates order_to_message mapping");
        ok &= expect(value == "$event-remapped",
                     "timeline index helper replaceTimelineEventId writes remapped order_to_message value");

        ok &= expect(eventOrderDb.get(txn, integerKey(99), value),
                     "timeline index helper replaceTimelineEventId rewrites order-entry value");
        const auto remappedOrderEntry = db::parseOrderEntry(value);
        ok &= expect(remappedOrderEntry.eventId.has_value() &&
                       remappedOrderEntry.eventId.value_or("") == "$event-remapped",
                     "timeline index helper replaceTimelineEventId stores remapped order-entry event id");

        db::putEventOrderMapping(
          txn, eventOrderDb, eventToOrderDb, 55, "$event-helper", db::serializeOrderEntry("$event-helper"));
        ok &= expect(eventToOrderDb.get(txn, "$event-helper", value),
                     "timeline index helper putEventOrderMapping writes event_to_order entry");
        ok &= expect(value == integerKey(55),
                     "timeline index helper putEventOrderMapping preserves event-order index bytes");
        ok &= expect(eventOrderDb.get(txn, integerKey(55), value),
                     "timeline index helper putEventOrderMapping writes event_order payload");
        const auto helperOrderEntry = db::parseOrderEntry(value);
        ok &= expect(helperOrderEntry.eventId.has_value() &&
                       helperOrderEntry.eventId.value_or("") == "$event-helper",
                     "timeline index helper putEventOrderMapping stores expected order-entry event id");

        db::putOrderEntry(txn, eventOrderDb, 56, "$event-helper-2", "batch-56");
        ok &= expect(eventOrderDb.get(txn, integerKey(56), value),
                     "timeline index helper putOrderEntry writes event_order payload");
        const auto helperOrderEntry2 = db::parseOrderEntry(value);
        ok &= expect(helperOrderEntry2.eventId.has_value() &&
                       helperOrderEntry2.eventId.value_or("") == "$event-helper-2",
                     "timeline index helper putOrderEntry stores expected event id");
        ok &= expect(helperOrderEntry2.hasPrevBatch &&
                       helperOrderEntry2.prevBatch.has_value() &&
                       helperOrderEntry2.prevBatch.value_or("") == "batch-56",
                     "timeline index helper putOrderEntry stores expected prev_batch");

        db::putEventOrderMappingForEvent(
          txn, eventOrderDb, eventToOrderDb, 57, "$event-helper-3", "batch-57");
        ok &= expect(eventToOrderDb.get(txn, "$event-helper-3", value),
                     "timeline index helper putEventOrderMappingForEvent writes event_to_order entry");
        ok &= expect(value == integerKey(57),
                     "timeline index helper putEventOrderMappingForEvent preserves event-order index");
        ok &= expect(eventOrderDb.get(txn, integerKey(57), value),
                     "timeline index helper putEventOrderMappingForEvent writes event_order payload");
        const auto helperOrderEntry3 = db::parseOrderEntry(value);
        ok &= expect(helperOrderEntry3.eventId.has_value() &&
                       helperOrderEntry3.eventId.value_or("") == "$event-helper-3",
                     "timeline index helper putEventOrderMappingForEvent stores expected event id");
        ok &= expect(helperOrderEntry3.hasPrevBatch &&
                       helperOrderEntry3.prevBatch.has_value() &&
                       helperOrderEntry3.prevBatch.value_or("") == "batch-57",
                     "timeline index helper putEventOrderMappingForEvent stores expected prev_batch");

        db::putMessageOrderMapping(
          txn, orderToMessageDb, messageToOrderDb, 101, "$message-helper", db::PutFlags::Append);
        ok &= expect(messageToOrderDb.get(txn, "$message-helper", value),
                     "timeline index helper putMessageOrderMapping writes message_to_order entry");
        ok &= expect(value == integerKey(101),
                     "timeline index helper putMessageOrderMapping preserves message-order index bytes");
        ok &= expect(orderToMessageDb.get(txn, integerKey(101), value),
                     "timeline index helper putMessageOrderMapping writes order_to_message entry");
        ok &= expect(value == "$message-helper",
                     "timeline index helper putMessageOrderMapping stores expected event id");

        std::uint64_t appendEventCursor = 1000;
        const auto appendedEventOrder   = db::appendEventOrderEntry(
          txn, eventOrderDb, eventToOrderDb, appendEventCursor, "$event-appended", R"({"event_id":"$event-appended"})");
        ok &= expect(appendedEventOrder == 1001 && appendEventCursor == 1001,
                     "timeline index helper appendEventOrderEntry increments event-order cursor");
        ok &= expect(eventToOrderDb.get(txn, "$event-appended", value) && value == integerKey(1001),
                     "timeline index helper appendEventOrderEntry writes event_to_order mapping");

        std::uint64_t prependEventCursor = 900;
        const auto prependedEventOrder   = db::prependEventOrderEntry(
          txn, eventOrderDb, eventToOrderDb, prependEventCursor, "$event-prepended", R"({"event_id":"$event-prepended"})");
        ok &= expect(prependedEventOrder == 899 && prependEventCursor == 899,
                     "timeline index helper prependEventOrderEntry decrements event-order cursor");
        ok &= expect(eventToOrderDb.get(txn, "$event-prepended", value) && value == integerKey(899),
                     "timeline index helper prependEventOrderEntry writes event_to_order mapping");

        std::uint64_t appendMessageCursor = 1000;
        const auto appendedMessageOrder =
          db::appendMessageOrderEntry(txn, orderToMessageDb, messageToOrderDb, appendMessageCursor, "$msg-appended");
        ok &= expect(appendedMessageOrder == 1001 && appendMessageCursor == 1001,
                     "timeline index helper appendMessageOrderEntry increments message-order cursor");
        ok &= expect(messageToOrderDb.get(txn, "$msg-appended", value) && value == integerKey(1001),
                     "timeline index helper appendMessageOrderEntry writes message_to_order mapping");

        std::uint64_t prependMessageCursor = 900;
        const auto prependedMessageOrder =
          db::prependMessageOrderEntry(txn, orderToMessageDb, messageToOrderDb, prependMessageCursor, "$msg-prepended");
        ok &= expect(prependedMessageOrder == 899 && prependMessageCursor == 899,
                     "timeline index helper prependMessageOrderEntry decrements message-order cursor");
        ok &= expect(messageToOrderDb.get(txn, "$msg-prepended", value) && value == integerKey(899),
                     "timeline index helper prependMessageOrderEntry writes message_to_order mapping");

        ok &= expect(!db::removeMessageOrderMapping(
                       txn, messageToOrderDb, orderToMessageDb, "$missing"),
                     "timeline index helper removeMessageOrderMapping is false for missing event");
        ok &= expect(db::removeMessageOrderMapping(txn, messageToOrderDb, orderToMessageDb, "$event"),
                     "timeline index helper removeMessageOrderMapping removes existing mapping");

        ok &= expect(!messageToOrderDb.get(txn, "$event", value),
                     "timeline index helper removes message_to_order entry");
        ok &= expect(!orderToMessageDb.get(txn, messageOrder, value),
                     "timeline index helper removes order_to_message entry");

        ok &= expect(messageToOrderDb.put(txn, "$event", messageOrder),
                     "timeline index helper setup restores message_to_order mapping");
        ok &= expect(orderToMessageDb.put(txn, messageOrder, "$event"),
                     "timeline index helper setup restores order_to_message mapping");

        db::removeTimelineEventReferences(txn,
                                          eventsDb,
                                          relationsDb,
                                          eventToOrderDb,
                                          messageToOrderDb,
                                          orderToMessageDb,
                                          "$event");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto messageToOrderDb = backend->openDbi(txn, "message_to_order");
        auto eventOrderDb =
          backend->openDbi(txn, "event_order", openOptions(db::DbiFlags::IntegerKey));
        auto orderToMessageDb =
          backend->openDbi(txn, "order_to_message", openOptions(db::DbiFlags::IntegerKey));
        auto eventsDb       = backend->openDbi(txn, "events");
        auto relationsDb    = backend->openDbi(txn, "relations");
        auto eventToOrderDb = backend->openDbi(txn, "event_to_order");

        std::string_view value;
        ok &= expect(!eventsDb.get(txn, "$event", value),
                     "timeline index helper removes event payload");
        ok &= expect(!eventToOrderDb.get(txn, "$event", value),
                     "timeline index helper removes event_to_order mapping");
        ok &= expect(!messageToOrderDb.get(txn, "$event", value),
                     "timeline index helper removes message_to_order mapping");
        ok &= expect(!orderToMessageDb.get(txn, integerKey(7), value),
                     "timeline index helper removes order_to_message mapping");
        ok &= expect(eventOrderDb.get(txn, integerKey(99), value),
                     "timeline index helper keeps remapped order-entry key/value");
        const auto persistedRemapEntry = db::parseOrderEntry(value);
        ok &= expect(persistedRemapEntry.eventId.has_value() &&
                       persistedRemapEntry.eventId.value_or("") == "$event-remapped",
                     "timeline index helper keeps remapped order-entry event id");

        const auto removedRelations = db::listDupValues(txn, relationsDb, "$event");
        ok &= expect(removedRelations.empty(),
                     "timeline index helper removes relation entries keyed by event id");

        const auto unrelatedRelations = db::listDupValues(txn, relationsDb, "$other");
        ok &= expect(unrelatedRelations.size() == 1 && unrelatedRelations.front() == "$event",
                     "timeline index helper keeps unrelated relation keys");
    }

    {
        auto txn = backend->beginTxn();
        auto eventToOrderDb =
          backend->openDbi(txn, "scan_event_to_order", openOptions(db::DbiFlags::Create));
        auto eventOrderDb = backend->openDbi(
          txn, "scan_event_order", openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        auto messageToOrderDb =
          backend->openDbi(txn, "scan_message_to_order", openOptions(db::DbiFlags::Create));
        auto orderToMessageDb = backend->openDbi(
          txn,
          "scan_order_to_message",
          openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        auto unusedOrderToMessageDb = backend->openDbi(
          txn,
          "scan_order_to_message_empty",
          openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));

        ok &= expect(eventOrderDb.put(txn, integerKey(1), db::serializeOrderEntry("$a")),
                     "timeline index helper scan setup writes order entry #1");
        ok &= expect(eventOrderDb.put(txn, integerKey(2), db::serializeOrderEntry("$b")),
                     "timeline index helper scan setup writes order entry #2");
        ok &= expect(eventOrderDb.put(txn, integerKey(3), db::serializeOrderEntry("$c")),
                     "timeline index helper scan setup writes order entry #3");
        ok &= expect(eventOrderDb.put(txn, integerKey(4), db::serializeOrderEntry("$d")),
                     "timeline index helper scan setup writes order entry #4");

        ok &= expect(eventToOrderDb.put(txn, "$a", integerKey(1)),
                     "timeline index helper scan setup writes event_to_order #1");
        ok &= expect(eventToOrderDb.put(txn, "$b", integerKey(2)),
                     "timeline index helper scan setup writes event_to_order #2");
        ok &= expect(eventToOrderDb.put(txn, "$c", integerKey(3)),
                     "timeline index helper scan setup writes event_to_order #3");
        ok &= expect(eventToOrderDb.put(txn, "$d", integerKey(4)),
                     "timeline index helper scan setup writes event_to_order #4");

        ok &= expect(messageToOrderDb.put(txn, "$a", integerKey(101)),
                     "timeline index helper scan setup writes visible message #1");
        ok &= expect(messageToOrderDb.put(txn, "$d", integerKey(104)),
                     "timeline index helper scan setup writes visible message #2");
        ok &= expect(orderToMessageDb.put(txn, integerKey(5), "$x"),
                     "timeline index helper scan setup writes order_to_message #1");
        ok &= expect(orderToMessageDb.put(txn, integerKey(9), "$y"),
                     "timeline index helper scan setup writes order_to_message #2");
        ok &= expect(orderToMessageDb.put(txn, integerKey(7), "$z"),
                     "timeline index helper scan setup writes order_to_message #3");
        ok &= expect(unusedOrderToMessageDb.size(txn) == 0,
                     "timeline index helper scan setup creates empty order_to_message db");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto eventToOrderDb = backend->openDbi(txn, "scan_event_to_order");
        auto eventOrderDb =
          backend->openDbi(txn, "scan_event_order", openOptions(db::DbiFlags::IntegerKey));
        auto messageToOrderDb = backend->openDbi(txn, "scan_message_to_order");
        auto orderToMessageDb =
          backend->openDbi(txn, "scan_order_to_message", openOptions(db::DbiFlags::IntegerKey));
        auto emptyOrderToMessageDb =
          backend->openDbi(txn, "scan_order_to_message_empty", openOptions(db::DbiFlags::IntegerKey));

        const auto invisibleFromA =
          db::lastInvisibleEventAfter(txn, eventToOrderDb, eventOrderDb, messageToOrderDb, "$a");
        ok &= expect(invisibleFromA.has_value(),
                     "timeline index helper lastInvisibleEventAfter returns value for visible start");
        ok &= expect(invisibleFromA.has_value() && invisibleFromA->first == 1 &&
                       invisibleFromA->second == "$a",
                     "timeline index helper lastInvisibleEventAfter returns current event when start is visible");

        const auto invisibleFromB =
          db::lastInvisibleEventAfter(txn, eventToOrderDb, eventOrderDb, messageToOrderDb, "$b");
        ok &= expect(invisibleFromB.has_value(),
                     "timeline index helper lastInvisibleEventAfter returns value for invisible start");
        ok &= expect(invisibleFromB.has_value() && invisibleFromB->first == 3 &&
                       invisibleFromB->second == "$c",
                     "timeline index helper lastInvisibleEventAfter returns last invisible before next visible");

        const auto visibleFromC =
          db::lastVisibleEvent(txn, eventToOrderDb, eventOrderDb, messageToOrderDb, "$c");
        ok &= expect(visibleFromC.has_value(),
                     "timeline index helper lastVisibleEvent returns value for invisible start");
        ok &= expect(visibleFromC.has_value() && visibleFromC->first == 1 &&
                       visibleFromC->second == "$a",
                     "timeline index helper lastVisibleEvent finds nearest previous visible event");

        const auto visibleFromD =
          db::lastVisibleEvent(txn, eventToOrderDb, eventOrderDb, messageToOrderDb, "$d");
        ok &= expect(visibleFromD.has_value(),
                     "timeline index helper lastVisibleEvent returns value for visible start");
        ok &= expect(visibleFromD.has_value() && visibleFromD->first == 4 &&
                       visibleFromD->second == "$d",
                     "timeline index helper lastVisibleEvent returns current event when start is visible");

        ok &= expect(!db::lastVisibleEvent(txn,
                                           eventToOrderDb,
                                           eventOrderDb,
                                           messageToOrderDb,
                                           "$missing")
                       .has_value(),
                     "timeline index helper lastVisibleEvent returns no value for missing start");
        ok &= expect(!db::lastInvisibleEventAfter(txn,
                                                  eventToOrderDb,
                                                  eventOrderDb,
                                                  messageToOrderDb,
                                                  "$missing")
                       .has_value(),
                     "timeline index helper lastInvisibleEventAfter returns no value for missing start");

        const auto lastTimelineEvent = db::lastTimelineEventId(txn, orderToMessageDb);
        ok &= expect(lastTimelineEvent.has_value() && lastTimelineEvent.value_or("") == "$y",
                     "timeline index helper lastTimelineEventId returns event at highest timeline index");

        const auto range = db::timelineRange(txn, orderToMessageDb);
        ok &= expect(range.has_value(), "timeline index helper timelineRange returns value for non-empty db");
        ok &= expect(range.has_value() && range->first == 5 && range->second == 9,
                     "timeline index helper timelineRange returns first and last timeline indexes");
        const auto firstOrdered = db::firstOrderedIndex(txn, orderToMessageDb);
        ok &= expect(firstOrdered.has_value() && firstOrdered.value_or(0) == 5,
                     "timeline index helper firstOrderedIndex returns first integer-key index");
        const auto lastOrdered = db::lastOrderedIndex(txn, orderToMessageDb);
        ok &= expect(lastOrdered.has_value() && lastOrdered.value_or(0) == 9,
                     "timeline index helper lastOrderedIndex returns last integer-key index");
        ok &= expect(!db::firstOrderedIndex(txn, emptyOrderToMessageDb).has_value(),
                     "timeline index helper firstOrderedIndex returns no value for empty db");
        ok &= expect(!db::lastOrderedIndex(txn, emptyOrderToMessageDb).has_value(),
                     "timeline index helper lastOrderedIndex returns no value for empty db");

        const auto visibleMessageIndex = db::timelineIndexForEvent(txn, messageToOrderDb, "$d");
        ok &= expect(visibleMessageIndex.has_value() && visibleMessageIndex.value_or(0) == 104,
                     "timeline index helper timelineIndexForEvent returns mapped timeline index");
        ok &= expect(!db::timelineIndexForEvent(txn, messageToOrderDb, "$missing").has_value(),
                     "timeline index helper timelineIndexForEvent returns no value for missing event");

        const auto eventIndex = db::eventIndexForEvent(txn, eventToOrderDb, "$c");
        ok &= expect(eventIndex.has_value() && eventIndex.value_or(0) == 3,
                     "timeline index helper eventIndexForEvent returns mapped event order index");
        ok &= expect(!db::eventIndexForEvent(txn, eventToOrderDb, "$missing").has_value(),
                     "timeline index helper eventIndexForEvent returns no value for missing event");

        const auto timelineEventAtSeven = db::timelineEventIdAtIndex(txn, orderToMessageDb, 7);
        ok &= expect(timelineEventAtSeven.has_value() && timelineEventAtSeven.value_or("") == "$z",
                     "timeline index helper timelineEventIdAtIndex returns event at requested index");
        ok &= expect(!db::timelineEventIdAtIndex(txn, orderToMessageDb, 100).has_value(),
                     "timeline index helper timelineEventIdAtIndex returns no value for missing index");
    }

    {
        auto txn = backend->beginTxn();
        auto pendingDb =
          backend->openDbi(txn, "pending_cleanup", openOptions(db::DbiFlags::Create));
        ok &= expect(pendingDb.put(txn, "1", "$txn-remove"),
                     "timeline index helper pending setup writes entry #1");
        ok &= expect(pendingDb.put(txn, "2", "$keep"),
                     "timeline index helper pending setup writes entry #2");
        ok &= expect(pendingDb.put(txn, "3", "$txn-remove"),
                     "timeline index helper pending setup writes entry #3");

        const auto removed = db::removePendingEntriesByTxnId(txn, pendingDb, "$txn-remove");
        ok &= expect(removed == 2,
                     "timeline index helper removePendingEntriesByTxnId removes matching pending entries");
        txn.commit();
    }

    {
        auto txn      = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto pendingDb = backend->openDbi(txn, "pending_cleanup");
        const auto pendingEntries = db::listEntries(txn, pendingDb);
        ok &= expect(pendingEntries.size() == 1,
                     "timeline index helper removePendingEntriesByTxnId leaves non-matching pending entries");
        ok &= expect(pendingEntries.size() >= 1 && pendingEntries[0].first == "2" &&
                       pendingEntries[0].second == "$keep",
                     "timeline index helper removePendingEntriesByTxnId preserves non-matching value");
    }

    {
        auto txn = backend->beginTxn();
        auto eventOrderDb = backend->openDbi(
          txn,
          "prev_batch_order",
          openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        ok &= expect(eventOrderDb.put(txn, integerKey(1), db::serializeOrderEntry("$first")),
                     "timeline index helper prev-batch setup writes first order entry");
        ok &= expect(eventOrderDb.put(txn,
                                      integerKey(2),
                                      db::serializeOrderEntry("$second", "existing-token")),
                     "timeline index helper prev-batch setup writes second order entry");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto eventOrderDb =
          backend->openDbi(txn, "prev_batch_order", openOptions(db::DbiFlags::IntegerKey));
        ok &= expect(!db::firstPrevBatchToken(txn, eventOrderDb).has_value(),
                     "timeline index helper firstPrevBatchToken returns no value when first entry has no token");
    }

    {
        auto txn = backend->beginTxn();
        auto eventOrderDb =
          backend->openDbi(txn, "prev_batch_order", openOptions(db::DbiFlags::IntegerKey));
        ok &= expect(db::setOrderEntryPrevBatch(txn, eventOrderDb, 1, "updated-token"),
                     "timeline index helper setOrderEntryPrevBatch updates existing entry");
        ok &= expect(!db::setOrderEntryPrevBatch(txn, eventOrderDb, 99, "missing-token"),
                     "timeline index helper setOrderEntryPrevBatch returns false for missing entry");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto eventOrderDb =
          backend->openDbi(txn, "prev_batch_order", openOptions(db::DbiFlags::IntegerKey));
        const auto firstToken = db::firstPrevBatchToken(txn, eventOrderDb);
        ok &= expect(firstToken.has_value() && firstToken.value_or("") == "updated-token",
                     "timeline index helper firstPrevBatchToken reads updated token");
        std::string_view encoded;
        ok &= expect(eventOrderDb.get(txn, integerKey(1), encoded),
                     "timeline index helper setOrderEntryPrevBatch keeps updated entry readable");
        const auto updatedEntry = db::parseOrderEntry(encoded);
        ok &= expect(updatedEntry.hasPrevBatch && updatedEntry.prevBatch.has_value() &&
                       updatedEntry.prevBatch.value_or("") == "updated-token",
                     "timeline index helper setOrderEntryPrevBatch writes token in entry payload");
    }

    {
        auto txn = backend->beginTxn();
        auto eventOrderDb = backend->openDbi(
          txn,
          "order_marker_scan",
          openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        ok &= expect(eventOrderDb.put(txn, integerKey(1), db::serializeOrderEntry("$e1")),
                     "timeline index helper marker scan setup writes order entry #1");
        ok &= expect(eventOrderDb.put(txn, integerKey(2), db::serializeOrderEntry("$e2")),
                     "timeline index helper marker scan setup writes order entry #2");
        ok &= expect(eventOrderDb.put(txn, integerKey(3), db::serializeOrderEntry("$e3", "token")),
                     "timeline index helper marker scan setup writes marker entry");
        ok &= expect(eventOrderDb.put(txn, integerKey(4), db::serializeOrderEntry("$e4")),
                     "timeline index helper marker scan setup writes order entry #4");
        ok &= expect(eventOrderDb.put(txn, integerKey(5), db::serializeOrderEntry("$e5")),
                     "timeline index helper marker scan setup writes order entry #5");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto eventOrderDb =
          backend->openDbi(txn, "order_marker_scan", openOptions(db::DbiFlags::IntegerKey));

        const auto orderEntriesToDelete = db::listOrderEntriesAfterPrevBatchMarker(txn, eventOrderDb);
        ok &= expect(orderEntriesToDelete.size() == 2,
                     "timeline index helper listOrderEntriesAfterPrevBatchMarker returns entries before marker");
        ok &= expect(orderEntriesToDelete.size() >= 2 &&
                       readIntegerKey(orderEntriesToDelete[0].first) == 2 &&
                       readIntegerKey(orderEntriesToDelete[1].first) == 1,
                     "timeline index helper listOrderEntriesAfterPrevBatchMarker preserves backward scan order");

        const auto eventIds = db::listOrderEntryEventIds(txn, eventOrderDb);
        ok &= expect(eventIds.size() == 5,
                     "timeline index helper listOrderEntryEventIds returns all event ids");
        ok &= expect(eventIds.size() >= 5 && eventIds[0] == "$e1" && eventIds[4] == "$e5",
                     "timeline index helper listOrderEntryEventIds preserves ordered iteration");
    }

    {
        auto txn = backend->beginTxn();
        auto eventOrderDb = backend->openDbi(
          txn,
          "order_cleanup",
          openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        auto eventToOrderDb =
          backend->openDbi(txn, "order_cleanup_event_to_order", openOptions(db::DbiFlags::Create));
        auto messageToOrderDb =
          backend->openDbi(txn, "order_cleanup_message_to_order", openOptions(db::DbiFlags::Create));
        auto orderToMessageDb = backend->openDbi(
          txn,
          "order_cleanup_order_to_message",
          openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        auto eventsDb = backend->openDbi(txn, "order_cleanup_events", openOptions(db::DbiFlags::Create));
        auto relationsDb = backend->openDbi(
          txn,
          "order_cleanup_relations",
          openOptions(db::DbiFlags::Create | db::DbiFlags::DupSort));

        ok &= expect(eventOrderDb.put(txn, integerKey(1), db::serializeOrderEntry("$e1")),
                     "timeline index helper order cleanup setup writes order entry #1");
        ok &= expect(eventOrderDb.put(txn, integerKey(2), db::serializeOrderEntry("$e2")),
                     "timeline index helper order cleanup setup writes order entry #2");
        ok &= expect(eventOrderDb.put(txn, integerKey(3), db::serializeOrderEntry("$e3")),
                     "timeline index helper order cleanup setup writes order entry #3");
        ok &= expect(eventToOrderDb.put(txn, "$e1", integerKey(1)),
                     "timeline index helper order cleanup setup writes event_to_order #1");
        ok &= expect(eventToOrderDb.put(txn, "$e2", integerKey(2)),
                     "timeline index helper order cleanup setup writes event_to_order #2");
        ok &= expect(eventToOrderDb.put(txn, "$e3", integerKey(3)),
                     "timeline index helper order cleanup setup writes event_to_order #3");
        ok &= expect(messageToOrderDb.put(txn, "$e1", integerKey(11)),
                     "timeline index helper order cleanup setup writes message_to_order #1");
        ok &= expect(messageToOrderDb.put(txn, "$e2", integerKey(12)),
                     "timeline index helper order cleanup setup writes message_to_order #2");
        ok &= expect(orderToMessageDb.put(txn, integerKey(11), "$e1"),
                     "timeline index helper order cleanup setup writes order_to_message #1");
        ok &= expect(orderToMessageDb.put(txn, integerKey(12), "$e2"),
                     "timeline index helper order cleanup setup writes order_to_message #2");
        ok &= expect(eventsDb.put(txn, "$e1", R"({"event_id":"$e1"})"),
                     "timeline index helper order cleanup setup writes event payload #1");
        ok &= expect(eventsDb.put(txn, "$e2", R"({"event_id":"$e2"})"),
                     "timeline index helper order cleanup setup writes event payload #2");
        ok &= expect(eventsDb.put(txn, "$e3", R"({"event_id":"$e3"})"),
                     "timeline index helper order cleanup setup writes event payload #3");
        ok &= expect(relationsDb.put(txn, "$e1", "$r1"),
                     "timeline index helper order cleanup setup writes relation #1");
        ok &= expect(relationsDb.put(txn, "$e2", "$r2"),
                     "timeline index helper order cleanup setup writes relation #2");

        std::string_view entryTwoValue;
        ok &= expect(eventOrderDb.get(txn, integerKey(2), entryTwoValue),
                     "timeline index helper order cleanup setup reads order entry #2 value");
        db::removeOrderEntryReferences(txn,
                                       eventsDb,
                                       relationsDb,
                                       eventToOrderDb,
                                       messageToOrderDb,
                                       orderToMessageDb,
                                       entryTwoValue);

        std::string_view entryOneValue;
        ok &= expect(eventOrderDb.get(txn, integerKey(1), entryOneValue),
                     "timeline index helper order cleanup setup reads order entry #1 value");
        db::removeOrderEntryWithReferences(txn,
                                           eventOrderDb,
                                           eventsDb,
                                           relationsDb,
                                           eventToOrderDb,
                                           messageToOrderDb,
                                           orderToMessageDb,
                                           integerKey(1),
                                           entryOneValue);

        const auto removedCount = db::eraseOrderEntriesWithReferencesIf(
          txn,
          eventOrderDb,
          eventsDb,
          relationsDb,
          eventToOrderDb,
          messageToOrderDb,
          orderToMessageDb,
          0,
          10,
          [](std::string_view orderKey, std::string_view /*orderEntryValue*/) {
              return orderKey == integerKey(3);
          });
        ok &= expect(removedCount == 1,
                     "timeline index helper eraseOrderEntriesWithReferencesIf removes matching order entries");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto eventOrderDb =
          backend->openDbi(txn, "order_cleanup", openOptions(db::DbiFlags::IntegerKey));
        auto eventToOrderDb = backend->openDbi(txn, "order_cleanup_event_to_order");
        auto messageToOrderDb = backend->openDbi(txn, "order_cleanup_message_to_order");
        auto orderToMessageDb = backend->openDbi(
          txn,
          "order_cleanup_order_to_message",
          openOptions(db::DbiFlags::IntegerKey));
        auto eventsDb = backend->openDbi(txn, "order_cleanup_events");
        auto relationsDb = backend->openDbi(txn, "order_cleanup_relations");

        std::string_view value;
        ok &= expect(eventOrderDb.size(txn) == 1,
                     "timeline index helper order cleanup leaves only one order entry");
        ok &= expect(!eventsDb.get(txn, "$e1", value) && !eventsDb.get(txn, "$e2", value) &&
                       !eventsDb.get(txn, "$e3", value),
                     "timeline index helper order cleanup removes event payloads for cleaned entries");
        ok &= expect(!eventToOrderDb.get(txn, "$e1", value) && !eventToOrderDb.get(txn, "$e2", value) &&
                       !eventToOrderDb.get(txn, "$e3", value),
                     "timeline index helper order cleanup removes event_to_order mappings");
        ok &= expect(!messageToOrderDb.get(txn, "$e1", value) &&
                       !messageToOrderDb.get(txn, "$e2", value),
                     "timeline index helper order cleanup removes message_to_order mappings");
        ok &= expect(!orderToMessageDb.get(txn, integerKey(11), value) &&
                       !orderToMessageDb.get(txn, integerKey(12), value),
                     "timeline index helper order cleanup removes order_to_message mappings");
        ok &= expect(db::listDupValues(txn, relationsDb, "$e1").empty() &&
                       db::listDupValues(txn, relationsDb, "$e2").empty(),
                     "timeline index helper order cleanup removes relation values for cleaned entries");
    }

    {
        auto txn = backend->beginTxn();
        auto eventOrderDb = backend->openDbi(
          txn,
          "message_mapping_cleanup_order",
          openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        auto orderToMessageDb = backend->openDbi(
          txn,
          "message_mapping_cleanup_o2m",
          openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        auto messageToOrderDb = backend->openDbi(
          txn, "message_mapping_cleanup_m2o", openOptions(db::DbiFlags::Create));

        ok &= expect(eventOrderDb.put(txn, integerKey(1), db::serializeOrderEntry("$keep-1")),
                     "timeline index helper message mapping cleanup setup writes order entry #1");
        ok &= expect(eventOrderDb.put(txn, integerKey(2), db::serializeOrderEntry("$keep-2")),
                     "timeline index helper message mapping cleanup setup writes order entry #2");
        ok &= expect(orderToMessageDb.put(txn, integerKey(11), "$keep-1"),
                     "timeline index helper message mapping cleanup setup writes o2m keep #1");
        ok &= expect(orderToMessageDb.put(txn, integerKey(12), "$keep-2"),
                     "timeline index helper message mapping cleanup setup writes o2m keep #2");
        ok &= expect(orderToMessageDb.put(txn, integerKey(13), "$stale"),
                     "timeline index helper message mapping cleanup setup writes o2m stale");
        ok &= expect(messageToOrderDb.put(txn, "$keep-1", integerKey(11)),
                     "timeline index helper message mapping cleanup setup writes m2o keep #1");
        ok &= expect(messageToOrderDb.put(txn, "$keep-2", integerKey(12)),
                     "timeline index helper message mapping cleanup setup writes m2o keep #2");
        ok &= expect(messageToOrderDb.put(txn, "$stale", integerKey(13)),
                     "timeline index helper message mapping cleanup setup writes m2o stale");

        const auto removed = db::removeMessageOrderMappingsNotInOrderEntries(
          txn, eventOrderDb, orderToMessageDb, messageToOrderDb);
        ok &= expect(removed == 1,
                     "timeline index helper removeMessageOrderMappingsNotInOrderEntries removes stale mappings");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto orderToMessageDb = backend->openDbi(
          txn,
          "message_mapping_cleanup_o2m",
          openOptions(db::DbiFlags::IntegerKey));
        auto messageToOrderDb = backend->openDbi(txn, "message_mapping_cleanup_m2o");

        std::string_view value;
        ok &= expect(!orderToMessageDb.get(txn, integerKey(13), value),
                     "timeline index helper removeMessageOrderMappingsNotInOrderEntries removes stale o2m key");
        ok &= expect(!messageToOrderDb.get(txn, "$stale", value),
                     "timeline index helper removeMessageOrderMappingsNotInOrderEntries removes stale m2o key");
        ok &= expect(orderToMessageDb.get(txn, integerKey(11), value) && value == "$keep-1",
                     "timeline index helper removeMessageOrderMappingsNotInOrderEntries preserves keep o2m #1");
        ok &= expect(orderToMessageDb.get(txn, integerKey(12), value) && value == "$keep-2",
                     "timeline index helper removeMessageOrderMappingsNotInOrderEntries preserves keep o2m #2");
        ok &= expect(messageToOrderDb.get(txn, "$keep-1", value) && value == integerKey(11),
                     "timeline index helper removeMessageOrderMappingsNotInOrderEntries preserves keep m2o #1");
        ok &= expect(messageToOrderDb.get(txn, "$keep-2", value) && value == integerKey(12),
                     "timeline index helper removeMessageOrderMappingsNotInOrderEntries preserves keep m2o #2");
    }

    {
        auto txn = backend->beginTxn();
        auto eventOrderDb = backend->openDbi(
          txn,
          "trim_oldest_order",
          openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        auto eventToOrderDb =
          backend->openDbi(txn, "trim_oldest_e2o", openOptions(db::DbiFlags::Create));
        auto messageToOrderDb =
          backend->openDbi(txn, "trim_oldest_m2o", openOptions(db::DbiFlags::Create));
        auto orderToMessageDb = backend->openDbi(
          txn,
          "trim_oldest_o2m",
          openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        auto eventsDb = backend->openDbi(txn, "trim_oldest_events", openOptions(db::DbiFlags::Create));
        auto relationsDb = backend->openDbi(
          txn,
          "trim_oldest_relations",
          openOptions(db::DbiFlags::Create | db::DbiFlags::DupSort));

        for (std::uint64_t index = 1; index <= 4; ++index) {
            const auto eventId = "$trim-" + std::to_string(index);
            ok &= expect(eventOrderDb.put(txn, integerKey(index), db::serializeOrderEntry(eventId)),
                         "timeline index helper trim-oldest setup writes event_order entry");
            ok &= expect(eventToOrderDb.put(txn, eventId, integerKey(index)),
                         "timeline index helper trim-oldest setup writes event_to_order entry");
            ok &= expect(messageToOrderDb.put(txn, eventId, integerKey(index + 100)),
                         "timeline index helper trim-oldest setup writes message_to_order entry");
            ok &= expect(orderToMessageDb.put(txn, integerKey(index + 100), eventId),
                         "timeline index helper trim-oldest setup writes order_to_message entry");
            ok &= expect(eventsDb.put(txn, eventId, "{}"),
                         "timeline index helper trim-oldest setup writes event payload");
            ok &= expect(relationsDb.put(txn, eventId, "$rel"),
                         "timeline index helper trim-oldest setup writes relation value");
        }

        const auto removed = db::trimOldestOrderEntriesWithReferences(txn,
                                                                       eventOrderDb,
                                                                       eventsDb,
                                                                       relationsDb,
                                                                       eventToOrderDb,
                                                                       messageToOrderDb,
                                                                       orderToMessageDb,
                                                                       2);
        ok &= expect(removed == 2,
                     "timeline index helper trimOldestOrderEntriesWithReferences removes requested count");
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto eventOrderDb =
          backend->openDbi(txn, "trim_oldest_order", openOptions(db::DbiFlags::IntegerKey));
        auto eventToOrderDb = backend->openDbi(txn, "trim_oldest_e2o");
        auto messageToOrderDb = backend->openDbi(txn, "trim_oldest_m2o");
        auto orderToMessageDb =
          backend->openDbi(txn, "trim_oldest_o2m", openOptions(db::DbiFlags::IntegerKey));
        auto eventsDb = backend->openDbi(txn, "trim_oldest_events");

        std::string_view value;
        ok &= expect(eventOrderDb.size(txn) == 2,
                     "timeline index helper trimOldestOrderEntriesWithReferences leaves remaining order entries");
        ok &= expect(!eventToOrderDb.get(txn, "$trim-1", value) && !eventToOrderDb.get(txn, "$trim-2", value),
                     "timeline index helper trimOldestOrderEntriesWithReferences removes early event_to_order mappings");
        ok &= expect(!messageToOrderDb.get(txn, "$trim-1", value) &&
                       !messageToOrderDb.get(txn, "$trim-2", value),
                     "timeline index helper trimOldestOrderEntriesWithReferences removes early message_to_order mappings");
        ok &= expect(orderToMessageDb.get(txn, integerKey(103), value) && value == "$trim-3",
                     "timeline index helper trimOldestOrderEntriesWithReferences preserves later order_to_message mapping");
        ok &= expect(!eventsDb.get(txn, "$trim-1", value) && !eventsDb.get(txn, "$trim-2", value),
                     "timeline index helper trimOldestOrderEntriesWithReferences removes early event payloads");
    }

    {
        auto txn = backend->beginTxn();
        auto eventOrderDb = backend->openDbi(
          txn,
          "clear_before_marker_order",
          openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        auto eventToOrderDb =
          backend->openDbi(txn, "clear_before_marker_e2o", openOptions(db::DbiFlags::Create));
        auto messageToOrderDb =
          backend->openDbi(txn, "clear_before_marker_m2o", openOptions(db::DbiFlags::Create));
        auto orderToMessageDb = backend->openDbi(
          txn,
          "clear_before_marker_o2m",
          openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        auto eventsDb =
          backend->openDbi(txn, "clear_before_marker_events", openOptions(db::DbiFlags::Create));
        auto relationsDb = backend->openDbi(
          txn,
          "clear_before_marker_relations",
          openOptions(db::DbiFlags::Create | db::DbiFlags::DupSort));

        for (std::uint64_t index = 1; index <= 5; ++index) {
            const auto eventId = "$marker-" + std::to_string(index);
            const auto orderEntry = index == 3 ? db::serializeOrderEntry(eventId, "batch-token")
                                               : db::serializeOrderEntry(eventId);
            ok &= expect(eventOrderDb.put(txn, integerKey(index), orderEntry),
                         "timeline index helper clear-before-marker setup writes event_order entry");
            ok &= expect(eventToOrderDb.put(txn, eventId, integerKey(index)),
                         "timeline index helper clear-before-marker setup writes event_to_order entry");
            ok &= expect(messageToOrderDb.put(txn, eventId, integerKey(index + 200)),
                         "timeline index helper clear-before-marker setup writes message_to_order entry");
            ok &= expect(orderToMessageDb.put(txn, integerKey(index + 200), eventId),
                         "timeline index helper clear-before-marker setup writes order_to_message entry");
            ok &= expect(eventsDb.put(txn, eventId, "{}"),
                         "timeline index helper clear-before-marker setup writes event payload");
            ok &= expect(relationsDb.put(txn, eventId, "$rel"),
                         "timeline index helper clear-before-marker setup writes relation value");
        }

        ok &= expect(messageToOrderDb.put(txn, "$stale-marker", integerKey(999)),
                     "timeline index helper clear-before-marker setup writes stale message_to_order entry");
        ok &= expect(orderToMessageDb.put(txn, integerKey(999), "$stale-marker"),
                     "timeline index helper clear-before-marker setup writes stale order_to_message entry");

        db::cleanupTimelineBeforePrevBatchMarker(txn,
                                                 eventOrderDb,
                                                 eventsDb,
                                                 relationsDb,
                                                 eventToOrderDb,
                                                 messageToOrderDb,
                                                 orderToMessageDb);
        txn.commit();
    }

    {
        auto txn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto eventOrderDb =
          backend->openDbi(txn, "clear_before_marker_order", openOptions(db::DbiFlags::IntegerKey));
        auto eventToOrderDb = backend->openDbi(txn, "clear_before_marker_e2o");
        auto messageToOrderDb = backend->openDbi(txn, "clear_before_marker_m2o");
        auto orderToMessageDb = backend->openDbi(
          txn,
          "clear_before_marker_o2m",
          openOptions(db::DbiFlags::IntegerKey));
        auto eventsDb = backend->openDbi(txn, "clear_before_marker_events");

        std::string_view value;
        ok &= expect(eventOrderDb.size(txn) == 3,
                     "timeline index helper cleanupTimelineBeforePrevBatchMarker keeps marker and newer entries");
        ok &= expect(!eventToOrderDb.get(txn, "$marker-1", value) &&
                       !eventToOrderDb.get(txn, "$marker-2", value),
                     "timeline index helper cleanupTimelineBeforePrevBatchMarker removes older event_to_order mappings");
        ok &= expect(orderToMessageDb.get(txn, integerKey(203), value) && value == "$marker-3",
                     "timeline index helper cleanupTimelineBeforePrevBatchMarker preserves marker order_to_message mapping");
        ok &= expect(!orderToMessageDb.get(txn, integerKey(999), value),
                     "timeline index helper cleanupTimelineBeforePrevBatchMarker removes stale order_to_message mapping");
        ok &= expect(!messageToOrderDb.get(txn, "$stale-marker", value),
                     "timeline index helper cleanupTimelineBeforePrevBatchMarker removes stale message_to_order mapping");
        ok &= expect(eventsDb.get(txn, "$marker-3", value) && eventsDb.get(txn, "$marker-4", value) &&
                       eventsDb.get(txn, "$marker-5", value),
                     "timeline index helper cleanupTimelineBeforePrevBatchMarker keeps marker and newer payloads");
    }

    backend->close();
    return ok;
}

bool
testFactory()
{
    bool ok = true;

    auto defaultBackend = db::createDefaultBackend();
#if KOMAI_DB_WITH_LMDB
    ok &= expect(defaultBackend->id() == db::kLmdbBackendId, "default backend is lmdb");
    ok &= expect(defaultBackend->supportsCompaction(), "lmdb backend reports compaction support");
    ok &= expect(defaultBackend->storageCategory() == db::StorageCategory::Persistent,
                 "lmdb backend is persistent");
#else
    ok &= expect(defaultBackend->id() == db::kMemoryBackendId, "default backend falls back to memory");
    ok &= expect(!defaultBackend->supportsCompaction(),
                 "memory default reports no compaction support");
    ok &= expect(defaultBackend->storageCategory() == db::StorageCategory::Ephemeral,
                 "memory default is ephemeral");
#endif

    auto memoryBackend = db::createBackend(db::kMemoryBackendId);
    ok &= expect(memoryBackend->id() == db::kMemoryBackendId, "memory backend is creatable");
    ok &= expect(!memoryBackend->supportsCompaction(),
                 "memory backend reports no compaction support");
    ok &= expect(memoryBackend->storageCategory() == db::StorageCategory::Ephemeral,
                 "memory backend is ephemeral");

    auto configuredDefault = db::createConfiguredBackend("");
#if KOMAI_DB_WITH_LMDB
    ok &= expect(configuredDefault->id() == db::kLmdbBackendId, "configured backend defaults to lmdb on empty id");
#else
    ok &= expect(configuredDefault->id() == db::kMemoryBackendId,
                 "configured backend defaults to memory when lmdb is disabled");
#endif

    auto configuredMemory = db::createConfiguredBackend(db::kMemoryBackendId);
    ok &= expect(configuredMemory->id() == db::kMemoryBackendId,
                 "configured backend accepts explicit memory id");
    auto configuredInMemoryAlias = db::createConfiguredBackend(db::kInMemoryBackendId);
    ok &= expect(configuredInMemoryAlias->id() == db::kMemoryBackendId,
                 "configured backend aliases in-memory to memory id");

    EnvVarGuard envGuard("KOMAI_DB_BACKEND_TEST_OVERRIDE");
    envGuard.unset();
    auto envDefault = db::createConfiguredBackendFromEnvironment(envGuard.name_);
#if KOMAI_DB_WITH_LMDB
    ok &= expect(envDefault->id() == db::kLmdbBackendId, "environment-based backend defaults to lmdb when unset");
#else
    ok &= expect(envDefault->id() == db::kMemoryBackendId,
                 "environment-based backend defaults to memory when lmdb is disabled");
#endif

    envGuard.set(db::kMemoryBackendId);
    auto envMemory = db::createConfiguredBackendFromEnvironment(envGuard.name_);
    ok &= expect(envMemory->id() == db::kMemoryBackendId, "environment-based backend accepts memory id");

    envGuard.set("not-a-backend");
    ok &= expectDbError([&] { db::createConfiguredBackendFromEnvironment(envGuard.name_); },
                        "environment-based backend rejects unknown id");

    ok &= expectDbError([] { db::createBackend("not-a-backend"); },
                        "unknown backend id fails with db::Error");
    ok &= expectDbError([] { db::createConfiguredBackend("not-a-backend"); },
                        "configured backend rejects unknown id");
#if KOMAI_DB_WITH_LMDB
    auto lmdbBackend = db::createBackend(db::kLmdbBackendId);
    ok &= expect(lmdbBackend->id() == db::kLmdbBackendId, "lmdb backend is creatable when enabled");
#else
    ok &= expectDbError([] { db::createBackend(db::kLmdbBackendId); },
                        "lmdb backend creation fails when lmdb support is disabled");
#endif
    return ok;
}

bool
testInMemoryBackend()
{
    bool ok = true;

    auto backend               = db::createBackend(db::kMemoryBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
    backend->open("", options);

    ok &= expect(backend->isOpen(), "memory backend opens");

    {
        auto rwTxn = backend->beginTxn();
        auto main  = backend->openDbi(rwTxn, "main", openOptions(db::DbiFlags::Create));
        ok &= expect(main.put(rwTxn, "k", "v"), "memory put into main");
        rwTxn.commit();
    }

    {
        auto roTxn  = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto mainRo = backend->openDbi(roTxn, "main");
        std::string_view value;
        ok &= expect(mainRo.get(roTxn, "k", value), "memory get finds written key");
        ok &= expect(value == "v", "memory get returns expected value");

        const auto names = backend->listDbiNames(roTxn);
        ok &= expect(containsName(names, "main"), "memory listDbiNames contains main");

        ok &= expectDbError([&] { backend->openDbi(roTxn, ""); },
                            "memory openDbi rejects empty database name");
    }

    {
        auto rwDupTxn = backend->beginTxn();
        auto dupDb    = backend->openDbi(
          rwDupTxn,
          "state_by_key",
          openOptions(db::DbiFlags::Create | db::DbiFlags::DupSort,
                      db::DupsortComparator::StateKey));
        ok &= expect(dupDb.put(rwDupTxn, "m.room.member", compositeStateValue("zeta", "$event2")),
                     "memory put dupsort value #1");
        ok &= expect(dupDb.put(rwDupTxn, "m.room.member", compositeStateValue("alpha", "$event1")),
                     "memory put dupsort value #2");
        rwDupTxn.commit();
    }

    {
        auto roDupTxn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto dupDbRo  = backend->openDbi(
          roDupTxn, "state_by_key", openOptions(db::DbiFlags::DupSort, db::DupsortComparator::StateKey));

        auto cursor = db::Cursor::open(roDupTxn, dupDbRo);
        std::string_view key = "m.room.member", dupValue;
        ok &= expect(cursor.get(key, dupValue, db::CursorOp::Set), "memory dupsort cursor set works");
        ok &= expect(key == "m.room.member", "memory dupsort key is expected");
        ok &= expect(stateKeyFromComposite(dupValue) == "alpha",
                     "memory dupsort comparator orders by state_key");

        ok &= expectDbError(
          [&] {
              backend->openDbi(roDupTxn,
                               "state_by_key",
                               openOptions(db::DbiFlags::DupSort,
                                           db::DupsortComparator::LegacyStateByKeyJson));
          },
          "memory openDbi rejects comparator mismatch");
    }

    {
        auto rwPlainTxn = backend->beginTxn();
        ok &= expectDbError(
          [&] {
              backend->openDbi(
                rwPlainTxn,
                "plain",
                openOptions(db::DbiFlags::Create, db::DupsortComparator::StateKey));
          },
          "memory openDbi rejects dupsort comparator on non-dupsort db");
    }

    ok &= testCursorAndOrderingContract(*backend, db::kMemoryBackendId);

    backend->close();
    ok &= expect(!backend->isOpen(), "memory backend closes");
    return ok;
}

bool
testLmdbBackend()
{
#if !KOMAI_DB_WITH_LMDB
    return true;
#else
    bool ok = true;

    QTemporaryDir tmp;
    ok &= expect(tmp.isValid(), "temporary directory for lmdb backend");
    if (!tmp.isValid())
        return false;

    auto backend               = db::createBackend(db::kLmdbBackendId);
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 24;
    options.maxDbs             = 32;
    options.durability         = db::Durability::Durable;
    backend->open(tmp.path().toStdString(), options);

    ok &= expect(backend->isOpen(), "lmdb backend opens");

    {
        auto rwTxn = backend->beginTxn();
        auto one   = backend->openDbi(rwTxn, "one", openOptions(db::DbiFlags::Create));
        auto two = backend->openDbi(
          rwTxn, "two", openOptions(db::DbiFlags::Create | db::DbiFlags::DupSort));
        ok &= expect(one.put(rwTxn, "k1", "v1"), "lmdb put in one");
        ok &= expect(two.put(rwTxn, "k2", "v2"), "lmdb put in two");
        rwTxn.commit();
    }

    {
        auto roTxn      = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        const auto names = backend->listDbiNames(roTxn);
        ok &= expect(containsName(names, "one"), "lmdb listDbiNames contains one");
        ok &= expect(containsName(names, "two"), "lmdb listDbiNames contains two");

        ok &= expectDbError([&] { backend->openDbi(roTxn, ""); },
                            "lmdb openDbi rejects empty database name");
        ok &= expectDbError(
          [&] {
              backend->openDbi(
                roTxn, "plain", openOptions(db::DbiFlags::None, db::DupsortComparator::StateKey));
          },
          "lmdb openDbi rejects dupsort comparator on non-dupsort db");
    }

    ok &= testCursorAndOrderingContract(*backend, db::kLmdbBackendId);

    backend->close();
    ok &= expect(!backend->isOpen(), "lmdb backend closes");

    backend->open(tmp.path().toStdString(), options);
    {
        auto roTxn = backend->beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto one   = backend->openDbi(roTxn, "one");
        std::string_view value;
        ok &= expect(one.get(roTxn, "k1", value), "lmdb data survives reopen");
        ok &= expect(value == "v1", "lmdb reopened value matches written value");
    }

    backend->close();
    ok &= expect(!backend->isOpen(), "lmdb backend closes");
    return ok;
#endif
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testCatalog();
    ok &= testSchemaHelpers();
    ok &= testLegacyOlmMigrationHelpers();
    ok &= testNamePolicy();
    ok &= testOpenHelpers();
    ok &= testStateIndexHelper();
    ok &= testSyncStateHelper();
    ok &= testMegolmIndexHelper();
    ok &= testReadReceiptIndexHelper();
    ok &= testRoomInfoHelper();
    ok &= testMemberInfoHelper();
    ok &= testJsonHelpers();
    ok &= testCacheCryptoHelpers();
    ok &= testOlmSessionIndexHelper();
    ok &= testDupIndexHelper();
    ok &= testScanHelper();
    ok &= testOrderEntryHelper();
    ok &= testTimelineIndexHelper();
    ok &= testCompactionHelper();
    ok &= testFactory();
    ok &= testInMemoryBackend();
    ok &= testLmdbBackend();
    return ok ? 0 : 1;
}
