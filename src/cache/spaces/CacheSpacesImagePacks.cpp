// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <exception>
#include <functional>
#include <optional>
#include <string_view>
#include <unordered_set>

#include <QDebug>

#include <nlohmann/json.hpp>

#include <mtx/requests.hpp>
#include <mtx/responses/common.hpp>

#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"
#include "cache/schema/RoomStore.h"

std::vector<ImagePackInfo>
MatrixStore::getImagePacks(const std::string &room_id, std::optional<bool> stickers)
{
    auto txn = ro_txn(storage());
    std::vector<ImagePackInfo> infos;

    auto addPack = [&infos, stickers](const mtx::events::msc2545::ImagePack &pack,
                                      const std::string &source_room,
                                      const std::string &state_key,
                                      bool from_space) {
        bool pack_is_sticker = pack.pack ? pack.pack->is_sticker() : true;
        bool pack_is_emoji   = pack.pack ? pack.pack->is_emoji() : true;
        bool pack_matches =
          !stickers.has_value() || (stickers.value() ? pack_is_sticker : pack_is_emoji);

        ImagePackInfo info;
        info.source_room = source_room;
        info.state_key   = state_key;
        info.pack.pack   = pack.pack;
        info.from_space  = from_space;

        for (const auto &img : pack.images) {
            if (stickers.has_value() &&
                (img.second.overrides_usage()
                   ? (stickers.value() ? !img.second.is_sticker() : !img.second.is_emoji())
                   : !pack_matches))
                continue;

            info.pack.images.insert(img);
        }

        if (!info.pack.images.empty())
            infos.push_back(std::move(info));
    };

    // packs from account data
    if (auto accountpack =
          getAccountData(txn, mtx::events::EventType::ImagePackInAccountData, "")) {
        auto tmp =
          std::get_if<mtx::events::EphemeralEvent<mtx::events::msc2545::ImagePack>>(&*accountpack);
        if (tmp)
            addPack(tmp->content, "", "", false);
    }

    // packs from rooms, that were enabled globally
    if (auto roomPacks = getAccountData(txn, mtx::events::EventType::ImagePackRooms, "")) {
        auto tmp = std::get_if<mtx::events::EphemeralEvent<mtx::events::msc2545::ImagePackRooms>>(
          &*roomPacks);
        if (tmp) {
            for (const auto &[room_id2, state_to_d] : tmp->content.rooms) {
                // don't add stickers from this room twice
                if (room_id2 == room_id)
                    continue;

                for (const auto &[state_id, d] : state_to_d) {
                    (void)d;
                    if (auto pack =
                          getStateEvent<mtx::events::msc2545::ImagePack>(txn, room_id2, state_id))
                        addPack(pack->content, room_id2, state_id, false);
                }
            }
        }
    }

    std::function<void(const std::string &room_id)> addRoomAndCanonicalParents;
    std::unordered_set<std::string> visitedRooms;
    addRoomAndCanonicalParents =
      [this, &addRoomAndCanonicalParents, &addPack, &visitedRooms, &txn, &room_id](
        const std::string &current_room) {
          if (visitedRooms.count(current_room))
              return;
          else
              visitedRooms.insert(current_room);

          if (auto pack = getStateEvent<mtx::events::msc2545::ImagePack>(txn, current_room)) {
              addPack(pack->content, current_room, "", current_room != room_id);
          }
          for (const auto &pack :
               getStateEventsWithType<mtx::events::msc2545::ImagePack>(txn, current_room)) {
              addPack(pack.content, current_room, pack.state_key, current_room != room_id);
          }

          for (const auto &parent :
               getStateEventsWithType<mtx::events::state::space::Parent>(txn, current_room)) {
              if (parent.content.canonical && parent.content.via && !parent.content.via->empty()) {
                  try {
                      addRoomAndCanonicalParents(parent.state_key);
                  } catch (const db::Error &) {
                      cache::activeLoggers().db->debug(
                        "Skipping events from parent community, because we are "
                        "not joined to it: {}",
                        parent.state_key);
                  }
              }
          }
      };

    // packs from current room and then iterate canonical space parents
    addRoomAndCanonicalParents(room_id);

    return infos;
}

std::optional<mtx::events::collections::RoomAccountDataEvents>
MatrixStore::getAccountData(mtx::events::EventType type, const std::string &room_id)
{
    auto txn = ro_txn(storage());
    return getAccountData(txn, type, room_id);
}

std::optional<std::string>
MatrixStore::getAccountDataByType(const std::string &type, const std::string &room_id)
{
    auto txn = ro_txn(storage());
    return getAccountDataByType(txn, type, room_id);
}

std::optional<mtx::events::collections::RoomAccountDataEvents>
MatrixStore::getAccountData(db::Transaction &txn,
                            mtx::events::EventType type,
                            const std::string &room_id)
{
    try {
        auto db_ = getAccountDataDb(txn, room_id);

        std::string_view data;
        if (room_store::get(
              txn, db_, cache::schema::RoomDb::AccountData, room_id, to_string(type), data)) {
            mtx::responses::utils::RoomAccountDataEvents events;
            nlohmann::json j = nlohmann::json::array({
              nlohmann::json::parse(data),
            });
            mtx::responses::utils::parse_room_account_data_events(j, events);
            if (events.size() == 1)
                return events.front();
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::optional<std::string>
MatrixStore::getAccountDataByType(db::Transaction &txn,
                                  const std::string &type,
                                  const std::string &room_id)
{
    try {
        auto db_ = getAccountDataDb(txn, room_id);

        std::string_view data;
        if (room_store::get(txn, db_, cache::schema::RoomDb::AccountData, room_id, type, data))
            return std::string(data);
    } catch (...) {
    }
    return std::nullopt;
}
