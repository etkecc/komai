// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/core/Cache.h"
#include "cache/core/Cache_p.h"

#include <functional>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_set>
#include <variant>

#include <QMap>
#include <spdlog/logger.h>

#include <nlohmann/json.hpp>

#include <mtx/responses/common.hpp>

#include "cache/api/CacheApiWrappers.h"
#include "db/RoomInfo.h"
#include "db/SyncState.h"

std::vector<std::string>
Cache::getRoomIds(db::Transaction &txn)
{
    return db::listUniqueKeys(txn, db->rooms);
}

void
Cache::updateSpaces(db::Transaction &txn,
                    const std::set<std::string> &spaces_with_updates,
                    std::set<std::string> rooms_with_updates)
{
    if (spaces_with_updates.empty() && rooms_with_updates.empty())
        return;

    for (const auto &space : spaces_with_updates) {
        // delete old entries
        {
            db::forEachDupValue(
              txn, db->spacesChildren, space, [this, &txn, &space](std::string_view space_child) {
                  db->spacesParents.del(txn, space_child, space);
                  return true;
              });
            db->spacesChildren.del(txn, space);
        }

        for (const auto &event :
             getStateEventsWithType<mtx::events::state::space::Child>(txn, space)) {
            if (event.content.via.has_value() && event.state_key.size() > 3 &&
                event.state_key.at(0) == '!') {
                db->spacesChildren.put(txn, space, event.state_key);
                db->spacesParents.put(txn, event.state_key, space);
            }
        }

        for (const auto &r : getRoomIds(txn)) {
            if (auto parent = getStateEvent<mtx::events::state::space::Parent>(txn, r, space)) {
                rooms_with_updates.insert(r);
            }
        }
    }

    const auto space_event_type = to_string(mtx::events::EventType::SpaceChild);

    for (const auto &room : rooms_with_updates) {
        for (const auto &event :
             getStateEventsWithType<mtx::events::state::space::Parent>(txn, room)) {
            if (event.content.via.has_value() && event.state_key.size() > 3 &&
                event.state_key.at(0) == '!') {
                const std::string &space = event.state_key;

                auto pls = getStateEvent<mtx::events::state::PowerLevels>(txn, space);

                if (!pls)
                    continue;

                if (pls->content.user_level(event.sender) >=
                    pls->content.state_level(space_event_type)) {
                    db->spacesChildren.put(txn, space, room);
                    db->spacesParents.put(txn, room, space);
                } else {
                    cache::activeLoggers().db->debug(
                      "Skipping {} in {} because of missing PL. {}: {} < {}",
                      room,
                      space,
                      event.sender,
                      pls->content.user_level(event.sender),
                      pls->content.state_level(space_event_type));
                }
            }
        }
    }
}

QMap<QString, std::optional<RoomInfo>>
Cache::spaces()
{
    auto txn = ro_txn(storage());

    QMap<QString, std::optional<RoomInfo>> ret;
    db::forEachUniqueKey(txn, db->spacesChildren, [this, &txn, &ret](std::string_view space_id) {
        bool hasNonEmptyChild = false;
        db::forEachDupValue(
          txn, db->spacesChildren, space_id, [&hasNonEmptyChild](std::string_view space_child) {
              if (space_child.empty())
                  return true;
              hasNonEmptyChild = true;
              return false;
          });
        if (!hasNonEmptyChild)
            return true;

        const auto spaceId = std::string(space_id);
        std::string_view room_data;
        if (db->rooms.get(txn, spaceId, room_data)) {
            try {
                RoomInfo tmp = db::parseRoomInfo(room_data);
                ret.insert(QString::fromStdString(spaceId), tmp);
            } catch (const std::exception &e) {
                cache::activeLoggers().db->warn(
                  "failed to parse room info for space {}: {}", spaceId, e.what());
            }
        } else {
            ret.insert(QString::fromStdString(spaceId), std::nullopt);
        }

        return true;
    });

    return ret;
}

std::vector<std::string>
Cache::getParentRoomIds(const std::string &room_id)
{
    auto txn = ro_txn(storage());

    std::vector<std::string> roomids;
    db::forEachDupValue(txn, db->spacesParents, room_id, [&roomids](std::string_view parentRoomId) {
        if (!parentRoomId.empty())
            roomids.emplace_back(parentRoomId);
        return true;
    });

    return roomids;
}

std::vector<std::string>
Cache::getChildRoomIds(const std::string &room_id)
{
    auto txn = ro_txn(storage());

    std::vector<std::string> roomids;
    db::forEachDupValue(txn, db->spacesChildren, room_id, [&roomids](std::string_view childRoomId) {
        if (!childRoomId.empty())
            roomids.emplace_back(childRoomId);
        return true;
    });

    return roomids;
}
