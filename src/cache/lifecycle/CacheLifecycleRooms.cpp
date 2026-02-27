// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <map>
#include <set>
#include <string_view>

#include <nlohmann/json.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QTimer>

#include <spdlog/logger.h>

#include "Utils.h"
#include "cache/api/CacheApiContext.h"
#include "db/Json.h"
#include "db/StorageApi.h"
#include "db/SyncState.h"

void
Cache::removeLeftRooms(db::Transaction &txn,
                       const std::map<std::string, mtx::responses::LeftRoom> &rooms)
{
    for (const auto &room : rooms) {
        removeRoom(txn, room.first);

        // Clean up leftover invites.
        removeInvite(txn, room.first);
    }
}

void
Cache::removeInvite(db::Transaction &txn, const std::string &room_id)
{
    db->invites.del(txn, room_id);
    getInviteStatesDb(txn, room_id).drop(txn, true);
    getInviteMembersDb(txn, room_id).drop(txn, true);
}

void
Cache::removeInvite(const std::string &room_id)
{
    auto txn = beginTxn();
    removeInvite(txn, room_id);
    txn.commit();
}

void
Cache::removeRoom(db::Transaction &txn, const std::string &roomid)
{
    db->rooms.del(txn, roomid);
    getStatesDb(txn, roomid).drop(txn, true);
    getAccountDataDb(txn, roomid).drop(txn, true);
    getMembersDb(txn, roomid).drop(txn, true);
}

void
Cache::removeRoom(const std::string &roomid)
{
    auto txn = beginTxn();
    db->rooms.del(txn, roomid);
    txn.commit();
}

void
Cache::setNextBatchToken(db::Transaction &txn, const std::string &token)
{
    db::putSyncStateValue(txn, db->syncState, db::catalog::SyncStateKey::NextBatch, token);
}

bool
Cache::isInitialized()
{
    if (!db::isOpen(storage()))
        return false;

    auto txn = ro_txn(storage());
    return db::getSyncStateValue(txn, db->syncState, db::catalog::SyncStateKey::NextBatch)
      .has_value();
}

std::string
Cache::nextBatchToken()
{
    if (!db::isOpen(storage()))
        throw std::runtime_error("Storage backend is closed");

    auto txn = ro_txn(storage());
    return db::getSyncStateValue(txn, db->syncState, db::catalog::SyncStateKey::NextBatch)
      .value_or("");
}

void
Cache::deleteData()
{
    if (this->databaseReady_) {
        this->databaseReady_ = false;
        db::close(storage());
        verification_storage.status.clear();

        if (!cacheDirectory_.isEmpty()) {
            QDir(cacheDirectory_).removeRecursively();
            cache::activeLoggers().db->info("deleted cache files from disk");
        }
    } else {
        this->databaseReady_ = false;
    }

    emit secretChanged("pickle_secret");
}

cache::UserReceipts
Cache::readReceipts(const QString &event_id, const QString &room_id)
{
    cache::UserReceipts receipts;

    try {
        auto txn = ro_txn(storage());

        std::string_view value;

        bool res = db::getReadReceiptValue(
          txn, db->readReceipts, event_id.toStdString(), room_id.toStdString(), value);

        if (res) {
            auto json_response =
              nlohmann::json::parse(std::string_view(value.data(), value.size()));
            auto values = json_response.get<std::map<std::string, uint64_t>>();

            for (const auto &v : values)
                // timestamp, user_id
                receipts.emplace(v.second, v.first);
        }

    } catch (const db::Error &e) {
        cache::activeLoggers().db->critical("readReceipts: {}", e.what());
    }

    return receipts;
}

void
Cache::updateReadReceipt(db::Transaction &txn, const std::string &room_id, const Receipts &receipts)
{
    auto user_id = this->localUserId_.toStdString();
    for (const auto &receipt : receipts) {
        const auto event_id = receipt.first;
        auto event_receipts = receipt.second;

        try {
            std::string_view prev_value;

            bool exists =
              db::getReadReceiptValue(txn, db->readReceipts, event_id, room_id, prev_value);

            std::map<std::string, uint64_t> saved_receipts;

            // If an entry for the event id already exists, we would
            // merge the existing receipts with the new ones.
            if (exists) {
                auto json_value =
                  nlohmann::json::parse(std::string_view(prev_value.data(), prev_value.size()));

                // Retrieve the saved receipts.
                saved_receipts = json_value.get<std::map<std::string, uint64_t>>();
            }

            // Append the new ones.
            for (const auto &[read_by, timestamp] : event_receipts) {
                saved_receipts.emplace(read_by, timestamp);
            }

            // Save back the merged (or only the new) receipts.
            nlohmann::json json_updated_value = saved_receipts;
            std::string merged_receipts       = json_updated_value.dump();

            db::putReadReceiptValue(txn, db->readReceipts, event_id, room_id, merged_receipts);

        } catch (const db::Error &e) {
            cache::activeLoggers().db->critical("updateReadReceipts: {}", e.what());
        }
    }
}

std::string
Cache::getFullyReadEventId(const std::string &room_id)
{
    auto txn = ro_txn(storage());

    if (auto ev = getAccountData(txn, mtx::events::EventType::FullyRead, room_id)) {
        if (auto fr =
              std::get_if<mtx::events::AccountDataEvent<mtx::events::account_data::FullyRead>>(
                &ev.value())) {
            return fr->content.event_id;
        }
    }
    return std::string();
}

void
Cache::calculateRoomReadStatus()
{
    const auto joined_rooms = joinedRooms();

    std::map<QString, bool> readStatus;

    for (const auto &room : joined_rooms)
        readStatus.emplace(QString::fromStdString(room), calculateRoomReadStatus(room));

    emit roomReadStatus(readStatus);
}

bool
Cache::calculateRoomReadStatus(const std::string &room_id)
{
    std::string last_event_id_, fullyReadEventId_;
    {
        auto txn = ro_txn(storage());

        // Get last event id on the room.
        const auto last_event_id = getLastEventId(txn, room_id);
        const auto localUser     = utils::localUser().toStdString();

        std::string fullyReadEventId = getFullyReadEventId(room_id);

        if (last_event_id.empty() || fullyReadEventId.empty())
            return true;

        if (last_event_id == fullyReadEventId)
            return false;

        last_event_id_    = std::string(last_event_id);
        fullyReadEventId_ = std::string(fullyReadEventId);
    }

    // Retrieve all read receipts for that event.
    return getEventIndex(room_id, last_event_id_) > getEventIndex(room_id, fullyReadEventId_);
}
