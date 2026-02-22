// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>
#include <exception>

#include <mtx/responses/common.hpp>
#include <mtx/responses/messages.hpp>
#include <mtxclient/utils.hpp>
#include <nlohmann/json.hpp>

#include "Logging.h"
#include "EventAccessors.h"
#include "UserSettingsPage.h"
#include "Utils.h"
#include "db/Maintenance.h"

void
Cache::updateState(const std::string &room, const mtx::responses::StateEvents &state, bool wipe)
{
    auto txn         = beginTxn();
    auto statesdb    = getStatesDb(txn, room);
    auto stateskeydb = getStatesKeyDb(txn, room);
    auto membersdb   = getMembersDb(txn, room);
    auto eventsDb    = getEventsDb(txn, room);

    if (wipe) {
        membersdb.drop(txn);
        statesdb.drop(txn);
        stateskeydb.drop(txn);
    }

    saveStateEvents(txn, statesdb, stateskeydb, membersdb, eventsDb, room, state.events);

    RoomInfo updatedInfo;

    {
        try {
            if (auto previousRoomInfo = db::getRoomInfo(txn, db->rooms, room))
                updatedInfo = std::move(*previousRoomInfo);
        } catch (const std::exception &e) {
            nhlog::db()->warn("failed to parse room info for room '{}': {}", room, e.what());
        }
    }

    updatedInfo.name       = getRoomName(txn, statesdb, membersdb).toStdString();
    updatedInfo.topic      = getRoomTopic(txn, statesdb).toStdString();
    updatedInfo.avatar_url = getRoomAvatarUrl(txn, statesdb, membersdb).toStdString();
    updatedInfo.version    = getRoomVersion(txn, statesdb).toStdString();

    updatedInfo.is_space      = getRoomIsSpace(txn, statesdb);
    updatedInfo.is_tombstoned = getRoomIsTombstoned(txn, statesdb);

    db::putRoomInfo(txn, db->rooms, room, updatedInfo);
    updateSpaces(txn, {room}, {room});
    txn.commit();
}

template<typename T>
void
Cache::saveStateEvents(db::Transaction &txn,
                       db::Store &statesdb,
                       db::Store &stateskeydb,
                       db::Store &membersdb,
                       db::Store &eventsDb,
                       const std::string &room_id,
                       const std::vector<T> &events)
{
    for (const auto &e : events)
        saveStateEvent(txn, statesdb, stateskeydb, membersdb, eventsDb, room_id, e);
}

template<class T>
void
Cache::saveStateEvent(db::Transaction &txn,
                      db::Store &statesdb,
                      db::Store &stateskeydb,
                      db::Store &membersdb,
                      db::Store &eventsDb,
                      const std::string &room_id,
                      const T &event)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    if (auto e = std::get_if<StateEvent<Member>>(&event); e != nullptr) {
        switch (e->content.membership) {
        //
        // We only keep users with invite or join membership.
        //
        case Membership::Invite:
        case Membership::Join: {
            auto display_name =
              e->content.display_name.empty() ? e->state_key : e->content.display_name;

            std::string inviter = "";
            if (e->content.membership == mtx::events::state::Membership::Invite) {
                inviter = e->sender;
            }

            // Lightweight representation of a member.
            MemberInfo tmp{
              display_name,
              e->content.avatar_url,
              inviter,
              e->content.reason,
              e->content.is_direct,
            };

            db::putMemberInfo(txn, membersdb, e->state_key, tmp);
            break;
        }
        default: {
            membersdb.del(txn, e->state_key, "");
            break;
        }
        }
    } else if (auto encr = std::get_if<StateEvent<Encryption>>(&event)) {
        if (!encr->state_key.empty())
            return;

        setEncryptedRoom(txn, room_id);

        std::string_view temp;
        // ensure we don't replace the event in the db
        if (statesdb.get(txn, to_string(encr->type), temp)) {
            return;
        }
    }

    std::visit(
      [&txn, &statesdb, &stateskeydb, &eventsDb, &membersdb](const auto &e) {
          if constexpr (isStateEvent_<decltype(e)>) {
              eventsDb.put(txn, e.event_id, nlohmann::json(e).dump());

              if (e.type != EventType::Unsupported) {
                  if (std::is_same_v<std::remove_cv_t<std::remove_reference_t<decltype(e)>>,
                                     StateEvent<mtx::events::msg::Redacted>>) {
                      // apply the redaction event
                      if (e.type == EventType::RoomMember) {
                          // membership is not revoked, but names are yeeted (so we set the name
                          // to the mxid)
                          MemberInfo tmp{e.state_key, ""};
                          db::putMemberInfo(txn, membersdb, e.state_key, tmp);
                      } else if (e.state_key.empty()) {
                          // strictly speaking some stuff in those events can be redacted, but
                          // this is close enough. Ref:
                          // https://spec.matrix.org/v1.6/rooms/v10/#redactions
                          if (e.type != EventType::RoomCreate &&
                              e.type != EventType::RoomJoinRules &&
                              e.type != EventType::RoomPowerLevels &&
                              e.type != EventType::RoomHistoryVisibility)
                              statesdb.del(txn, to_string(e.type));
                      } else
                          db::removeStateEventId(
                            txn, stateskeydb, to_string(e.type), e.state_key, e.event_id);
                  } else if (e.state_key.empty()) {
                      statesdb.put(txn, to_string(e.type), nlohmann::json(e).dump());
                  } else {
                      db::putStateEventId(
                        txn, stateskeydb, to_string(e.type), e.state_key, e.event_id);
                  }
              }
          }
      },
      event);
}

void
Cache::saveStateEvent(db::Transaction &txn,
                      db::Store &statesdb,
                      db::Store &stateskeydb,
                      db::Store &membersdb,
                      db::Store &eventsDb,
                      const std::string &room_id,
                      const mtx::events::collections::StateEvents &event)
{
    Cache::saveStateEvent<mtx::events::collections::StateEvents>(
      txn, statesdb, stateskeydb, membersdb, eventsDb, room_id, event);
}

template void
Cache::saveStateEvents<mtx::events::collections::StateEvents>(db::Transaction &txn,
                                                             db::Store &statesdb,
                                                             db::Store &stateskeydb,
                                                             db::Store &membersdb,
                                                             db::Store &eventsDb,
                                                             const std::string &room_id,
                                                             const std::vector<mtx::events::collections::StateEvents> &events);

template void
Cache::saveStateEvents<mtx::events::collections::TimelineEvents>(db::Transaction &txn,
                                                               db::Store &statesdb,
                                                               db::Store &stateskeydb,
                                                               db::Store &membersdb,
                                                               db::Store &eventsDb,
                                                               const std::string &room_id,
                                                               const std::vector<mtx::events::collections::TimelineEvents> &events);

// no-op: moved transaction-based state-event getters to CacheLifecycleStateHelpersGetters.cpp
