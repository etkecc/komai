// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <map>
#include <set>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QTimer>

#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"
#include "cache/schema/CacheSchema.h"
#include "db/Json.h"
#include "db/storage/Core.h"
#include "db/storage/Crypto.h"
#include "db/storage/Scan.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "utils/Utils.h"

namespace {

bool
jsonKeyMatchesRoom(std::string_view key, std::string_view roomId)
{
    nlohmann::json parsed;
    if (!db::parseJsonValue(key, parsed))
        return false;

    const auto roomIt = parsed.find("room_id");
    return roomIt != parsed.end() && roomIt->is_string() &&
           roomIt->get_ref<const std::string &>() == roomId;
}

} // namespace

void
MatrixStore::removeLeftRooms(db::Transaction &txn,
                             const std::map<std::string, mtx::responses::LeftRoom> &rooms)
{
    for (const auto &room : rooms)
        removeRoom(txn, room.first);
}

void
MatrixStore::removeInvite(db::Transaction &txn, const std::string &room_id)
{
    db->invites.del(txn, room_id);
    getInviteStatesDb(txn, room_id).drop(txn, true);
    getInviteMembersDb(txn, room_id).drop(txn, true);
}

void
MatrixStore::removeInvite(const std::string &room_id)
{
    auto txn = beginTxn();
    removeInvite(txn, room_id);
    txn.commit();
}

void
MatrixStore::removeRoom(db::Transaction &txn, const std::string &roomid)
{
    try {
        auto eventsDb = cache::schema::openRoomStore(
          storage(), txn, roomid, cache::schema::RoomDb::Events, false);
        for (const auto &eventId : db::listKeys(txn, eventsDb))
            db->notifications.del(txn, eventId);
    } catch (const std::exception &) {
    }

    const auto parentSpaces = db::listDupValues(txn, db->spacesParents, roomid);
    for (const auto &parentSpace : parentSpaces)
        db->spacesChildren.del(txn, parentSpace, roomid);
    db->spacesParents.del(txn, roomid);

    const auto childRooms = db::listDupValues(txn, db->spacesChildren, roomid);
    for (const auto &childRoom : childRooms)
        db->spacesParents.del(txn, childRoom, roomid);
    db->spacesChildren.del(txn, roomid);

    db::eraseEntriesIf(txn, db->readReceipts, [&roomid](std::string_view key, std::string_view) {
        return jsonKeyMatchesRoom(key, roomid);
    });
    db::eraseEntriesIf(
      txn, db->inboundMegolmSessions, [&roomid](std::string_view key, std::string_view) {
          return jsonKeyMatchesRoom(key, roomid);
      });
    db::eraseEntriesIf(
      txn, db->megolmSessionsData, [&roomid](std::string_view key, std::string_view) {
          return jsonKeyMatchesRoom(key, roomid);
      });

    db->rooms.del(txn, roomid);
    db->invites.del(txn, roomid);
    db->encryptedRooms_.del(txn, roomid);
    db->eventExpiryBgJob_.del(txn, roomid);
    db->outboundMegolmSessions.del(txn, roomid);

    const auto roomPrefix = roomid + "/";
    for (const auto &dbName : storage().listStoreNames(txn)) {
        if (!std::string_view(dbName).starts_with(roomPrefix))
            continue;

        db::openNamedStore(storage(), txn, dbName, false).drop(txn, true);
    }
}

void
MatrixStore::removeRoom(const std::string &roomid)
{
    auto txn = beginTxn();
    removeRoom(txn, roomid);
    txn.commit();
}

void
MatrixStore::setNextBatchToken(db::Transaction &txn, const std::string &token)
{
    cache::sync_state::putNextBatchToken(txn, db->syncState, token);
}

bool
MatrixStore::isInitialized()
{
    if (!db::isOpen(storage()))
        return false;

    auto txn = ro_txn(storage());
    return cache::sync_state::getNextBatchToken(txn, db->syncState).has_value();
}

std::string
MatrixStore::nextBatchToken()
{
    if (!db::isOpen(storage()))
        throw std::runtime_error("Storage backend is closed");

    auto txn = ro_txn(storage());
    return cache::sync_state::getNextBatchToken(txn, db->syncState).value_or("");
}

void
MatrixStore::deleteData()
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
MatrixStore::readReceipts(const QString &event_id, const QString &room_id)
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
MatrixStore::updateReadReceipt(db::Transaction &txn,
                               const std::string &room_id,
                               const Receipts &receipts)
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
MatrixStore::getFullyReadEventId(const std::string &room_id)
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
MatrixStore::calculateRoomReadStatus()
{
    const auto joined_rooms = joinedRooms();
    const int policy =
      static_cast<int>(UserSettings::instance()->sidebarsRoomListUnreadDetectionPolicy());

    std::map<QString, bool> readStatus;

    for (const auto &room : joined_rooms)
        readStatus.emplace(QString::fromStdString(room), calculateRoomReadStatus(room, policy));

    emit roomReadStatus(readStatus);
}

bool
MatrixStore::calculateRoomReadStatus(const std::string &room_id)
{
    const int policy =
      static_cast<int>(UserSettings::instance()->sidebarsRoomListUnreadDetectionPolicy());
    return calculateRoomReadStatus(room_id, policy);
}

bool
MatrixStore::calculateRoomReadStatus(const std::string &room_id, int policy)
{
    std::string last_event_id_, fullyReadEventId_;
    {
        auto txn = ro_txn(storage());

        // Get last event id on the room, respecting the unread detection policy.
        const auto last_event_id =
          (policy == static_cast<int>(UserSettings::UnreadDetectionPolicy::MessagesOnly))
            ? getLastContentEventId(txn, room_id)
            : getLastEventId(txn, room_id);

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
