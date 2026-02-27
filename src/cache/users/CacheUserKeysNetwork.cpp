// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <mtx/requests.hpp>

#include <spdlog/logger.h>

#include "MatrixClient.h"
#include "cache/api/CacheApiContext.h"

void
MatrixStore::markUserKeysOutOfDate(db::Transaction &txn,
                                   db::Store &db_,
                                   const std::vector<std::string> &user_ids,
                                   const std::string &sync_token)
{
    mtx::requests::QueryKeys query;
    query.token = sync_token;

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

        query.device_keys[user] = {};

        if (query.device_keys.size() >= 32) {
            http::client()->query_keys(
              query,
              [this, sync_token](const mtx::responses::QueryKeys &keys, mtx::http::RequestErr err) {
                  if (err) {
                      cache::activeLoggers().net->warn("failed to query device keys: {} {}",
                                                       err->matrix_error.error,
                                                       static_cast<int>(err->status_code));
                      return;
                  }

                  emit userKeysUpdate(sync_token, keys);
              });
            query.device_keys.clear();
        }
    }

    if (!query.device_keys.empty())
        http::client()->query_keys(
          query,
          [this, sync_token](const mtx::responses::QueryKeys &keys, mtx::http::RequestErr err) {
              if (err) {
                  cache::activeLoggers().net->warn("failed to query device keys: {} {}",
                                                   err->matrix_error.error,
                                                   static_cast<int>(err->status_code));
                  return;
              }

              emit userKeysUpdate(sync_token, keys);
          });
}

void
MatrixStore::query_keys(const std::string &user_id,
                        std::function<void(const UserKeyCache &, mtx::http::RequestErr)> cb)
{
    if (user_id.size() > 255) {
        cache::activeLoggers().db->debug("Skipping device key query for user with invalid mxid: {}",
                                         user_id);

        mtx::http::ClientError err{};
        err.parse_error = "invalid mxid, more than 255 bytes";
        cb({}, err);
        return;
    }

    mtx::requests::QueryKeys req;
    std::string last_changed;
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

        req.device_keys[user_id] = {};

        if (cache_)
            last_changed = cache_->last_changed;
        req.token = last_changed;
    }

    // use context object so that we can disconnect again
    QObject *context{new QObject(this)};
    QObject::connect(
      this,
      &MatrixStore::userKeysUpdateFinalize,
      context,
      [cb, user_id, context_ = context, this](std::string updated_user) mutable {
          if (user_id == updated_user) {
              context_->deleteLater();
              auto txn  = ro_txn(storage());
              auto keys = this->userKeys_(user_id, txn);
              cb(keys.value_or(UserKeyCache{}), {});
          }
      },
      Qt::QueuedConnection);

    http::client()->query_keys(
      req,
      [cb, user_id, last_changed, this](const mtx::responses::QueryKeys &res,
                                        mtx::http::RequestErr err) {
          if (err) {
              cache::activeLoggers().net->warn("failed to query device keys: {},{}",
                                               mtx::errors::to_string(err->matrix_error.errcode),
                                               static_cast<int>(err->status_code));
              cb({}, err);
              return;
          }

          emit userKeysUpdate(last_changed, res);
          emit userKeysUpdateFinalize(user_id);
      });
}
