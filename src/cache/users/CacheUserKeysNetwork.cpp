// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"

void
MatrixStore::markUserKeysOutOfDate(db::Transaction &txn,
                                   db::Store &db_,
                                   const std::vector<std::string> &user_ids,
                                   const std::string &sync_token)
{
    for (const auto &user : user_ids) {
        if (user.size() > 255) {
            cache::activeLoggers().db->debug(
              "Skipping device key query for user with invalid mxid: {}", user);
            continue;
        }

        cache::activeLoggers().db->debug("Marking user keys out of date: {}", user);

        UserKeyCache cacheEntry{};
        try {
            db::getJsonValue(txn, db_, user, cacheEntry);
        } catch (std::exception &e) {
            cache::activeLoggers().db->error("Failed to parse {}: {}", user, e.what());
        }
        cacheEntry.last_changed = sync_token;

        db::putJsonValue(txn, db_, user, cacheEntry);
    }

    if (!user_ids.empty())
        cache::activeLoggers().crypto->warn(
          "Skipping legacy user-key refresh for {} users during the matrix-sdk migration",
          user_ids.size());
}

void
MatrixStore::query_keys(
  const std::string &user_id,
  std::function<void(const UserKeyCache &, const std::optional<mtx::http::ClientError> &)> cb)
{
    if (user_id.size() > 255) {
        cache::activeLoggers().db->debug("Skipping device key query for user with invalid mxid: {}",
                                         user_id);

        mtx::http::ClientError err{};
        err.parse_error = "invalid mxid, more than 255 bytes";
        cb({}, err);
        return;
    }

    std::optional<UserKeyCache> cachedKeys;
    {
        auto txn    = ro_txn(storage());
        auto cache_ = userKeys_(user_id, txn);

        if (cache_.has_value()) {
            if (cache_->updated_at == cache_->last_changed) {
                cb(cache_.value(), {});
                return;
            } else
                cache::activeLoggers().db->info("Keys outdated for {}: {} vs {}",
                                                user_id,
                                                cache_->updated_at,
                                                cache_->last_changed);
        } else
            cache::activeLoggers().db->info("No keys found for {}", user_id);
        cachedKeys = cache_;
    }

    cache::activeLoggers().crypto->warn(
      "Legacy device-key query for {} is unavailable during the matrix-sdk migration", user_id);

    mtx::http::ClientError err{};
    err.parse_error = "legacy device-key query is unavailable during the matrix-sdk migration";
    cb(cachedKeys.value_or(UserKeyCache{}), err);
}
