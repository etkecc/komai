// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

#include <functional>
#include <string_view>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <mtx/secret_storage.hpp>

#include <spdlog/logger.h>

#include "CacheApiWrappers.h"
#include "db/Maintenance.h"

//! Should be changed when a breaking change occurs in the cache format.
//! This will reset client's data.
static constexpr std::string_view CURRENT_CACHE_FORMAT_VERSION{"2023.10.22"};

namespace cache::detail {

std::vector<std::pair<std::string, std::function<bool()>>>
buildPreMigrations(Cache *cache);

std::vector<std::pair<std::string, std::function<bool()>>>
buildPostMigrations(Cache *cache);
}

//! migrates db to the current format
bool
Cache::runMigrations()
{
    std::string stored_version;
    {
        auto txn = ro_txn(storage());
        auto currentVersion =
          db::getSyncStateValue(txn, db->syncState, db::catalog::SyncStateKey::CacheFormatVersion);

        if (!currentVersion.has_value())
            return false;

        stored_version = std::move(*currentVersion);
    }

    auto migrations = cache::detail::buildPreMigrations(this);
    migrations.emplace_back("2021.08.22", [this]() {
        try {
            auto txn      = beginTxn(nullptr);
            auto room_ids = getRoomIds(txn);

            for (const auto &room : room_ids) {
                for (const auto roomDb : db::maintenance::roomDbsForFullResync()) {
                    const auto dbName = db::catalog::roomName(room, roomDb);
                    std::string error;
                    if (!db::maintenance::tryDropNamedStore(storage(), txn, dbName, &error) &&
                        !error.empty())
                        cache::activeLoggers().db->warn("Failed to drop '{}': {}", dbName, error);
                }
            }

            // clear db, don't delete
            db->rooms.drop(txn, false);
            setNextBatchToken(txn, "");

            txn.commit();
        } catch (const db::Error &) {
            cache::activeLoggers().db->critical("Failed to clear cache!");
            return false;
        }

        cache::activeLoggers().db->info(
          "Successfully cleared the cache. Will do a clean sync after startup.");
        return true;
    });
    migrations.emplace_back("2021.08.31", [this]() {
        storeSecretInStore("pickle_secret", "secret");
        this->pickle_secret_ = "secret";
        return true;
    });
    migrations.emplace_back("2022.11.06", [this]() {
        databaseReady_ = false;
        loadSecretsFromStore(
          {
            {std::string(mtx::secret_storage::secrets::cross_signing_master), false},
            {std::string(mtx::secret_storage::secrets::cross_signing_self_signing), false},
            {std::string(mtx::secret_storage::secrets::cross_signing_user_signing), false},
            {std::string(mtx::secret_storage::secrets::megolm_backup_v1), false},
          },
          [this,
           count = 1](const std::string &name, bool internal, const std::string &value) mutable {
              cache::activeLoggers().db->critical("Loaded secret {}", name);
              this->storeSecret(name, value, internal);

              // HACK(Nico): delay deletion to not crash because of multiple
              // nested deletions.
              // Since this is just migration code, this should be fine.

              QTimer::singleShot(count * 2000, this, [this, name, internal] {
                  deleteSecretFromStore(name, internal);
              });
              count++;
          },
          false);

        while (!this->databaseReady_) {
            QCoreApplication::instance()->processEvents(QEventLoop::AllEvents, 100);
        }

        return true;
    });
    auto postMigrations = cache::detail::buildPostMigrations(this);
    migrations.insert(migrations.end(), postMigrations.begin(), postMigrations.end());

    cache::activeLoggers().db->info("Running migrations, this may take a while!");
    for (const auto &[target_version, migration] : migrations) {
        if (target_version > stored_version)
            if (!migration()) {
                cache::activeLoggers().db->critical("migration failure!");
                return false;
            }
    }
    cache::activeLoggers().db->info("Migrations finished.");

    setCurrentFormat();
    return true;
}

cache::CacheVersion
Cache::formatVersion()
{
    auto txn = ro_txn(storage());
    auto currentVersion =
      db::getSyncStateValue(txn, db->syncState, db::catalog::SyncStateKey::CacheFormatVersion);
    if (!currentVersion.has_value())
        return cache::CacheVersion::Older;

    std::string stored_version = *currentVersion;

    if (stored_version < CURRENT_CACHE_FORMAT_VERSION)
        return cache::CacheVersion::Older;
    else if (stored_version > CURRENT_CACHE_FORMAT_VERSION)
        return cache::CacheVersion::Older;
    else
        return cache::CacheVersion::Current;
}

void
Cache::setCurrentFormat()
{
    auto txn = beginTxn();
    db::putSyncStateValue(txn,
                          db->syncState,
                          db::catalog::SyncStateKey::CacheFormatVersion,
                          CURRENT_CACHE_FORMAT_VERSION);

    txn.commit();
}
