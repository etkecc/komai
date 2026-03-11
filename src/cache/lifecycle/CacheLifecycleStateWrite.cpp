// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <exception>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <mtx/responses/common.hpp>
#include <mtx/responses/messages.hpp>
#include <mtxclient/utils.hpp>
#include <nlohmann/json.hpp>

#include "events/EventAccessors.h"
#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"
#include "cache/schema/RoomStore.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "utils/Utils.h"

void
MatrixStore::updateState(const std::string &room,
                         const mtx::responses::StateEvents &state,
                         bool wipe)
{
    auto txn         = beginTxn();
    auto statesdb    = getStatesDb(txn, room);
    auto stateskeydb = getStatesKeyDb(txn, room);
    auto membersdb   = getMembersDb(txn, room);
    auto eventsDb    = getEventsDb(txn, room);

    if (wipe) {
        room_store::eraseEntries(txn, membersdb, cache::schema::RoomDb::Members, room);
        room_store::eraseEntries(txn, statesdb, cache::schema::RoomDb::State, room);
        room_store::eraseEntries(txn, stateskeydb, cache::schema::RoomDb::StatesKey, room);
    }

    saveStateEvents(txn, statesdb, stateskeydb, membersdb, eventsDb, room, state.events);

    RoomInfo updatedInfo;

    {
        try {
            if (auto previousRoomInfo = cache::codec::getRoomInfo(txn, db->rooms, room))
                updatedInfo = std::move(*previousRoomInfo);
        } catch (const std::exception &e) {
            cache::activeLoggers().db->warn(
              "failed to parse room info for room '{}': {}", room, e.what());
        }
    }

    updatedInfo.name       = getRoomName(txn, room, statesdb, membersdb).toStdString();
    updatedInfo.topic      = getRoomTopic(txn, room, statesdb).toStdString();
    updatedInfo.avatar_url = getRoomAvatarUrl(txn, room, statesdb, membersdb).toStdString();
    updatedInfo.version    = getRoomVersion(txn, room, statesdb).toStdString();

    updatedInfo.is_space      = getRoomIsSpace(txn, room, statesdb);
    updatedInfo.is_tombstoned = getRoomIsTombstoned(txn, room, statesdb);

    cache::codec::putRoomInfo(txn, db->rooms, room, updatedInfo);
    updateSpaces(txn, {room}, {room});
    txn.commit();
}

template<typename T>
void
MatrixStore::saveStateEvents(db::Transaction &txn,
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
MatrixStore::saveStateEvent(db::Transaction &txn,
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

            room_store::put(txn,
                            membersdb,
                            cache::schema::RoomDb::Members,
                            room_id,
                            e->state_key,
                            cache::codec::serializeMemberInfo(tmp));
            break;
        }
        default: {
            room_store::del(txn, membersdb, cache::schema::RoomDb::Members, room_id, e->state_key);
            break;
        }
        }
    } else if (auto encr = std::get_if<StateEvent<Encryption>>(&event)) {
        if (!encr->state_key.empty())
            return;

        setEncryptedRoom(txn, room_id);

        std::string_view temp;
        // ensure we don't replace the event in the db
        if (room_store::get(
              txn, statesdb, cache::schema::RoomDb::State, room_id, to_string(encr->type), temp)) {
            return;
        }
    }

    std::visit(
      [&txn, &statesdb, &stateskeydb, &eventsDb, &membersdb, &room_id](const auto &e) {
          if constexpr (isStateEvent_<decltype(e)>) {
              room_store::put(txn,
                              eventsDb,
                              cache::schema::RoomDb::Events,
                              room_id,
                              e.event_id,
                              nlohmann::json(e).dump());

              if (e.type != EventType::Unsupported) {
                  if (std::is_same_v<std::remove_cv_t<std::remove_reference_t<decltype(e)>>,
                                     StateEvent<mtx::events::msg::Redacted>>) {
                      // apply the redaction event
                      if (e.type == EventType::RoomMember) {
                          // membership is not revoked, but names are yeeted (so we set the name
                          // to the mxid)
                          MemberInfo tmp{e.state_key, ""};
                          room_store::put(txn,
                                          membersdb,
                                          cache::schema::RoomDb::Members,
                                          room_id,
                                          e.state_key,
                                          cache::codec::serializeMemberInfo(tmp));
                      } else if (e.state_key.empty()) {
                          // strictly speaking some stuff in those events can be redacted, but
                          // this is close enough. Ref:
                          // https://spec.matrix.org/v1.6/rooms/v10/#redactions
                          if (e.type != EventType::RoomCreate &&
                              e.type != EventType::RoomJoinRules &&
                              e.type != EventType::RoomPowerLevels &&
                              e.type != EventType::RoomHistoryVisibility)
                              room_store::del(txn,
                                              statesdb,
                                              cache::schema::RoomDb::State,
                                              room_id,
                                              to_string(e.type));
                      } else
                          db::removeStateEventId(txn,
                                                 stateskeydb,
                                                 room_store::key(cache::schema::RoomDb::StatesKey,
                                                                 room_id,
                                                                 to_string(e.type)),
                                                 e.state_key,
                                                 e.event_id);
                  } else if (e.state_key.empty()) {
                      room_store::put(txn,
                                      statesdb,
                                      cache::schema::RoomDb::State,
                                      room_id,
                                      to_string(e.type),
                                      nlohmann::json(e).dump());
                  } else {
                      db::putStateEventId(txn,
                                          stateskeydb,
                                          room_store::key(cache::schema::RoomDb::StatesKey,
                                                          room_id,
                                                          to_string(e.type)),
                                          e.state_key,
                                          e.event_id);
                  }
              }
          }
      },
      event);
}

void
MatrixStore::saveStateEvent(db::Transaction &txn,
                            db::Store &statesdb,
                            db::Store &stateskeydb,
                            db::Store &membersdb,
                            db::Store &eventsDb,
                            const std::string &room_id,
                            const mtx::events::collections::StateEvents &event)
{
    MatrixStore::saveStateEvent<mtx::events::collections::StateEvents>(
      txn, statesdb, stateskeydb, membersdb, eventsDb, room_id, event);
}

template void
MatrixStore::saveStateEvents<mtx::events::collections::StateEvents>(
  db::Transaction &txn,
  db::Store &statesdb,
  db::Store &stateskeydb,
  db::Store &membersdb,
  db::Store &eventsDb,
  const std::string &room_id,
  const std::vector<mtx::events::collections::StateEvents> &events);

template void
MatrixStore::saveStateEvents<mtx::events::collections::TimelineEvents>(
  db::Transaction &txn,
  db::Store &statesdb,
  db::Store &stateskeydb,
  db::Store &membersdb,
  db::Store &eventsDb,
  const std::string &room_id,
  const std::vector<mtx::events::collections::TimelineEvents> &events);
