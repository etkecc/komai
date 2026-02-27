// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/NamePolicy.h"
#include "db/storage/Catalog.h"
#include "db/storage/Core.h"

namespace db::storage {

inline StoreOpenOptions
openOptionsForName(std::string_view name)
{
    return ::db::openOptionsForName(name);
}

inline StoreOpenOptions
openOptionsForGlobal(catalog::GlobalDb db)
{
    return ::db::openOptionsForGlobal(db);
}

inline StoreOpenOptions
openOptionsForRoom(catalog::RoomDb db)
{
    return ::db::openOptionsForRoom(db);
}

inline Store
openNamedStore(Database &database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    auto options = openOptionsForName(name);
    options.flags |= flags;
    if (create)
        options.flags |= StoreFlags::Create;

    db::storage::requireCapabilities(database, options.flags);
    return database.openStore(txn, name, options);
}

inline Store
openNamedStore(Database *database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return openNamedStore(*database, txn, name, create, flags);
}

inline Store
openNamedStore(std::unique_ptr<Database> &database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database.get(), txn, name, create, flags);
}

inline Store
openNamedStore(const std::unique_ptr<Database> &database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database.get(), txn, name, create, flags);
}

inline Store
openStore(Database &database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database, txn, name, create, flags);
}

inline Store
openStore(Database *database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return openNamedStore(database, txn, name, create, flags);
}

inline Store
openStore(std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database, txn, name, create, flags);
}

inline Store
openStore(const std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database, txn, name, create, flags);
}

inline Store
openStore(Database &database, Transaction &txn, std::string_view name, const Options &options)
{
    db::storage::requireCapabilities(database, options.flags);
    return database.openStore(txn, name, options);
}

inline Store
openStore(Database *database, Transaction &txn, std::string_view name, const Options &options)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return openStore(*database, txn, name, options);
}

inline Store
openStore(std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          const Options &options)
{
    return openStore(database.get(), txn, name, options);
}

inline Store
openStore(const std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          const Options &options)
{
    return openStore(database.get(), txn, name, options);
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
openGlobalStore(Database *database, Transaction &txn, catalog::GlobalDb store, bool create = true)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return openGlobalStore(*database, txn, store, create);
}

inline Store
openGlobalStore(std::unique_ptr<Database> &database,
                Transaction &txn,
                catalog::GlobalDb store,
                bool create = true)
{
    return openGlobalStore(database.get(), txn, store, create);
}

inline Store
openGlobalStore(const std::unique_ptr<Database> &database,
                Transaction &txn,
                catalog::GlobalDb store,
                bool create = true)
{
    return openGlobalStore(database.get(), txn, store, create);
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

inline Store
openRoomStore(Database *database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return openRoomStore(*database, txn, roomId, store, create);
}

inline Store
openRoomStore(std::unique_ptr<Database> &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return openRoomStore(database.get(), txn, roomId, store, create);
}

inline Store
openRoomStore(const std::unique_ptr<Database> &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return openRoomStore(database.get(), txn, roomId, store, create);
}

} // namespace db::storage

namespace db {

inline Store
openNamedStore(Database &database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    return storage::openNamedStore(database, txn, name, create, flags);
}

inline Store
openNamedStore(Database *database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    return storage::openNamedStore(database, txn, name, create, flags);
}

inline Store
openNamedStore(std::unique_ptr<Database> &database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    return storage::openNamedStore(database, txn, name, create, flags);
}

inline Store
openNamedStore(const std::unique_ptr<Database> &database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    return storage::openNamedStore(database, txn, name, create, flags);
}

inline Store
openStore(Database &database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return storage::openStore(database, txn, name, create, flags);
}

inline Store
openStore(Database *database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return storage::openStore(database, txn, name, create, flags);
}

inline Store
openStore(std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return storage::openStore(database, txn, name, create, flags);
}

inline Store
openStore(const std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return storage::openStore(database, txn, name, create, flags);
}

inline Store
openStore(Database &database,
          Transaction &txn,
          std::string_view name,
          const StoreOpenOptions &options)
{
    return storage::openStore(database, txn, name, options);
}

inline Store
openStore(Database *database,
          Transaction &txn,
          std::string_view name,
          const StoreOpenOptions &options)
{
    return storage::openStore(database, txn, name, options);
}

inline Store
openStore(std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          const StoreOpenOptions &options)
{
    return storage::openStore(database, txn, name, options);
}

inline Store
openStore(const std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          const StoreOpenOptions &options)
{
    return storage::openStore(database, txn, name, options);
}

inline Store
openGlobalStore(Database &database, Transaction &txn, catalog::GlobalDb store, bool create = true)
{
    return storage::openGlobalStore(database, txn, store, create);
}

inline Store
openGlobalStore(Database *database, Transaction &txn, catalog::GlobalDb store, bool create = true)
{
    return storage::openGlobalStore(database, txn, store, create);
}

inline Store
openGlobalStore(std::unique_ptr<Database> &database,
                Transaction &txn,
                catalog::GlobalDb store,
                bool create = true)
{
    return storage::openGlobalStore(database, txn, store, create);
}

inline Store
openGlobalStore(const std::unique_ptr<Database> &database,
                Transaction &txn,
                catalog::GlobalDb store,
                bool create = true)
{
    return storage::openGlobalStore(database, txn, store, create);
}

inline Store
openRoomStore(Database &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return storage::openRoomStore(database, txn, roomId, store, create);
}

inline Store
openRoomStore(Database *database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return storage::openRoomStore(database, txn, roomId, store, create);
}

inline Store
openRoomStore(std::unique_ptr<Database> &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return storage::openRoomStore(database, txn, roomId, store, create);
}

inline Store
openRoomStore(const std::unique_ptr<Database> &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return storage::openRoomStore(database, txn, roomId, store, create);
}

} // namespace db
