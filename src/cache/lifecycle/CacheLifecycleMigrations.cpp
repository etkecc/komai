// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <exception>
#include <string_view>

#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"
#include "cache/schema/CacheSchema.h"

//! Should be changed when a breaking change occurs in the cache format.
//! Older formats are reset (no legacy migration chain retained).
static constexpr std::string_view CURRENT_CACHE_FORMAT_VERSION{"2026.03.11"};

//! migrates db to the current format
bool
MatrixStore::runMigrations()
{
    std::string stored_version;
    {
        auto txn            = ro_txn(storage());
        auto currentVersion = cache::sync_state::getCacheFormatVersion(txn, db->syncState);
        stored_version      = currentVersion.value_or("");
    }

    if (stored_version == CURRENT_CACHE_FORMAT_VERSION)
        return true;

    cache::activeLoggers().db->warn(
      "Resetting incompatible cache format '{}'; target format is '{}'.",
      stored_version.empty() ? "<unset>" : stored_version,
      CURRENT_CACHE_FORMAT_VERSION);

    try {
        auto txn                = beginTxn(nullptr);
        const auto syncStateDbi = db::catalog::globalName(db::catalog::GlobalDb::SyncState);
        for (const auto &dbName : storage().listStoreNames(txn)) {
            if (dbName == syncStateDbi)
                continue;

            try {
                db::openNamedStore(storage(), txn, dbName, false).drop(txn, true);
            } catch (const std::exception &e) {
                cache::activeLoggers().db->warn(
                  "Failed to drop '{}' while resetting incompatible cache: {}", dbName, e.what());
            }
        }

        cache::sync_state::putNextBatchToken(txn, db->syncState, "");
        cache::sync_state::putCacheFormatVersion(txn, db->syncState, CURRENT_CACHE_FORMAT_VERSION);
        txn.commit();
    } catch (const db::Error &e) {
        cache::activeLoggers().db->critical("Failed to reset incompatible cache format '{}': {}",
                                            stored_version.empty() ? "<unset>" : stored_version,
                                            e.what());
        return false;
    }

    cache::activeLoggers().db->info("Incompatible cache reset completed.");
    return true;
}

cache::CacheVersion
MatrixStore::formatVersion()
{
    auto txn            = ro_txn(storage());
    auto currentVersion = cache::sync_state::getCacheFormatVersion(txn, db->syncState);
    if (!currentVersion.has_value())
        return cache::CacheVersion::Older;

    std::string stored_version = *currentVersion;

    if (stored_version < CURRENT_CACHE_FORMAT_VERSION)
        return cache::CacheVersion::Older;
    if (stored_version > CURRENT_CACHE_FORMAT_VERSION)
        return cache::CacheVersion::Newer;

    return cache::CacheVersion::Current;
}

void
MatrixStore::setCurrentFormat()
{
    auto txn = beginTxn();
    cache::sync_state::putCacheFormatVersion(txn, db->syncState, CURRENT_CACHE_FORMAT_VERSION);

    txn.commit();
}
