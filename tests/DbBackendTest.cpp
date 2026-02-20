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

#include "db/Backend.h"
#include "db/Catalog.h"
#include "db/DbTypes.h"
#include "db/NamePolicy.h"

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
          txn, dupDbName.c_str(), openOptions(db::DbiFlags::Create | db::DbiFlags::DupSort));
        ok &= expect(dbi.put(txn, "k", "b"), testName("dupsort put #1"));
        ok &= expect(dbi.put(txn, "k", "a"), testName("dupsort put #2"));
        ok &= expect(dbi.put(txn, "k", "c"), testName("dupsort put #3"));
        ok &= expect(dbi.put(txn, "z", "zz"), testName("dupsort put #4"));
        txn.commit();
    }

    {
        auto txn = backend.beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto dbi = backend.openDbi(txn, dupDbName.c_str(), openOptions(db::DbiFlags::DupSort));

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
          txn, intDbName.c_str(), openOptions(db::DbiFlags::Create | db::DbiFlags::IntegerKey));
        ok &= expect(dbi.put(txn, integerKey(5), "five"), testName("integer-key put #1"));
        ok &= expect(dbi.put(txn, integerKey(1), "one"), testName("integer-key put #2"));
        ok &= expect(dbi.put(txn, integerKey(3), "three"), testName("integer-key put #3"));
        txn.commit();
    }

    {
        auto txn = backend.beginTxn(nullptr, db::TxnFlags::ReadOnly);
        auto dbi =
          backend.openDbi(txn, intDbName.c_str(), openOptions(db::DbiFlags::IntegerKey));

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
testFactory()
{
    bool ok = true;

    auto defaultBackend = db::createDefaultBackend();
    ok &= expect(defaultBackend->id() == "lmdb", "default backend is lmdb");
    ok &= expect(defaultBackend->supportsCompaction(), "lmdb backend reports compaction support");

    auto memoryBackend = db::createBackend("memory");
    ok &= expect(memoryBackend->id() == "memory", "memory backend is creatable");
    ok &= expect(!memoryBackend->supportsCompaction(),
                 "memory backend reports no compaction support");

    ok &= expectDbError([] { db::createBackend("not-a-backend"); },
                        "unknown backend id fails with db::Error");
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
    backend->open(QString{}, options);

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

        ok &= expectDbError([&] { backend->openDbi(roTxn, nullptr); },
                            "memory openDbi rejects null database name");
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
    backend->open(tmp.path(), options);

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

        ok &= expectDbError([&] { backend->openDbi(roTxn, nullptr); },
                            "lmdb openDbi rejects null database name");
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

    backend->open(tmp.path(), options);
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
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testCatalog();
    ok &= testNamePolicy();
    ok &= testFactory();
    ok &= testInMemoryBackend();
    ok &= testLmdbBackend();
    return ok ? 0 : 1;
}
