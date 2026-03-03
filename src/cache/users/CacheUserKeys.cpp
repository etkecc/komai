// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <mutex>

#include <nlohmann/json.hpp>
#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"
#include "encryption/Olm.h"
#include "utils/Utils.h"

std::optional<UserKeyCache>
MatrixStore::userKeys(const std::string &user_id)
{
    auto txn = ro_txn(storage());
    return userKeys_(user_id, txn);
}

std::optional<UserKeyCache>
MatrixStore::userKeys_(const std::string &user_id, db::Transaction &txn)
{
    try {
        auto db_ = getUserKeysDb(txn);
        return db::getJsonValue<UserKeyCache>(txn, db_, user_id);
    } catch (std::exception &e) {
        cache::activeLoggers().db->error(
          "Failed to retrieve user keys for {}: {}", user_id, e.what());
        return std::nullopt;
    }
}

void
MatrixStore::updateUserKeys(const std::string &sync_token,
                            const mtx::responses::QueryKeys &keyQuery)
{
    auto txn = beginTxn();
    auto db_ = getUserKeysDb(txn);

    std::map<std::string, UserKeyCache> updates;

    for (const auto &[user, keys] : keyQuery.device_keys)
        updates[user].device_keys = keys;
    for (const auto &[user, keys] : keyQuery.master_keys)
        updates[user].master_keys = keys;
    for (const auto &[user, keys] : keyQuery.user_signing_keys)
        updates[user].user_signing_keys = keys;
    for (const auto &[user, keys] : keyQuery.self_signing_keys)
        updates[user].self_signing_keys = keys;

    for (auto &[user, update] : updates) {
        cache::activeLoggers().db->debug("Updated user keys: {}", user);

        auto updateToWrite = update;

        UserKeyCache oldEntry{};
        try {
            if (db::getJsonValue(txn, db_, user, oldEntry)) {
                updateToWrite     = oldEntry;
                auto last_changed = updateToWrite.last_changed;
                // skip if we are tracking this and expect it to be up to date with the last
                // sync token
                if (!last_changed.empty() && last_changed != sync_token) {
                    cache::activeLoggers().db->debug(
                      "Not storing update for user {}, because "
                      "last_changed {}, but we fetched update for {}",
                      user,
                      last_changed,
                      sync_token);
                    continue;
                }

                if (!updateToWrite.master_keys.keys.empty() &&
                    update.master_keys.keys != updateToWrite.master_keys.keys) {
                    cache::activeLoggers().db->debug("Master key of {} changed:\nold: {}\nnew: {}",
                                                     user,
                                                     updateToWrite.master_keys.keys.size(),
                                                     update.master_keys.keys.size());
                    updateToWrite.master_key_changed = true;
                }

                updateToWrite.master_keys       = update.master_keys;
                updateToWrite.self_signing_keys = update.self_signing_keys;
                updateToWrite.user_signing_keys = update.user_signing_keys;

                auto oldDeviceKeys = std::move(updateToWrite.device_keys);
                updateToWrite.device_keys.clear();

                // Don't insert keys, which we have seen once already
                for (const auto &[device_id, device_keys] : update.device_keys) {
                    if (oldDeviceKeys.count(device_id) &&
                        oldDeviceKeys.at(device_id).keys == device_keys.keys) {
                        // this is safe, since the keys are the same
                        updateToWrite.device_keys[device_id] = device_keys;
                    } else {
                        bool keyReused = false;
                        for (const auto &[key_id, key] : device_keys.keys) {
                            (void)key_id;
                            if (updateToWrite.seen_device_keys.count(key)) {
                                cache::activeLoggers().crypto->warn(
                                  "Key '{}' reused by ({}: {})", key, user, device_id);
                                keyReused = true;
                                break;
                            }
                            if (updateToWrite.seen_device_ids.count(device_id)) {
                                cache::activeLoggers().crypto->warn(
                                  "device_id '{}' reused by ({})", device_id, user);
                                keyReused = true;
                                break;
                            }
                        }

                        if (!keyReused && !oldDeviceKeys.count(device_id)) {
                            // ensure the key has a valid signature from itself
                            std::string device_signing_key = "ed25519:" + device_keys.device_id;
                            if (device_id != device_keys.device_id) {
                                cache::activeLoggers().crypto->warn(
                                  "device {}:{} has a different device id "
                                  "in the body: {}",
                                  user,
                                  device_id,
                                  device_keys.device_id);
                                continue;
                            }
                            if (!device_keys.signatures.count(user) ||
                                !device_keys.signatures.at(user).count(device_signing_key)) {
                                cache::activeLoggers().crypto->warn(
                                  "device {}:{} has no signature", user, device_id);
                                continue;
                            }
                            if (!device_keys.keys.count(device_signing_key) ||
                                !device_keys.keys.count("curve25519:" + device_id)) {
                                cache::activeLoggers().crypto->warn(
                                  "Device key has no curve25519 or ed25519 key  {}:{}",
                                  user,
                                  device_id);
                                continue;
                            }

                            if (!mtx::crypto::ed25519_verify_signature(
                                  device_keys.keys.at(device_signing_key),
                                  nlohmann::json(device_keys),
                                  device_keys.signatures.at(user).at(device_signing_key))) {
                                cache::activeLoggers().crypto->warn(
                                  "device {}:{} has an invalid signature", user, device_id);
                                continue;
                            }

                            updateToWrite.device_keys[device_id] = device_keys;
                        }
                    }

                    for (const auto &[key_id, key] : device_keys.keys) {
                        (void)key_id;
                        updateToWrite.seen_device_keys.insert(key);
                    }
                    updateToWrite.seen_device_ids.insert(device_id);
                }
            }
        } catch (const std::exception &e) {
            cache::activeLoggers().db->warn(
              "Could not parse existing user key cache for {} ({}). Replacing entry.",
              user,
              e.what());
        }
        updateToWrite.updated_at = sync_token;
        db::putJsonValue(txn, db_, user, updateToWrite);
    }

    txn.commit();

    std::map<std::string, VerificationStatus> tmp;
    const auto local_user = utils::localUser().toStdString();

    {
        std::unique_lock<std::mutex> lock(verification_storage.verification_storage_mtx);
        for (auto &[user_id, update] : updates) {
            (void)update;
            if (user_id == local_user) {
                std::swap(tmp, verification_storage.status);
            } else {
                verification_storage.status.erase(user_id);
            }
        }
    }

    for (auto &[user_id, update] : updates) {
        (void)update;
        if (user_id == local_user) {
            for (const auto &[user, status] : tmp) {
                (void)status;
                emit verificationStatusChanged(user);
            }
        } else {
            emit verificationStatusChanged(user_id);
        }
    }
}

void
MatrixStore::markUserKeysOutOfDate(const std::vector<std::string> &user_ids)
{
    auto currentBatchToken = nextBatchToken();
    auto txn               = beginTxn();
    auto db_               = getUserKeysDb(txn);
    markUserKeysOutOfDate(txn, db_, user_ids, currentBatchToken);
    txn.commit();
}
