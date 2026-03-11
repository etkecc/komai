// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cache/schema/CacheSchema.h"
#include "db/Scan.h"

namespace cache::room_store {

enum class SharedStore
{
    Plain,
    Ordered,
    Dupsort,
};

inline constexpr SharedStore
sharedStore(schema::RoomDb db) noexcept
{
    switch (db) {
    case schema::RoomDb::Events:
    case schema::RoomDb::EventToOrder:
    case schema::RoomDb::MessageToOrder:
    case schema::RoomDb::InviteState:
    case schema::RoomDb::InviteMembers:
    case schema::RoomDb::State:
    case schema::RoomDb::AccountData:
    case schema::RoomDb::Members:
        return SharedStore::Plain;
    case schema::RoomDb::EventOrder:
    case schema::RoomDb::OrderToMessage:
    case schema::RoomDb::Pending:
        return SharedStore::Ordered;
    case schema::RoomDb::Related:
    case schema::RoomDb::StatesKey:
        return SharedStore::Dupsort;
    }

    return SharedStore::Plain;
}

inline constexpr schema::GlobalDb
globalStore(SharedStore store) noexcept
{
    switch (store) {
    case SharedStore::Plain:
        return schema::GlobalDb::SharedRoomPlain;
    case SharedStore::Ordered:
        return schema::GlobalDb::SharedRoomOrdered;
    case SharedStore::Dupsort:
        return schema::GlobalDb::SharedRoomDupsort;
    }

    return schema::GlobalDb::SharedRoomPlain;
}

inline constexpr std::string_view
logicalStoreTag(schema::RoomDb db) noexcept
{
    switch (db) {
    case schema::RoomDb::Events:
        return "events";
    case schema::RoomDb::EventOrder:
        return "event_order";
    case schema::RoomDb::EventToOrder:
        return "event2order";
    case schema::RoomDb::MessageToOrder:
        return "msg2order";
    case schema::RoomDb::OrderToMessage:
        return "order2msg";
    case schema::RoomDb::Pending:
        return "pending";
    case schema::RoomDb::Related:
        return "related";
    case schema::RoomDb::InviteState:
        return "invite_state";
    case schema::RoomDb::InviteMembers:
        return "invite_members";
    case schema::RoomDb::State:
        return "state";
    case schema::RoomDb::StatesKey:
        return "states_key";
    case schema::RoomDb::AccountData:
        return "account_data";
    case schema::RoomDb::Members:
        return "members";
    }

    return "events";
}

inline std::string
prefix(schema::RoomDb db, std::string_view roomId)
{
    const auto tag = logicalStoreTag(db);

    std::string key;
    key.reserve(tag.size() + 1 + roomId.size() + 1);
    key.append(tag);
    key.push_back('\0');
    key.append(roomId);
    key.push_back('\0');
    return key;
}

inline std::string
key(schema::RoomDb db, std::string_view roomId, std::string_view subkey)
{
    std::string composite = prefix(db, roomId);
    composite.append(subkey);
    return composite;
}

inline std::optional<std::string_view>
subkeyFromKey(schema::RoomDb db, std::string_view roomId, std::string_view compositeKey) noexcept
{
    const auto keyPrefix = prefix(db, roomId);
    if (!compositeKey.starts_with(keyPrefix))
        return std::nullopt;

    return compositeKey.substr(keyPrefix.size());
}

inline bool
get(db::Transaction &txn,
    db::Store &store,
    schema::RoomDb db,
    std::string_view roomId,
    std::string_view subkey,
    std::string_view &value)
{
    return store.get(txn, key(db, roomId, subkey), value);
}

inline bool
put(db::Transaction &txn,
    db::Store &store,
    schema::RoomDb db,
    std::string_view roomId,
    std::string_view subkey,
    std::string_view value,
    db::PutFlags flags = db::PutFlags::None)
{
    return store.put(txn, key(db, roomId, subkey), value, flags);
}

inline bool
del(db::Transaction &txn,
    db::Store &store,
    schema::RoomDb db,
    std::string_view roomId,
    std::string_view subkey)
{
    return store.del(txn, key(db, roomId, subkey));
}

inline bool
del(db::Transaction &txn,
    db::Store &store,
    schema::RoomDb db,
    std::string_view roomId,
    std::string_view subkey,
    std::string_view value)
{
    return store.del(txn, key(db, roomId, subkey), value);
}

inline std::size_t
countEntries(db::Transaction &txn, db::Store &store, schema::RoomDb db, std::string_view roomId)
{
    std::size_t count    = 0;
    const auto keyPrefix = prefix(db, roomId);
    db::forEachEntryWithPrefix(txn, store, keyPrefix, [&count](std::string_view, std::string_view) {
        count += 1;
        return true;
    });
    return count;
}

inline std::vector<std::string>
listKeys(db::Transaction &txn, db::Store &store, schema::RoomDb db, std::string_view roomId)
{
    std::vector<std::string> keys;
    const auto keyPrefix = prefix(db, roomId);
    db::forEachEntryWithPrefix(
      txn, store, keyPrefix, [&keys, &keyPrefix](std::string_view compositeKey, std::string_view) {
          keys.emplace_back(compositeKey.substr(keyPrefix.size()));
          return true;
      });
    return keys;
}

inline void
forEachEntry(db::Transaction &txn,
             db::Store &store,
             schema::RoomDb db,
             std::string_view roomId,
             const std::function<bool(std::string_view subkey, std::string_view value)> &visitor)
{
    const auto keyPrefix = prefix(db, roomId);
    db::forEachEntryWithPrefix(
      txn,
      store,
      keyPrefix,
      [&visitor, &keyPrefix](std::string_view compositeKey, std::string_view value) {
          return visitor(compositeKey.substr(keyPrefix.size()), value);
      });
}

inline void
forEachEntry(db::Transaction &txn,
             db::Store &store,
             schema::RoomDb db,
             std::string_view roomId,
             std::size_t startIndex,
             std::size_t limit,
             const std::function<bool(std::string_view subkey, std::string_view value)> &visitor)
{
    if (limit == 0)
        return;

    std::size_t currentIndex = 0;
    std::size_t remaining    = limit;
    forEachEntry(txn,
                 store,
                 db,
                 roomId,
                 [&visitor, &currentIndex, &remaining, startIndex](std::string_view subkey,
                                                                   std::string_view value) {
                     if (currentIndex < startIndex) {
                         currentIndex += 1;
                         return true;
                     }
                     if (remaining == 0)
                         return false;

                     currentIndex += 1;
                     remaining -= 1;
                     return visitor(subkey, value);
                 });
}

inline std::size_t
eraseEntries(db::Transaction &txn, db::Store &store, schema::RoomDb db, std::string_view roomId)
{
    std::vector<std::pair<std::string, std::string>> entriesToDelete;
    const auto keyPrefix = prefix(db, roomId);
    db::forEachEntryWithPrefix(
      txn,
      store,
      keyPrefix,
      [&entriesToDelete](std::string_view compositeKey, std::string_view value) {
          entriesToDelete.emplace_back(std::string(compositeKey), std::string(value));
          return true;
      });

    for (const auto &[compositeKey, value] : entriesToDelete)
        store.del(txn, compositeKey, value);

    return entriesToDelete.size();
}

inline std::string
orderedIndexSubkey(std::uint64_t index)
{
    std::string subkey(sizeof(index), '\0');
    for (std::size_t i = 0; i < sizeof(index); ++i) {
        const auto shift = static_cast<unsigned>((sizeof(index) - 1 - i) * 8);
        subkey[i]        = static_cast<char>((index >> shift) & 0xffU);
    }
    return subkey;
}

inline std::optional<std::uint64_t>
orderedIndexFromSubkey(std::string_view rawIndex)
{
    if (rawIndex.size() != sizeof(std::uint64_t))
        return std::nullopt;

    std::uint64_t index = 0;
    for (unsigned char byte : rawIndex)
        index = (index << 8U) | byte;

    return index;
}

inline std::string
orderedIndexKey(schema::RoomDb db, std::string_view roomId, std::uint64_t index)
{
    std::string composite = prefix(db, roomId);
    composite.append(orderedIndexSubkey(index));
    return composite;
}

inline std::optional<std::uint64_t>
orderedIndexFromKey(schema::RoomDb db, std::string_view roomId, std::string_view compositeKey)
{
    const auto keyPrefix = prefix(db, roomId);
    if (!compositeKey.starts_with(keyPrefix))
        return std::nullopt;

    return orderedIndexFromSubkey(compositeKey.substr(keyPrefix.size()));
}

inline std::size_t
eraseOrderedEntries(db::Transaction &txn,
                    db::Store &store,
                    schema::RoomDb db,
                    std::string_view roomId)
{
    return eraseEntries(txn, store, db, roomId);
}

inline std::size_t
eraseDupsortEntries(db::Transaction &txn,
                    db::Store &store,
                    schema::RoomDb db,
                    std::string_view roomId)
{
    return eraseEntries(txn, store, db, roomId);
}

} // namespace cache::room_store

namespace room_store = cache::room_store;
