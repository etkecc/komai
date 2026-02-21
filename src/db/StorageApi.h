// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

#include "db/Error.h"
#include "db/Backend.h"
#include "db/NamePolicy.h"
#include "db/Catalog.h"

namespace db::storage {

using Database    = db::Database;
using Transaction = db::Transaction;
using Store       = db::Store;
using CursorHandle = db::CursorHandle;
using Options     = db::StoreOpenOptions;

enum class AccessMode
{
    ReadWrite,
    ReadOnly,
};

enum class Capability
{
    None,
    Transactions,
    DuplicateKeys,
    IntegerKeys,
    PrefixScan,
};

inline AccessFlags
toAccessFlags(AccessMode mode) noexcept
{
    return mode == AccessMode::ReadOnly ? AccessFlags::ReadOnly : AccessFlags::None;
}

inline bool
supportsCapability(const Database &database, Capability capability) noexcept
{
    switch (capability) {
    case Capability::None:
        return true;
    case Capability::Transactions:
        // All backends we expose are transaction-capable today.
        return true;
    case Capability::DuplicateKeys:
        return database.supports(db::StoreCapability::DuplicateKeys);
    case Capability::IntegerKeys:
        return database.supports(db::StoreCapability::IntegerKeys);
    case Capability::PrefixScan:
        return database.supports(db::StoreCapability::PrefixScan);
    default:
        return false;
    }
}

inline void
requireCapabilities(const Database &database, StoreFlags flags)
{
    if (hasFlag(flags, StoreFlags::DupSort) && !supportsCapability(database, Capability::DuplicateKeys))
        throw Error("Backend does not support duplicate-key stores", ErrorKind::Invalid);
    if (hasFlag(flags, StoreFlags::IntegerKey) &&
        !supportsCapability(database, Capability::IntegerKeys))
        throw Error("Backend does not support integer-key stores", ErrorKind::Invalid);
}

class Cursor
{
public:
    Cursor() = default;
    Cursor(Transaction &txn, Store &store)
      : handle_(db::Cursor::open(txn, store))
    {}

    static Cursor
    open(Transaction &txn, Store &store)
    {
        return Cursor{txn, store};
    }

    bool
    moveFirst(std::string &key, std::string &value)
    {
        return move(MoveOp::First, key, value);
    }

    bool
    moveLast(std::string &key, std::string &value)
    {
        return move(MoveOp::Last, key, value);
    }

    bool
    moveNext(std::string &key, std::string &value)
    {
        return move(MoveOp::Next, key, value);
    }

    bool
    movePrev(std::string &key, std::string &value)
    {
        return move(MoveOp::Prev, key, value);
    }

    bool
    moveNextNoDup(std::string &key, std::string &value)
    {
        return move(MoveOp::NextNoDup, key, value);
    }

    bool
    moveNextDup(std::string &key, std::string &value)
    {
        return move(MoveOp::NextDup, key, value);
    }

    bool
    moveTo(std::string_view key, std::string &foundKey, std::string &foundValue)
    {
        std::string seekKey{key};
        return move(MoveOp::Set, seekKey, foundKey, foundValue);
    }

    bool
    moveToRange(std::string_view key, std::string &foundKey, std::string &foundValue)
    {
        std::string seekKey{key};
        return move(MoveOp::SetRange, seekKey, foundKey, foundValue);
    }

private:
    bool
    move(MoveOp op, std::string &key, std::string &value)
    {
        std::string_view keyBytes = key;
        std::string_view valueBytes;

        if (!handle_.get(keyBytes, valueBytes, op))
            return false;

        key.assign(keyBytes.data(), keyBytes.size());
        value.assign(valueBytes.data(), valueBytes.size());
        return true;
    }

    bool
    move(MoveOp op, std::string &seek, std::string &key, std::string &value)
    {
        key = seek;
        return move(op, key, value);
    }

    CursorHandle handle_;
};

inline Transaction
beginTransaction(Database &database, Transaction *parent = nullptr, AccessMode mode = AccessMode::ReadWrite)
{
    return database.beginTxn(parent, toAccessFlags(mode));
}

inline Transaction
beginReadTransaction(Database &database, Transaction *parent = nullptr)
{
    return beginTransaction(database, parent, AccessMode::ReadOnly);
}

inline Transaction
beginWriteTransaction(Database &database, Transaction *parent = nullptr)
{
    return beginTransaction(database, parent, AccessMode::ReadWrite);
}

inline Cursor
openCursor(Transaction &txn, Store &store)
{
    return Cursor::open(txn, store);
}

inline Store
openNamedStore(Database &database,
               Transaction &txn,
               std::string_view name,
               bool create = true,
               StoreFlags flags = StoreFlags::None)
{
    auto options      = openOptionsForName(name);
    options.flags   |= flags;
    if (create)
        options.flags |= StoreFlags::Create;

    requireCapabilities(database, options.flags);
    return database.openStore(txn, name, options);
}

inline Store
openStore(Database &database,
          Transaction &txn,
          std::string_view name,
          bool create = true,
          StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database, txn, name, create, flags);
}

inline Store
openStore(Database &database, Transaction &txn, std::string_view name, const Options &options)
{
    requireCapabilities(database, options.flags);
    return database.openStore(txn, name, options);
}

inline Store
openGlobalStore(Database &database, Transaction &txn, catalog::GlobalDb store, bool create = true)
{
    auto options = openOptionsForGlobal(store);
    if (create)
        options.flags |= StoreFlags::Create;

    return openStore(database, txn, catalog::globalName(store), options);
}

inline Store
openRoomStore(Database &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    auto options = openOptionsForRoom(store);
    if (create)
        options.flags |= StoreFlags::Create;

    return openStore(database, txn, catalog::roomName(roomId, store), options);
}

} // namespace db::storage
