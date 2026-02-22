// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

#include <stdexcept>

#include <spdlog/logger.h>

#include <QDir>
#include <QFile>

#if __has_include(<keychain.h>)
#include <keychain.h>
#else
#include <qt6keychain/keychain.h>
#endif

#include "ChatPage.h"
#include "CacheApiWrappers.h"
#include "MatrixClient.h"
#include "Paths.h"
#include "ProfileSecrets.h"
#include "UserSettingsPage.h"
#include "Utils.h"
#include "db/Maintenance.h"
#include "db/StorageApi.h"

extern bool needsCompact;

static constexpr auto MAX_DBS_DEFAULT = 32384U;

#if Q_PROCESSOR_WORDSIZE >= 5 // 40-bit or more, up to 2^(8*WORDSIZE) words addressable.
static constexpr auto DB_SIZE_DEFAULT = 32ULL * 1024ULL * 1024ULL * 1024ULL; // 32 GB
#elif Q_PROCESSOR_WORDSIZE == 4 // 32-bit address space limits mmaps
static constexpr auto DB_SIZE_DEFAULT = 1ULL * 1024ULL * 1024ULL * 1024ULL; // 1 GB
#else
#error Not enough virtual address space for the database on target CPU
#endif

namespace {

std::shared_ptr<spdlog::logger>
cacheDbLogger()
{
    return cache::activeLoggers().db;
}

} // namespace

static QString
cacheDirectoryName(const QString &userid, const QString &profile)
{
    return app_paths::data::databaseDirectory(userid, profile);
}

