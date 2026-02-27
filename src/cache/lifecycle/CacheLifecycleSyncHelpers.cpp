// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <map>
#include <string_view>
#include <vector>

#include <spdlog/logger.h>

#include <nlohmann/json.hpp>

#include <mtx/responses/common.hpp>

#include "cache/api/CacheApiWrappers.h"

void
Cache::saveInvites(db::Transaction &txn,
                   const std::map<std::string, mtx::responses::InvitedRoom> &rooms)
{
    for (const auto &room : rooms) {
        auto statesdb  = getInviteStatesDb(txn, room.first);
        auto membersdb = getInviteMembersDb(txn, room.first);

        saveInvite(txn, statesdb, membersdb, room.second);

        RoomInfo updatedInfo;
        updatedInfo.name       = getInviteRoomName(txn, statesdb, membersdb).toStdString();
        updatedInfo.topic      = getInviteRoomTopic(txn, statesdb).toStdString();
        updatedInfo.avatar_url = getInviteRoomAvatarUrl(txn, statesdb, membersdb).toStdString();
        updatedInfo.is_space   = getInviteRoomIsSpace(txn, statesdb);
        updatedInfo.is_invite  = true;

        db::putRoomInfo(txn, db->invites, room.first, updatedInfo);
    }
}

void
Cache::saveInvite(db::Transaction &txn,
                  db::Store &statesdb,
                  db::Store &membersdb,
                  const mtx::responses::InvitedRoom &room)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    for (const auto &e : room.invite_state) {
        if (auto msg = std::get_if<StrippedEvent<Member>>(&e)) {
            auto display_name =
              msg->content.display_name.empty() ? msg->state_key : msg->content.display_name;

            std::string inviter = "";
            if (msg->content.membership == mtx::events::state::Membership::Invite) {
                inviter = msg->sender;
            }

            MemberInfo tmp{display_name,
                           msg->content.avatar_url,
                           inviter,
                           msg->content.reason,
                           msg->content.is_direct};

            db::putMemberInfo(txn, membersdb, msg->state_key, tmp);
        } else {
            std::visit(
              [&txn, &statesdb](auto msg) {
                  auto j   = nlohmann::json(msg);
                  bool res = statesdb.put(txn, j["type"].get<std::string>(), j.dump());

                  if (!res) {
                      cache::activeLoggers().db->warn("couldn't save data: {}",
                                                      nlohmann::json(msg).dump());
                  }
              },
              e);
        }
    }
}

void
Cache::savePresence(
  db::Transaction &txn,
  const std::vector<mtx::events::Event<mtx::events::presence::Presence>> &presenceUpdates)
{
    for (const auto &update : presenceUpdates) {
        auto toWrite = nlohmann::json(update.content);
        // Nheko currently doesn't use those and it causes lots of db writes :)
        toWrite.erase("currently_active");
        toWrite.erase("last_active_ago");
        auto toWriteStr = toWrite.dump();

        std::string_view oldPresenceVal;

        db->presence.get(txn, update.sender, oldPresenceVal);
        if (oldPresenceVal != toWriteStr) {
            db->presence.put(txn, update.sender, toWriteStr);
        }
    }
}
