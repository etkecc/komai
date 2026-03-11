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
#include "db/storage/Serde.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "utils/Utils.h"

namespace {

struct StoredReadReceipt
{
    std::string eventId;
    uint64_t timestamp  = 0;
    uint64_t eventIndex = 0;
};

bool
jsonCompositeKeyMatchesRoom(std::string_view key, std::string_view roomId)
{
    nlohmann::json parsed;
    if (!db::parseJsonValue(key, parsed))
        return false;

    const auto roomIt = parsed.find("room_id");
    return roomIt != parsed.end() && roomIt->is_string() &&
           roomIt->get_ref<const std::string &>() == roomId;
}

std::optional<StoredReadReceipt>
parseStoredReadReceipt(std::string_view value)
{
    nlohmann::json parsed;
    if (!db::parseJsonValue(value, parsed))
        return std::nullopt;

    const auto eventIdIt    = parsed.find("event_id");
    const auto timestampIt  = parsed.find("timestamp");
    const auto eventIndexIt = parsed.find("event_index");
    if (eventIdIt == parsed.end() || timestampIt == parsed.end() || eventIndexIt == parsed.end() ||
        !eventIdIt->is_string() || !timestampIt->is_number_unsigned() ||
        !eventIndexIt->is_number_unsigned()) {
        return std::nullopt;
    }

    return StoredReadReceipt{
      .eventId    = eventIdIt->get<std::string>(),
      .timestamp  = timestampIt->get<uint64_t>(),
      .eventIndex = eventIndexIt->get<uint64_t>(),
    };
}

std::string
serializeStoredReadReceipt(const StoredReadReceipt &receipt)
{
    nlohmann::json serialized;
    serialized["event_id"]    = receipt.eventId;
    serialized["timestamp"]   = receipt.timestamp;
    serialized["event_index"] = receipt.eventIndex;
    return serialized.dump();
}

std::optional<uint64_t>
eventIndexForReceipt(db::Transaction &txn, db::Store &eventToOrderDb, std::string_view eventId)
{
    std::string_view rawEventIndex;
    if (!eventToOrderDb.get(txn, eventId, rawEventIndex))
        return std::nullopt;

    return db::fromSv<uint64_t>(rawEventIndex);
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

    std::vector<std::string> receiptUsers;
    db::forEachReadReceiptInRoom(
      txn, db->readReceipts, roomid, [&receiptUsers](std::string_view userId, std::string_view) {
          receiptUsers.emplace_back(userId);
          return true;
      });
    for (const auto &userId : receiptUsers)
        db->readReceipts.del(txn, db::readReceiptKey(roomid, userId));
    db::eraseEntriesIf(
      txn, db->inboundMegolmSessions, [&roomid](std::string_view key, std::string_view) {
          return jsonCompositeKeyMatchesRoom(key, roomid);
      });
    db::eraseEntriesIf(
      txn, db->megolmSessionsData, [&roomid](std::string_view key, std::string_view) {
          return jsonCompositeKeyMatchesRoom(key, roomid);
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
        auto txn          = ro_txn(storage());
        const auto roomId = room_id.toStdString();

        auto eventToOrderDb = getEventToOrderDb(txn, roomId);
        const auto targetEventIndex =
          eventIndexForReceipt(txn, eventToOrderDb, event_id.toStdString());
        if (!targetEventIndex)
            return receipts;

        db::forEachReadReceiptInRoom(
          txn,
          db->readReceipts,
          roomId,
          [&receipts, targetEventIndex](std::string_view userId, std::string_view value) {
              const auto receipt = parseStoredReadReceipt(value);
              if (!receipt || receipt->eventIndex < *targetEventIndex)
                  return true;

              receipts.emplace(receipt->timestamp, std::string(userId));
              return true;
          });
    } catch (const std::exception &e) {
        cache::activeLoggers().db->critical("readReceipts: {}", e.what());
    }

    return receipts;
}

void
MatrixStore::updateReadReceipt(db::Transaction &txn,
                               const std::string &room_id,
                               const Receipts &receipts)
{
    auto eventToOrderDb = getEventToOrderDb(txn, room_id);
    std::map<std::string, StoredReadReceipt> latestReceiptsByUser;

    for (const auto &[eventId, eventReceipts] : receipts) {
        const auto eventIndex = eventIndexForReceipt(txn, eventToOrderDb, eventId);
        if (!eventIndex) {
            cache::activeLoggers().db->warn(
              "Skipping read receipt for uncached event '{}' in room '{}'.", eventId, room_id);
            continue;
        }

        for (const auto &[readBy, timestamp] : eventReceipts) {
            StoredReadReceipt candidate{
              .eventId    = eventId,
              .timestamp  = timestamp,
              .eventIndex = *eventIndex,
            };

            auto existing = latestReceiptsByUser.find(readBy);
            if (existing == latestReceiptsByUser.end() ||
                candidate.eventIndex > existing->second.eventIndex ||
                (candidate.eventIndex == existing->second.eventIndex &&
                 candidate.timestamp > existing->second.timestamp)) {
                latestReceiptsByUser[readBy] = std::move(candidate);
            }
        }
    }

    for (const auto &[readBy, receipt] : latestReceiptsByUser)
        db::putReadReceiptValue(
          txn, db->readReceipts, room_id, readBy, serializeStoredReadReceipt(receipt));
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