void
Cache::setup()
{
    auto settings = UserSettings::instance();
    const auto logger = cacheDbLogger();

    if (logger)
        logger->debug("setting up cache");

    cacheDirectory_                      = cacheDirectoryName(localUserId_, settings->profile());
    const std::string cacheDirectoryPath = cacheDirectory_.toStdString();

    if (logger)
        logger->debug("Database at: {}", cacheDirectory_.toStdString());

    const bool isPersistentBackend =
      db::storageCategory(storage()) == db::StorageCategory::Persistent;
    const bool isInitial = isPersistentBackend && !QFile::exists(cacheDirectory_);

    auto storageOptions = [] {
        auto settings      = UserSettings::instance();
        std::size_t dbSize = settings->maxDbSize();
        if (dbSize == 0 || dbSize < DB_SIZE_DEFAULT)
            dbSize = DB_SIZE_DEFAULT;
        unsigned dbCount = settings->maxDbs();
        if (dbCount == 0 || dbCount < MAX_DBS_DEFAULT)
            dbCount = MAX_DBS_DEFAULT;

        // ignore unreasonably high values of more than a quarter of the addressable memory
        if (dbSize > (1ull << (Q_PROCESSOR_WORDSIZE * 8 - 2))) {
            dbSize = DB_SIZE_DEFAULT;
        }
        // Limit databases to about a million. This would cause more than 7-120MB to get written on
        // every commit, which I doubt would work well. File an issue, if you tested this and it
        // works fine.
        if (dbCount > (1u << 20)) {
            dbCount = 1u << 20;
        }

        return db::DatabaseOptions{
          .mapSizeBytes = dbSize,
          .maxDbs       = dbCount,
          .durability   = db::Durability::Relaxed,
        };
    }();

    if (logger)
        logger->info("Using storage backend: {}", db::id(storage()));
    if (!isPersistentBackend)
        if (logger)
            logger->warn("Using ephemeral storage backend; cache contents will be lost on restart");

    if (isInitial) {
        if (logger)
            logger->info("initializing {} backend", db::id(storage()));

        if (!QDir().mkpath(cacheDirectory_)) {
            throw std::runtime_error(
              ("Unable to create state directory:" + cacheDirectory_).toStdString().c_str());
        }
    }

    try {
        // NOTE(Nico): We may want a more aggressive mmap write strategy in the future, but
        // it can really mess up our database, so we shouldn't. For now, hopefully
        // the current relaxed sync mode is fast enough.
        //
        // 2022-10-28: Disable the nosync flags again in the hope to crack down on some database
        // corruption.
        // 2023-02-23: Reenable the nosync flags. There was no measureable benefit to resiliency,
        // but sync causes frequent lag sometimes even for the whole system. Possibly the data
        // corruption is a database-backend or filesystem bug. See
        // https://github.com/Nheko-Reborn/nheko/issues/1355
        // https://github.com/Nheko-Reborn/nheko/issues/1303
        db::open(storage(),
                 isPersistentBackend ? std::string_view(cacheDirectoryPath) : std::string_view{},
                 storageOptions);

        if (needsCompact) {
            if (!db::maintenance::supportsCompaction(storage())) {
                if (logger)
                    logger->warn("Storage backend '{}' does not support compaction, skipping.",
                                 db::id(storage()));
            } else {
                auto compactDir  = cacheDirectory_ + "-compacting";
                auto toDeleteDir = cacheDirectory_ + "-olddb";
                if (QFile::exists(cacheDirectory_))
                    QDir(compactDir).removeRecursively();
                if (QFile::exists(toDeleteDir))
                    QDir(toDeleteDir).removeRecursively();
                if (!QDir().mkpath(compactDir)) {
                    if (logger)
                        logger->warn("Failed to create directory '{}' for database compaction, "
                                     "skipping compaction!",
                                     compactDir.toStdString());
                } else {
                    // Create a temporary backend matching the current storage backend.
                    auto temp                        = db::createDatabase(db::id(storage()));
                    const std::string compactDirPath = compactDir.toStdString();
                    db::open(temp, compactDirPath, storageOptions);

                    // copy data
                    db::maintenance::compact(storage(), *temp);

                    // close envs
                    db::close(temp);
                    db::close(storage());

                    // swap the databases and delete old one
                    QDir().rename(cacheDirectory_, toDeleteDir);
                    QDir().rename(compactDir, cacheDirectory_);
                    QDir(toDeleteDir).removeRecursively();

                    // reopen env
                    db::open(storage(), cacheDirectoryPath, storageOptions);
                }
            }
        }
    } catch (const db::Error &e) {
        const auto errorKind = e.kind();
        if (errorKind != db::ErrorKind::VersionMismatch && errorKind != db::ErrorKind::Invalid) {
            throw std::runtime_error("Storage initialization failed: " + std::string(e.what()));
        }

        if (!isPersistentBackend) {
            throw;
        }

        if (logger)
            logger->warn("resetting cache due to incompatible storage format: {}", e.what());

        QDir stateDir(cacheDirectory_);

        auto eList = stateDir.entryList(QDir::NoDotAndDotDot);
        for (const auto &file : std::as_const(eList)) {
            if (!stateDir.remove(file))
                throw std::runtime_error(("Unable to delete file " + file).toStdString().c_str());
        }
        db::open(storage(), cacheDirectoryPath, storageOptions);
    }

    auto txn           = beginTxn();
    db->syncState      = db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::SyncState);
    db->rooms          = db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::Rooms);
    db->spacesChildren = db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::SpacesChildren);
    db->spacesParents  = db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::SpacesParents);
    db->invites        = db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::Invites);
    db->readReceipts   = db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::ReadReceipts);
    db->notifications  = db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::Notifications);
    db->presence       = db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::Presence);

    // Session management
    db->inboundMegolmSessions =
      db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::InboundMegolmSessions);
    db->outboundMegolmSessions =
      db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::OutboundMegolmSessions);
    db->megolmSessionsData =
      db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::MegolmSessionsData);

    db->olmSessions = db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::OlmSessions);

    // What rooms are encrypted
    db->encryptedRooms_ =
      db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::EncryptedRooms);
    db->eventExpiryBgJob_ =
      db::openGlobalStore(storage(), txn, db::catalog::GlobalDb::EventExpirationBgJob);

    [[maybe_unused]] auto verificationDb = getVerificationDb(txn);
    [[maybe_unused]] auto userKeysDb     = getUserKeysDb(txn);

    txn.commit();

    loadSecretsFromStore(
      {
        {"pickle_secret", true},
      },
      [this](const std::string &, bool, const std::string &value) { this->pickle_secret_ = value; },
      true);
}
