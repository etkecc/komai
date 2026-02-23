// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string_view>

#include "db/DbTypes.h"

namespace db {

class Txn;
class Dbi;

using Transaction = Txn;
using Store       = Dbi;

template<typename T>
std::optional<T>
parseJsonValue(std::string_view raw)
{
    try {
        return nlohmann::json::parse(raw).template get<T>();
    } catch (const nlohmann::json::exception &) {
        return std::nullopt;
    }
}

template<typename T>
bool
parseJsonValue(std::string_view raw, T &value)
{
    const auto parsed = parseJsonValue<T>(raw);
    if (!parsed)
        return false;

    value = std::move(*parsed);
    return true;
}

template<typename T, typename K>
std::optional<T>
getJsonValue(Transaction &txn, Store &db, const K &key)
{
    std::string_view raw;
    if (!db.get(txn, key, raw))
        return std::nullopt;

    return nlohmann::json::parse(std::string_view(raw.data(), raw.size())).template get<T>();
}

template<typename T, typename K>
bool
getJsonValue(Transaction &txn, Store &db, const K &key, T &value)
{
    const auto parsed = getJsonValue<T>(txn, db, key);
    if (!parsed)
        return false;

    value = std::move(*parsed);
    return true;
}

template<typename T, typename K>
void
putJsonValue(Transaction &txn,
             Store &db,
             const K &key,
             const T &value,
             PutFlags flags = PutFlags::None)
{
    db.put(txn, key, nlohmann::json(value).dump(), flags);
}

} // namespace db
