// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/core/Cache.h"
#include "cache/core/Cache_p.h"

#include <map>
#include <set>
#include <string_view>
#include <vector>

#include <mtx/responses/common.hpp>
#include <mtx/responses/messages.hpp>
#include <nlohmann/json.hpp>

#include <QCoreApplication>
#include <QEventLoop>
#include <QMessageBox>
#include <spdlog/logger.h>

#include "EventAccessors.h"
#include "Utils.h"
#include "cache/api/CacheApiWrappers.h"
#include "settings/ui/facade/UserSettingsPage.h"

void
Cache::saveState(const mtx::responses::Sync &res)
try {
    using namespace mtx::events;
    auto local_user_id = this->localUserId_.toStdString();

    auto currentBatchToken = res.next_batch;

    auto txn = beginTxn();

    setNextBatchToken(txn, res.next_batch);

    if (!res.account_data.events.empty()) {
        auto accountDataDb = getAccountDataDb(txn, "");
        for (const auto &ev : res.account_data.events)
            std::visit(
              [&txn, &accountDataDb](const auto &event) {
                  if constexpr (std::is_same_v<
                                  std::remove_cv_t<std::remove_reference_t<decltype(event)>>,
                                  AccountDataEvent<
                                    mtx::events::account_data::nheko_extensions::HiddenEvents>>) {
                      if (!event.content.hidden_event_types) {
                          accountDataDb.del(txn, "im.nheko.hidden_events");
                          return;
                      }
                  }

                  auto j = nlohmann::json(event);
                  accountDataDb.put(txn, j["type"].get<std::string>(), j.dump());
              },
              ev);
    }

    auto userKeyCacheDb = getUserKeysDb(txn);

    std::set<std::string> spaces_with_updates;
    std::set<std::string> rooms_with_space_updates;

    // Save joined rooms
    for (const auto &room : res.rooms.join) {
        auto statesdb    = getStatesDb(txn, room.first);
        auto stateskeydb = getStatesKeyDb(txn, room.first);
        auto membersdb   = getMembersDb(txn, room.first);
        auto eventsDb    = getEventsDb(txn, room.first);

        saveStateEvents(
          txn, statesdb, stateskeydb, membersdb, eventsDb, room.first, room.second.state.events);
        saveStateEvents(
          txn, statesdb, stateskeydb, membersdb, eventsDb, room.first, room.second.timeline.events);

        saveTimelineMessages(txn, eventsDb, room.first, room.second.timeline);

        RoomInfo updatedInfo;
        std::string originalRoomInfoDump;
        {
            // retrieve the old tags and modification ts
            std::string_view originalRoomInfoView;
            if (db->rooms.get(txn, room.first, originalRoomInfoView)) {
                originalRoomInfoDump = std::string(originalRoomInfoView);
                try {
                    RoomInfo tmp     = db::parseRoomInfo(originalRoomInfoDump);
                    updatedInfo.tags = std::move(tmp.tags);

                    updatedInfo.approximate_last_modification_ts =
                      tmp.approximate_last_modification_ts;
                } catch (const std::exception &e) {
                    cache::activeLoggers().db->warn(
                      "failed to parse room info: room_id ({}), {}: {}",
                      room.first,
                      originalRoomInfoDump,
                      e.what());
                }
            }
        }

        updatedInfo.name       = getRoomName(txn, statesdb, membersdb).toStdString();
        updatedInfo.topic      = getRoomTopic(txn, statesdb).toStdString();
        updatedInfo.avatar_url = getRoomAvatarUrl(txn, statesdb, membersdb).toStdString();
        updatedInfo.version    = getRoomVersion(txn, statesdb).toStdString();
        updatedInfo.is_space   = getRoomIsSpace(txn, statesdb);

        updatedInfo.notification_count = room.second.unread_notifications.notification_count;
        updatedInfo.highlight_count    = room.second.unread_notifications.highlight_count;

        if (updatedInfo.is_space) {
            bool space_updates = false;
            for (const auto &e : room.second.state.events)
                if (std::holds_alternative<StateEvent<state::space::Child>>(e) ||
                    std::holds_alternative<StateEvent<state::PowerLevels>>(e))
                    space_updates = true;
            for (const auto &e : room.second.timeline.events)
                if (std::holds_alternative<StateEvent<state::space::Child>>(e) ||
                    std::holds_alternative<StateEvent<state::PowerLevels>>(e))
                    space_updates = true;

            if (space_updates)
                spaces_with_updates.insert(room.first);
        }

        {
            bool room_has_space_update = false;
            for (const auto &e : room.second.state.events) {
                if (auto se = std::get_if<StateEvent<state::space::Parent>>(&e)) {
                    if (se->state_key.empty()) {
                        cache::activeLoggers().db->warn(
                          "Skipping space parent with empty state key in room {}", room.first);
                    } else {
                        spaces_with_updates.insert(se->state_key);
                        room_has_space_update = true;
                    }
                }
            }
            for (const auto &e : room.second.timeline.events) {
                if (auto se = std::get_if<StateEvent<state::space::Parent>>(&e)) {
                    if (se->state_key.empty()) {
                        cache::activeLoggers().db->warn(
                          "Skipping space child with empty state key in room {}", room.first);
                    } else {
                        spaces_with_updates.insert(se->state_key);
                        room_has_space_update = true;
                    }
                }
            }

            if (room_has_space_update)
                rooms_with_space_updates.insert(room.first);
        }

        // Process the account_data associated with this room
        if (!room.second.account_data.events.empty()) {
            auto accountDataDb = getAccountDataDb(txn, room.first);

            for (const auto &evt : room.second.account_data.events) {
                std::visit(
                  [&txn, &accountDataDb](const auto &event) {
                      if constexpr (std::is_same_v<
                                      std::remove_cv_t<std::remove_reference_t<decltype(event)>>,
                                      AccountDataEvent<mtx::events::account_data::nheko_extensions::
                                                         HiddenEvents>>) {
                          if (!event.content.hidden_event_types) {
                              accountDataDb.del(txn, "im.nheko.hidden_events");
                              return;
                          }
                      }
                      auto j = nlohmann::json(event);
                      accountDataDb.put(txn, j["type"].get<std::string>(), j.dump());
                  },
                  evt);

                // for tag events
                if (std::holds_alternative<AccountDataEvent<account_data::Tags>>(evt)) {
                    auto tags_evt = std::get<AccountDataEvent<account_data::Tags>>(evt);

                    updatedInfo.tags.clear();
                    for (const auto &tag : tags_evt.content.tags) {
                        updatedInfo.tags.push_back(tag.first);
                    }
                }
            }
        }

        for (const auto &e : room.second.timeline.events) {
            if (!mtx::accessors::is_message(e))
                continue;
            updatedInfo.approximate_last_modification_ts = mtx::accessors::origin_server_ts_ms(e);
        }

        if (auto newRoomInfoDump = db::serializeRoomInfo(updatedInfo);
            newRoomInfoDump != originalRoomInfoDump) {
            db::putRoomInfo(txn, db->rooms, room.first, updatedInfo);
        }

        for (const auto &e : room.second.ephemeral.events) {
            if (auto receiptsEv =
                  std::get_if<mtx::events::EphemeralEvent<mtx::events::ephemeral::Receipt>>(&e)) {
                Receipts receipts;

                for (const auto &[event_id, userReceipts] : receiptsEv->content.receipts) {
                    if (auto r = userReceipts.find(mtx::events::ephemeral::Receipt::Read);
                        r != userReceipts.end()) {
                        for (const auto &[user_id, receipt] : r->second.users) {
                            receipts[event_id][user_id] = receipt.ts;
                        }
                    }
                    if (userReceipts.count(mtx::events::ephemeral::Receipt::ReadPrivate)) {
                        const auto &users =
                          userReceipts.at(mtx::events::ephemeral::Receipt::ReadPrivate).users;
                        if (auto ts = users.find(local_user_id);
                            ts != users.end() && ts->second.ts != 0)
                            receipts[event_id][local_user_id] = ts->second.ts;
                    }
                }
                updateReadReceipt(txn, room.first, receipts);
            }
        }

        // Clean up non-valid invites.
        removeInvite(txn, room.first);
    }

    saveInvites(txn, res.rooms.invite);

    savePresence(txn, res.presence);

    markUserKeysOutOfDate(txn, userKeyCacheDb, res.device_lists.changed, currentBatchToken);

    removeLeftRooms(txn, res.rooms.leave);

    updateSpaces(txn, spaces_with_updates, std::move(rooms_with_space_updates));

    txn.commit();

    std::map<QString, bool> readStatus;

    for (const auto &room : res.rooms.join) {
        for (const auto &e : room.second.ephemeral.events) {
            if (auto receiptsEv =
                  std::get_if<mtx::events::EphemeralEvent<mtx::events::ephemeral::Receipt>>(&e)) {
                std::vector<QString> receipts;

                for (const auto &[event_id, userReceipts] : receiptsEv->content.receipts) {
                    if (auto r = userReceipts.find(mtx::events::ephemeral::Receipt::Read);
                        r != userReceipts.end()) {
                        for (const auto &[user_id, receipt] : r->second.users) {
                            (void)receipt;

                            if (user_id != local_user_id) {
                                receipts.push_back(QString::fromStdString(event_id));
                                break;
                            }
                        }
                    }
                }
                if (!receipts.empty())
                    emit newReadReceipts(QString::fromStdString(room.first), receipts);
            }
        }
        readStatus.emplace(QString::fromStdString(room.first), calculateRoomReadStatus(room.first));
    }

    emit roomReadStatus(readStatus);
} catch (const db::Error &storageException) {
    const auto errorKind = storageException.kind();
    if (errorKind == db::ErrorKind::DbsFull || errorKind == db::ErrorKind::MapFull) {
        if (errorKind == db::ErrorKind::DbsFull) {
            auto settings = UserSettings::instance();

            unsigned roomDbCount =
              static_cast<unsigned>((res.rooms.invite.size() + res.rooms.join.size() +
                                     res.rooms.knock.size() + res.rooms.leave.size()) *
                                    20);

            settings->setDbMaxStores(std::max(settings->dbMaxStores() * 2, roomDbCount));
        } else if (errorKind == db::ErrorKind::MapFull) {
            auto settings = UserSettings::instance();

            if (const auto mapSize = db::mapSizeBytes(storage()); mapSize.has_value()) {
                settings->setDbMaxSizeBytes(static_cast<qulonglong>(*mapSize * 2));
            }
        }

        QMessageBox::warning(
          nullptr,
          tr("Database limit reached"),
          tr("Your account is larger than our default database limit. We have "
             "increased the capacity automatically, however you will need to "
             "restart to apply this change. Komai will now close automatically."),
          QMessageBox::StandardButton::Close);
        QCoreApplication::exit(1);
        exit(1);
    }

    throw;
}
