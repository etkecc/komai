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

#include "db/Backend.h"
#include "db/Catalog.h"
#include "db/Compaction.h"
#include "db/DbTypes.h"
#include "db/NamePolicy.h"
#include "db/Open.h"
#include "db/Schema.h"
#include "db/StateIndex.h"

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

    auto backend               = db::createBackend("memory");
    db::BackendOptions options = {};
    options.mapSizeBytes       = 1U << 20;
    options.maxDbs             = 32;
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

    auto backend               = db::createBackend("memory");
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

    auto backend               = db::createBackend("memory");
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

    auto from                  = db::createBackend("memory");
    auto to                    = db::createBackend("memory");
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

    auto backend               = db::createBackend("memory");
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
testFactory()
{
    bool ok = true;

    auto defaultBackend = db::createDefaultBackend();
#if KOMAI_DB_WITH_LMDB
    ok &= expect(defaultBackend->id() == "lmdb", "default backend is lmdb");
    ok &= expect(defaultBackend->supportsCompaction(), "lmdb backend reports compaction support");
#else
    ok &= expect(defaultBackend->id() == "memory", "default backend falls back to memory");
    ok &= expect(!defaultBackend->supportsCompaction(),
                 "memory default reports no compaction support");
#endif

    auto memoryBackend = db::createBackend("memory");
    ok &= expect(memoryBackend->id() == "memory", "memory backend is creatable");
    ok &= expect(!memoryBackend->supportsCompaction(),
                 "memory backend reports no compaction support");

    auto configuredDefault = db::createConfiguredBackend("");
#if KOMAI_DB_WITH_LMDB
    ok &= expect(configuredDefault->id() == "lmdb", "configured backend defaults to lmdb on empty id");
#else
    ok &= expect(configuredDefault->id() == "memory",
                 "configured backend defaults to memory when lmdb is disabled");
#endif

    auto configuredMemory = db::createConfiguredBackend("memory");
    ok &= expect(configuredMemory->id() == "memory",
                 "configured backend accepts explicit memory id");

    EnvVarGuard envGuard("KOMAI_DB_BACKEND_TEST_OVERRIDE");
    envGuard.unset();
    auto envDefault = db::createConfiguredBackendFromEnvironment(envGuard.name_);
#if KOMAI_DB_WITH_LMDB
    ok &= expect(envDefault->id() == "lmdb", "environment-based backend defaults to lmdb when unset");
#else
    ok &= expect(envDefault->id() == "memory",
                 "environment-based backend defaults to memory when lmdb is disabled");
#endif

    envGuard.set("memory");
    auto envMemory = db::createConfiguredBackendFromEnvironment(envGuard.name_);
    ok &= expect(envMemory->id() == "memory", "environment-based backend accepts memory id");

    envGuard.set("not-a-backend");
    ok &= expectDbError([&] { db::createConfiguredBackendFromEnvironment(envGuard.name_); },
                        "environment-based backend rejects unknown id");

    ok &= expectDbError([] { db::createBackend("not-a-backend"); },
                        "unknown backend id fails with db::Error");
    ok &= expectDbError([] { db::createConfiguredBackend("not-a-backend"); },
                        "configured backend rejects unknown id");
#if KOMAI_DB_WITH_LMDB
    auto lmdbBackend = db::createBackend("lmdb");
    ok &= expect(lmdbBackend->id() == "lmdb", "lmdb backend is creatable when enabled");
#else
    ok &= expectDbError([] { db::createBackend("lmdb"); },
                        "lmdb backend creation fails when lmdb support is disabled");
#endif
    return ok;
}

bool
testInMemoryBackend()
{
    bool ok = true;

    auto backend               = db::createBackend("memory");
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

    ok &= testCursorAndOrderingContract(*backend, "memory");

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

    auto backend               = db::createBackend("lmdb");
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

    ok &= testCursorAndOrderingContract(*backend, "lmdb");

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
    ok &= testCompactionHelper();
    ok &= testFactory();
    ok &= testInMemoryBackend();
    ok &= testLmdbBackend();
    return ok ? 0 : 1;
}
