// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

#include <algorithm>
#include <mutex>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/logger.h>

#include "MatrixClient.h"
#include "Utils.h"
#include "encryption/Olm.h"
#include "CacheApiWrappers.h"

std::optional<VerificationCache>
Cache::verificationCache(const std::string &user_id, db::Transaction &txn)
{
    auto db_ = getVerificationDb(txn);
    try {
        return db::getJsonValue<VerificationCache>(txn, db_, user_id);
    } catch (std::exception &) {
        return {};
    }
}

void
Cache::markDeviceVerified(const std::string &user_id, const std::string &key)
{
    auto txn = beginTxn();
    auto db_ = getVerificationDb(txn);

    try {
        VerificationCache verified_state;
        db::getJsonValue(txn, db_, user_id, verified_state);

        for (const auto &device : verified_state.device_verified)
            if (device == key)
                return;

        verified_state.device_verified.insert(key);
        db::putJsonValue(txn, db_, user_id, verified_state);
        txn.commit();
    } catch (std::exception &) {
    }

    const auto local_user = utils::localUser().toStdString();
    std::map<std::string, VerificationStatus> tmp;
    {
        std::unique_lock<std::mutex> lock(verification_storage.verification_storage_mtx);
        if (user_id == local_user) {
            std::swap(tmp, verification_storage.status);
            verification_storage.status.clear();
        } else {
            verification_storage.status.erase(user_id);
        }
    }

    if (user_id == local_user) {
        for (const auto &[user, status] : tmp) {
            (void)status;
            emit verificationStatusChanged(user);
        }
    } else {
        emit verificationStatusChanged(user_id);
    }
}

void
Cache::markDeviceUnverified(const std::string &user_id, const std::string &key)
{
    auto txn = beginTxn();
    auto db_ = getVerificationDb(txn);

    try {
        VerificationCache verified_state;
        db::getJsonValue(txn, db_, user_id, verified_state);

        verified_state.device_verified.erase(key);

        db::putJsonValue(txn, db_, user_id, verified_state);
        txn.commit();
    } catch (std::exception &) {
    }

    const auto local_user = utils::localUser().toStdString();
    std::map<std::string, VerificationStatus> tmp;
    {
        std::unique_lock<std::mutex> lock(verification_storage.verification_storage_mtx);
        if (user_id == local_user) {
            std::swap(tmp, verification_storage.status);
        } else {
            verification_storage.status.erase(user_id);
        }
    }
    if (user_id == local_user) {
        for (const auto &[user, status] : tmp) {
            (void)status;
            emit verificationStatusChanged(user);
        }
    }
    emit verificationStatusChanged(user_id);
}

VerificationStatus
Cache::verificationStatus(const std::string &user_id)
{
    auto txn = ro_txn(storage());
    return verificationStatus_(user_id, txn);
}

VerificationStatus
Cache::verificationStatus_(const std::string &user_id, db::Transaction &txn)
{
    std::unique_lock<std::mutex> lock(verification_storage.verification_storage_mtx);
    if (verification_storage.status.count(user_id))
        return verification_storage.status.at(user_id);

    VerificationStatus status;

    // assume there is at least one unverified device until we have checked we have the device
    // list for that user.
    status.unverified_device_count = 1;
    status.no_keys                 = true;

    if (auto verifCache = verificationCache(user_id, txn)) {
        status.verified_devices = verifCache->device_verified;
    }

    const auto local_user = utils::localUser().toStdString();

    crypto::Trust trustlevel = crypto::Trust::Unverified;
    if (user_id == local_user) {
        status.verified_devices.insert(http::client()->device_id());
        trustlevel = crypto::Trust::Verified;
    }

    auto verifyAtLeastOneSig = [](const auto &toVerif,
                                  const std::map<std::string, std::string> &keys,
                                  const std::string &keyOwner) {
        if (!toVerif.signatures.count(keyOwner))
            return false;

        for (const auto &[key_id, signature] : toVerif.signatures.at(keyOwner)) {
            if (!keys.count(key_id))
                continue;

            if (mtx::crypto::ed25519_verify_signature(
                  keys.at(key_id), nlohmann::json(toVerif), signature))
                return true;
        }
        return false;
    };

    auto updateUnverifiedDevices = [&status](auto &theirDeviceKeys) {
        int currentVerifiedDevices = 0;
        for (const auto &device_id : status.verified_devices) {
            if (theirDeviceKeys.count(device_id))
                currentVerifiedDevices++;
        }
        status.unverified_device_count =
          static_cast<int>(theirDeviceKeys.size()) - currentVerifiedDevices;
    };

    try {
        // for local user verify this device_key -> our master_key -> our self_signing_key
        // -> our device_keys
        //
        // for other user verify this device_key -> our master_key -> our user_signing_key
        // -> their master key -> their self_signing_key -> their device_keys
        //
        // This means verifying the other user adds 2 extra steps,verifying our user_signing
        // key and their master key
        auto ourKeys   = userKeys_(local_user, txn);
        auto theirKeys = userKeys_(user_id, txn);
        if (theirKeys)
            status.no_keys = false;

        if (!ourKeys || !theirKeys) {
            verification_storage.status[user_id] = status;
            return status;
        }

        // Update verified devices count to count without cross-signing
        updateUnverifiedDevices(theirKeys->device_keys);

        {
            auto &mk           = ourKeys->master_keys;
            std::string dev_id = "ed25519:" + http::client()->device_id();
            if (!mk.signatures.count(local_user) || !mk.signatures.at(local_user).count(dev_id) ||
                !mtx::crypto::ed25519_verify_signature(olm::client()->identity_keys().ed25519,
                                                       nlohmann::json(mk),
                                                       mk.signatures.at(local_user).at(dev_id))) {
                                    cache::activeLoggers().crypto->debug("We have not verified our own master key");
                verification_storage.status[user_id] = status;
                return status;
            }
        }

        auto master_keys = ourKeys->master_keys.keys;

        if (user_id != local_user) {
            bool theirMasterKeyVerified =
              verifyAtLeastOneSig(ourKeys->user_signing_keys, master_keys, local_user) &&
              verifyAtLeastOneSig(
                theirKeys->master_keys, ourKeys->user_signing_keys.keys, local_user);

            if (theirMasterKeyVerified)
                trustlevel = crypto::Trust::Verified;
            else if (!theirKeys->master_key_changed)
                trustlevel = crypto::Trust::TOFU;
            else {
                verification_storage.status[user_id] = status;
                return status;
            }

            master_keys = theirKeys->master_keys.keys;
        }

        status.user_verified = trustlevel;

        verification_storage.status[user_id] = status;
        if (!verifyAtLeastOneSig(theirKeys->self_signing_keys, master_keys, user_id))
            return status;

        for (const auto &[device, device_key] : theirKeys->device_keys) {
            (void)device;
            try {
                auto identkey = device_key.keys.at("curve25519:" + device_key.device_id);
                if (verifyAtLeastOneSig(device_key, theirKeys->self_signing_keys.keys, user_id)) {
                    status.verified_devices.insert(device_key.device_id);
                    status.verified_device_keys[identkey] = trustlevel;
                }
            } catch (...) {
            }
        }

        updateUnverifiedDevices(theirKeys->device_keys);
        verification_storage.status[user_id] = status;
        return status;
    } catch (std::exception &e) {
                    cache::activeLoggers().db->error("Failed to calculate verification status of {}: {}", user_id, e.what());
        return status;
    }
}
