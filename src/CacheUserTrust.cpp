// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include <spdlog/logger.h>

#include "CacheApiWrappers.h"
#include "MatrixClient.h"
#include "Utils.h"

crypto::Trust
Cache::roomVerificationStatus(const std::string &room_id)
{
    crypto::Trust trust = crypto::Verified;

    try {
        auto txn = beginTxn();

        auto db_    = getMembersDb(txn, room_id);
        auto keysDb = getUserKeysDb(txn);
        std::vector<std::string> keysToRequest;

        db::forEachUniqueKey(
          txn, db_, [&keysToRequest, &trust, &txn, this](std::string_view user_id) {
              const auto userId = std::string(user_id);
              auto verif        = verificationStatus_(userId, txn);
              if (verif.unverified_device_count) {
                  trust = crypto::Unverified;
                  if (verif.verified_devices.empty() && verif.no_keys) {
                      // we probably don't have the keys yet, so query them
                      keysToRequest.push_back(userId);
                  }
              } else if (verif.user_verified == crypto::TOFU && trust == crypto::Verified)
                  trust = crypto::TOFU;
              return true;
          });

        if (!keysToRequest.empty()) {
            markUserKeysOutOfDate(
              txn,
              keysDb,
              keysToRequest,
              db::getSyncStateValue(txn, this->db->syncState, db::catalog::SyncStateKey::NextBatch)
                .value_or(""));
        }

    } catch (std::exception &e) {
        if (const auto logger = cache::activeLoggers().db)
            logger->error("Failed to calculate verification status for {}: {}", room_id, e.what());
        trust = crypto::Unverified;
    }

    return trust;
}

std::map<std::string, std::optional<UserKeyCache>>
Cache::getMembersWithKeys(const std::string &room_id, bool verified_only)
{
    try {
        auto txn = ro_txn(storage());
        std::map<std::string, std::optional<UserKeyCache>> members;

        auto db_    = getMembersDb(txn, room_id);
        auto keysDb = getUserKeysDb(txn);

        db::forEachUniqueKey(
          txn, db_, [&members, &keysDb, &txn, verified_only, this](std::string_view user_id) {
              const auto userId = std::string(user_id);
              if (auto k = db::getJsonValue<UserKeyCache>(txn, keysDb, userId)) {
                  if (verified_only) {
                      auto verif = verificationStatus_(userId, txn);

                      if (verif.user_verified == crypto::Trust::Verified ||
                          !verif.verified_devices.empty()) {
                          auto keyCopy = *k;
                          keyCopy.device_keys.clear();

                          std::copy_if(
                            k->device_keys.begin(),
                            k->device_keys.end(),
                            std::inserter(keyCopy.device_keys, keyCopy.device_keys.end()),
                            [&verif](const auto &key) {
                                auto curve25519 = key.second.keys.find("curve25519:" + key.first);
                                if (curve25519 == key.second.keys.end())
                                    return false;
                                if (auto t = verif.verified_device_keys.find(curve25519->second);
                                    t == verif.verified_device_keys.end() ||
                                    t->second != crypto::Trust::Verified)
                                    return false;

                                return key.first == key.second.device_id &&
                                       std::find(verif.verified_devices.begin(),
                                                 verif.verified_devices.end(),
                                                 key.first) != verif.verified_devices.end();
                            });

                          if (!keyCopy.device_keys.empty())
                              members[userId] = std::move(keyCopy);
                      }
                  } else {
                      members[userId] = std::move(*k);
                  }
              } else {
                  if (!verified_only)
                      members[userId] = {};
              }
              return true;
          });

        return members;
    } catch (std::exception &e) {
        if (const auto logger = cache::activeLoggers().db)
            logger->debug("Error retrieving members: {}", e.what());
        return {};
    }
}
