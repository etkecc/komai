// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Utils.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include <mtx/responses/messages.hpp>

#include "ChatPage.h"
#include "EventAccessors.h"
#include "Logging.h"
#include "MatrixClient.h"
#include "cache/Cache.h"
#include "settings/ui/facade/UserSettingsPage.h"

void
utils::removeDirectFromRoom(QString roomid)
{
    http::client()->get_account_data<mtx::events::account_data::Direct>(
      [roomid](mtx::events::account_data::Direct ev, mtx::http::RequestErr e) {
          if (e && e->status_code == 404)
              ev = {};
          else if (e) {
              nhlog::net()->error("Failed to retrieve m.direct: {}", *e);
              return;
          }

          auto r = roomid.toStdString();

          for (auto it = ev.user_to_rooms.begin(); it != ev.user_to_rooms.end();) {
              for (auto rit = it->second.begin(); rit != it->second.end();) {
                  if (r == *rit)
                      rit = it->second.erase(rit);
                  else
                      ++rit;
              }

              if (it->second.empty())
                  it = ev.user_to_rooms.erase(it);
              else
                  ++it;
          }

          http::client()->put_account_data(ev, [r](mtx::http::RequestErr e) {
              if (e)
                  nhlog::net()->error("Failed to update m.direct: {}", *e);
          });
      });
}

void
utils::markRoomAsDirect(QString roomid, std::vector<RoomMember> members)
{
    http::client()->get_account_data<mtx::events::account_data::Direct>(
      [roomid, members](mtx::events::account_data::Direct ev, mtx::http::RequestErr e) {
          if (e && e->status_code == 404)
              ev = {};
          else if (e) {
              nhlog::net()->error("Failed to retrieve m.direct: {}", *e);
              return;
          }

          auto local = utils::localUser();
          auto r     = roomid.toStdString();

          for (const auto &m : members) {
              if (m.user_id != local) {
                  ev.user_to_rooms[m.user_id.toStdString()].push_back(r);
              }
          }

          http::client()->put_account_data(ev, [r](mtx::http::RequestErr e) {
              if (e)
                  nhlog::net()->error("Failed to update m.direct: {}", *e);
          });
      });
}

std::vector<std::string>
utils::roomVias(const std::string &roomid)
{
    std::vector<std::string> vias;

    // for joined rooms
    {
        // see https://spec.matrix.org/v1.6/appendices/#routing for the algorithm

        auto members = cache::roomMembers(roomid);
        if (!members.empty()) {
            auto powerlevels =
              cache::getStateEvent<mtx::events::state::PowerLevels>(roomid).value_or(
                mtx::events::StateEvent<mtx::events::state::PowerLevels>{});
            auto acls = cache::getStateEvent<mtx::events::state::ServerAcl>(roomid);

            std::vector<QRegularExpression> allowedServers;
            std::vector<QRegularExpression> deniedServers;

            if (acls) {
                auto globToRegexp = [](const std::string &globExp) {
                    auto rawReg = QRegularExpression::escape(QString::fromStdString(globExp))
                                    .replace("\\*", ".*")
                                    .replace("\\?", ".");
                    return QRegularExpression(QRegularExpression::anchoredPattern(rawReg),
                                              QRegularExpression::DotMatchesEverythingOption |
                                                QRegularExpression::DontCaptureOption);
                };

                allowedServers.reserve(acls->content.allow.size());
                for (const auto &s : acls->content.allow)
                    allowedServers.push_back(globToRegexp(s));
                deniedServers.reserve(acls->content.deny.size());
                for (const auto &s : acls->content.deny)
                    allowedServers.push_back(globToRegexp(s));
            }

            auto isHostAllowed = [&acls, &allowedServers, &deniedServers](const std::string &host) {
                if (!acls)
                    return true;

                auto url = QUrl::fromEncoded(
                  "https://" + QByteArray::fromRawData(host.data(), host.size()), QUrl::StrictMode);
                if (url.hasQuery() || url.hasFragment())
                    return false;

                auto hostname = url.host();

                for (const auto &d : deniedServers)
                    if (d.match(hostname).hasMatch())
                        return false;
                for (const auto &a : allowedServers)
                    if (a.match(hostname).hasMatch())
                        return true;

                return false;
            };

            std::unordered_set<std::string> users_with_high_pl;
            std::set<std::string> users_with_high_pl_in_room;
            // we should pick PL > 50, but imo that is broken, so we just pick users who have admins
            // perm
            for (const auto &user : powerlevels.content.users) {
                if (user.second >= powerlevels.content.events_default &&
                    user.second >= powerlevels.content.state_default) {
                    auto host =
                      mtx::identifiers::parse<mtx::identifiers::User>(user.first).hostname();
                    if (isHostAllowed(host))
                        users_with_high_pl.insert(user.first);
                }
            }

            std::unordered_map<std::string, std::size_t> usercount_by_server;
            for (const auto &m : members) {
                auto user_id = mtx::identifiers::parse<mtx::identifiers::User>(m);
                usercount_by_server[user_id.hostname()] += 1;

                if (users_with_high_pl.count(m))
                    users_with_high_pl_in_room.insert(m);
            }

            std::erase_if(usercount_by_server, [&isHostAllowed](const auto &item) {
                return !isHostAllowed(item.first);
            });

            // add the highest powerlevel user
            auto max_pl_user = std::max_element(
              users_with_high_pl_in_room.begin(),
              users_with_high_pl_in_room.end(),
              [&pl_content = powerlevels.content](const std::string &a, const std::string &b) {
                  return pl_content.user_level(a) < pl_content.user_level(b);
              });
            if (max_pl_user != users_with_high_pl_in_room.end()) {
                auto host =
                  mtx::identifiers::parse<mtx::identifiers::User>(*max_pl_user).hostname();
                vias.push_back(host);
                usercount_by_server.erase(host);
            }

            // add up to 3 users, by usercount size from that server
            std::vector<std::pair<std::size_t, std::string>> servers_sorted_by_usercount;
            servers_sorted_by_usercount.reserve(usercount_by_server.size());
            for (const auto &[server, count] : usercount_by_server)
                servers_sorted_by_usercount.emplace_back(count, server);

            std::sort(servers_sorted_by_usercount.begin(),
                      servers_sorted_by_usercount.end(),
                      [](const auto &a, const auto &b) {
                          if (a.first == b.first)
                              // same pl, sort lex smaller server first
                              return a.second < b.second;

                          // sort high user count first
                          return a.first > b.first;
                      });

            for (const auto &server : servers_sorted_by_usercount) {
                if (vias.size() >= 3)
                    break;

                vias.push_back(server.second);
            }

            return vias;
        }
    }

    // for invites
    {
        auto members = cache::getMembersFromInvite(roomid, 0, 100);
        if (!members.empty()) {
            try {
                vias.push_back(
                  mtx::identifiers::parse<mtx::identifiers::User>(localUser().toStdString())
                    .hostname());
            } catch (const std::exception &) {
                vias.push_back(http::client()->user_id().hostname());
            }
            for (const auto &m : members) {
                if (vias.size() >= 3)
                    break;

                auto user_id =
                  mtx::identifiers::parse<mtx::identifiers::User>(m.user_id.toStdString());

                auto server = user_id.hostname();
                if (std::find(begin(vias), end(vias), server) == vias.end())
                    vias.push_back(server);
            }

            return vias;
        }
    }

    // for space previews
    auto parents = cache::getParentRoomIds(roomid);
    for (const auto &p : parents) {
        auto child = cache::getStateEvent<mtx::events::state::space::Child>(p, roomid);
        if (child && child->content.via)
            vias.insert(vias.end(), child->content.via->begin(), child->content.via->end());
    }

    std::sort(begin(vias), end(vias));
    auto last = std::unique(begin(vias), end(vias));
    vias.erase(last, end(vias));

    return vias;
}

void
utils::updateSpaceVias()
{
    if (!UserSettings::instance()->privacyMaintenanceUpdateSpaceVias())
        return;

    nhlog::net()->info("update space vias called");

    auto rooms = cache::roomInfo(false);

    auto us = utils::localUser().toStdString();

    auto weekAgo = (uint64_t)QDateTime::currentDateTime().addDays(-7).toMSecsSinceEpoch();

    struct ApplySpaceUpdatesState
    {
        std::vector<mtx::events::StateEvent<mtx::events::state::space::Child>> childrenToUpdate;
        std::vector<mtx::events::StateEvent<mtx::events::state::space::Parent>> parentsToUpdate;

        static void next(std::shared_ptr<ApplySpaceUpdatesState> state)
        {
            if (!state->childrenToUpdate.empty()) {
                const auto &child = state->childrenToUpdate.back();

                http::client()->send_state_event(
                  child.room_id,
                  child.state_key,
                  child.content,
                  [state = std::move(state)](const mtx::responses::EventId &,
                                             mtx::http::RequestErr e) mutable {
                      const auto &child_ = state->childrenToUpdate.back();
                      if (e) {
                          if (e->status_code == 429 && e->matrix_error.retry_after.count() != 0) {
                              ChatPage::instance()->callFunctionOnGuiThread(
                                [state    = std::move(state),
                                 interval = e->matrix_error.retry_after]() {
                                    QTimer::singleShot(interval * 3,
                                                       ChatPage::instance(),
                                                       [self = std::move(state)]() mutable {
                                                           next(std::move(self));
                                                       });
                                });
                              return;
                          }

                          nhlog::net()->error("Failed to update space child {} -> {}: {}",
                                              child_.room_id,
                                              child_.state_key,
                                              *e);
                      }
                      nhlog::net()->info(
                        "Updated space child {} -> {}", child_.room_id, child_.state_key);
                      state->childrenToUpdate.pop_back();
                      next(std::move(state));
                  });
                return;
            } else if (!state->parentsToUpdate.empty()) {
                const auto &parent = state->parentsToUpdate.back();

                http::client()->send_state_event(
                  parent.room_id,
                  parent.state_key,
                  parent.content,
                  [state = std::move(state)](const mtx::responses::EventId &,
                                             mtx::http::RequestErr e) mutable {
                      const auto &parent_ = state->parentsToUpdate.back();
                      if (e) {
                          if (e->status_code == 429 && e->matrix_error.retry_after.count() != 0) {
                              ChatPage::instance()->callFunctionOnGuiThread(
                                [state    = std::move(state),
                                 interval = e->matrix_error.retry_after]() {
                                    QTimer::singleShot(interval * 3,
                                                       ChatPage::instance(),
                                                       [self = std::move(state)]() mutable {
                                                           next(std::move(self));
                                                       });
                                });
                              return;
                          }

                          nhlog::net()->error("Failed to update space parent {} -> {}: {}",
                                              parent_.room_id,
                                              parent_.state_key,
                                              *e);
                      }
                      nhlog::net()->info(
                        "Updated space parent {} -> {}", parent_.room_id, parent_.state_key);
                      state->parentsToUpdate.pop_back();
                      next(std::move(state));
                  });
                return;
            }
        }
    };

    auto asus = std::make_shared<ApplySpaceUpdatesState>();

    for (const auto &[roomid, info] : rooms.toStdMap()) {
        if (!info.is_space)
            continue;

        auto spaceid = roomid.toStdString();

        if (auto pl = cache::getStateEvent<mtx::events::state::PowerLevels>(spaceid)
                        .value_or(mtx::events::StateEvent<mtx::events::state::PowerLevels>{})
                        .content;
            pl.user_level(us) < pl.state_level(to_string(mtx::events::EventType::SpaceChild)))
            continue;

        auto children = cache::getChildRoomIds(spaceid);

        for (const auto &childid : children) {
            // only update children we are joined to
            if (!rooms.contains(QString::fromStdString(childid)))
                continue;

            auto child = cache::getStateEvent<mtx::events::state::space::Child>(spaceid, childid);
            if (child &&
                // don't update too often
                child->origin_server_ts < weekAgo &&
                // ignore unset spaces
                (child->content.via && !child->content.via->empty())) {
                auto newVias = utils::roomVias(childid);

                if (!newVias.empty() && newVias != child->content.via) {
                    nhlog::net()->info("Will update {} -> {} child relation from {} to {}",
                                       spaceid,
                                       childid,
                                       fmt::join(*child->content.via, ","),
                                       fmt::join(newVias, ","));

                    child->content.via = std::move(newVias);
                    child->room_id     = spaceid;
                    asus->childrenToUpdate.push_back(*std::move(child));
                }
            }

            auto parent = cache::getStateEvent<mtx::events::state::space::Parent>(childid, spaceid);
            if (parent &&
                // don't update too often
                parent->origin_server_ts < weekAgo &&
                // ignore unset spaces
                (parent->content.via && !parent->content.via->empty())) {
                if (auto pl =
                      cache::getStateEvent<mtx::events::state::PowerLevels>(childid)
                        .value_or(mtx::events::StateEvent<mtx::events::state::PowerLevels>{})
                        .content;
                    pl.user_level(us) <
                    pl.state_level(to_string(mtx::events::EventType::SpaceParent)))
                    continue;

                auto newVias = utils::roomVias(spaceid);

                if (!newVias.empty() && newVias != parent->content.via) {
                    nhlog::net()->info("Will update {} -> {} parent relation from {} to {}",
                                       childid,
                                       spaceid,
                                       fmt::join(*parent->content.via, ","),
                                       fmt::join(newVias, ","));

                    parent->content.via = std::move(newVias);
                    parent->room_id     = childid;
                    asus->parentsToUpdate.push_back(*std::move(parent));
                }
            }
        }
    }

    ApplySpaceUpdatesState::next(std::move(asus));
}

std::atomic<bool> event_expiration_running = false;
void
utils::removeExpiredEvents()
{
    if (!UserSettings::instance()->privacyMaintenanceExpireEvents())
        return;

    if (event_expiration_running.exchange(true)) {
        nhlog::net()->info("Event expiration still running, not starting second job.");
        return;
    }

    nhlog::net()->info("Remove expired events starting.");

    auto rooms = cache::roomInfo(false);

    auto us = utils::localUser().toStdString();

    using ExpType = mtx::events::account_data::nheko_extensions::EventExpiry;
    static constexpr std::string_view KOMAI_EVENT_EXPIRY_TYPE = "cc.etke.komai.event_expiry";
    static auto getExpEv = [](const std::string &room = "") -> std::optional<ExpType> {
        if (auto raw = cache::getAccountDataByType(std::string(KOMAI_EVENT_EXPIRY_TYPE), room)) {
            try {
                const auto parsedEvent = nlohmann::json::parse(*raw);
                if (parsedEvent.is_object() && parsedEvent.contains("content")) {
                    auto content = parsedEvent.at("content").get<ExpType>();
                    if (content.expire_after_ms || content.keep_only_latest)
                        return std::optional{std::move(content)};
                }
            } catch (const std::exception &) {
            }
        }
        return std::nullopt;
    };

    struct ApplyEventExpiration
    {
        std::optional<ExpType> globalExpiry;
        std::vector<std::string> roomsToUpdate;
        std::string filter;

        std::string currentRoom;
        bool firstMessagesCall         = true;
        std::uint64_t currentRoomCount = 0;

        // batch token for pagination
        std::string currentRoomPrevToken;
        // event id of an event redacted in a previous run
        std::string currentRoomStopAt;
        // event id of first event redacted in the current run, hoping that the order stays the
        // same.
        std::string currentRoomFirstRedactedEvent;
        // (evtype,state_key) tuple to keep the latest state event of each.
        std::set<std::pair<std::string, std::string>> currentRoomStateEvents;
        // event ids pending redaction
        std::vector<std::string> currentRoomRedactionQueue;

        mtx::events::account_data::nheko_extensions::EventExpiry currentExpiry;

        static void next(std::shared_ptr<ApplyEventExpiration> state)
        {
            if (!state->currentRoomRedactionQueue.empty()) {
                auto evid = state->currentRoomRedactionQueue.back();
                auto room = state->currentRoom;
                http::client()->redact_event(
                  room,
                  evid,
                  [state = std::move(state), evid](const mtx::responses::EventId &,
                                                   mtx::http::RequestErr e) mutable {
                      if (e) {
                          if (e->status_code == 429 && e->matrix_error.retry_after.count() != 0) {
                              ChatPage::instance()->callFunctionOnGuiThread(
                                [state    = std::move(state),
                                 interval = e->matrix_error.retry_after]() {
                                    // triple interval to allow other traffic as well
                                    QTimer::singleShot(interval * 3,
                                                       ChatPage::instance(),
                                                       [self = std::move(state)]() mutable {
                                                           next(std::move(self));
                                                       });
                                });
                              return;
                          } else {
                              nhlog::net()->error("Failed to redact event {} in {}: {}",
                                                  evid,
                                                  state->currentRoom,
                                                  *e);
                              state->currentRoomRedactionQueue.pop_back();
                              next(std::move(state));
                          }
                      } else {
                          nhlog::net()->info("Redacted event {} in {}", evid, state->currentRoom);

                          if (state->currentRoomFirstRedactedEvent.empty())
                              state->currentRoomFirstRedactedEvent = evid;

                          state->currentRoomRedactionQueue.pop_back();
                          next(std::move(state));
                      }
                  });
            } else if (!state->currentRoom.empty()) {
                if (state->currentRoomPrevToken.empty() && !state->firstMessagesCall) {
                    nhlog::net()->info("Finished room {}", state->currentRoom);

                    if (!state->currentRoomFirstRedactedEvent.empty())
                        cache::storeEventExpirationProgress(
                          state->currentRoom,
                          nlohmann::json(state->currentExpiry).dump(),
                          state->currentRoomFirstRedactedEvent);

                    state->currentRoom.clear();
                    next(std::move(state));
                    return;
                }

                mtx::http::MessagesOpts opts{};
                opts.dir     = mtx::http::PaginationDirection::Backwards;
                opts.from    = state->currentRoomPrevToken;
                opts.limit   = 1000;
                opts.filter  = state->filter;
                opts.room_id = state->currentRoom;

                state->firstMessagesCall = false;

                http::client()->messages(
                  opts,
                  [state = std::move(state)](const mtx::responses::Messages &msgs,
                                             mtx::http::RequestErr error) mutable {
                      if (error) {
                          // skip success handler
                          nhlog::net()->warn(
                            "Finished room {} with error {}", state->currentRoom, *error);
                          state->currentRoom.clear();
                      } else if (msgs.chunk.empty()) {
                          state->currentRoomPrevToken.clear();
                      } else {
                          state->currentRoomPrevToken = msgs.end;

                          auto now = (uint64_t)QDateTime::currentMSecsSinceEpoch();
                          auto us  = utils::localUser().toStdString();

                          for (const auto &e : msgs.chunk) {
                              if (std::holds_alternative<
                                    mtx::events::RedactionEvent<mtx::events::msg::Redaction>>(e))
                                  continue;

                              if (std::holds_alternative<
                                    mtx::events::RoomEvent<mtx::events::msg::Redacted>>(e) ||
                                  std::holds_alternative<
                                    mtx::events::StateEvent<mtx::events::msg::Redacted>>(e)) {
                                  if (!state->currentRoomStopAt.empty() &&
                                      mtx::accessors::event_id(e) == state->currentRoomStopAt) {
                                      // There is no filter to remove redacted events from
                                      // pagination, so we try to stop early by caching what event
                                      // we redacted last if we reached the end of a room.
                                      nhlog::net()->info(
                                        "Found previous redaction marker, stopping early: {}",
                                        state->currentRoom);
                                      state->currentRoomPrevToken.clear();
                                      break;
                                  }
                                  continue;
                              }

                              if (std::holds_alternative<
                                    mtx::events::StateEvent<mtx::events::msg::Redacted>>(e))
                                  continue;

                              // synapse protects these 2 against redaction
                              if (std::holds_alternative<
                                    mtx::events::StateEvent<mtx::events::state::Create>>(e))
                                  continue;

                              if (std::holds_alternative<
                                    mtx::events::StateEvent<mtx::events::state::ServerAcl>>(e))
                                  continue;

                              // skip events we don't know to protect us from mistakes.
                              if (std::holds_alternative<
                                    mtx::events::RoomEvent<mtx::events::Unknown>>(e))
                                  continue;

                              if (mtx::accessors::sender(e) != us)
                                  continue;

                              state->currentRoomCount++;
                              if (state->currentRoomCount <= state->currentExpiry.protect_latest) {
                                  continue;
                              }

                              if (state->currentExpiry.exclude_state_events &&
                                  mtx::accessors::is_state_event(e))
                                  continue;

                              if (mtx::accessors::is_state_event(e)) {
                                  // skip the first state event of a type
                                  if (std::visit(
                                        [&state](const auto &se) {
                                            if constexpr (requires { se.state_key; })
                                                return state->currentRoomStateEvents
                                                  .emplace(to_string(se.type), se.state_key)
                                                  .second;
                                            else
                                                return true;
                                        },
                                        e))
                                      continue;
                              }

                              if (state->currentExpiry.keep_only_latest &&
                                  state->currentRoomCount > state->currentExpiry.keep_only_latest) {
                                  state->currentRoomRedactionQueue.push_back(
                                    mtx::accessors::event_id(e));
                              } else if (state->currentExpiry.expire_after_ms &&
                                         (state->currentExpiry.expire_after_ms +
                                          mtx::accessors::origin_server_ts(e).toMSecsSinceEpoch()) <
                                           now) {
                                  state->currentRoomRedactionQueue.push_back(
                                    mtx::accessors::event_id(e));
                              }
                          }
                      }

                      next(std::move(state));
                  });
            } else if (!state->roomsToUpdate.empty()) {
                const auto &room = state->roomsToUpdate.back();

                auto localExp = getExpEv(room);
                if (localExp) {
                    state->currentRoom   = room;
                    state->currentExpiry = *localExp;
                } else if (state->globalExpiry) {
                    state->currentRoom   = room;
                    state->currentExpiry = *state->globalExpiry;
                }
                state->firstMessagesCall    = true;
                state->currentRoomCount     = 0;
                state->currentRoomPrevToken = "";
                state->currentRoomRedactionQueue.clear();
                state->currentRoomStateEvents.clear();

                state->currentRoomStopAt = cache::loadEventExpirationProgress(
                  state->currentRoom, nlohmann::json(state->currentExpiry).dump());

                state->roomsToUpdate.pop_back();
                next(std::move(state));
            } else {
                nhlog::net()->info("Finished event expiry");
                event_expiration_running = false;
            }
        }
    };

    auto asus = std::make_shared<ApplyEventExpiration>();

    nlohmann::json filter;
    filter["timeline"]["senders"]   = nlohmann::json::array({us});
    filter["timeline"]["not_types"] = nlohmann::json::array({"m.room.redaction"});

    asus->filter = filter.dump();

    asus->globalExpiry = getExpEv();

    for (const auto &[roomid_, info] : rooms.toStdMap()) {
        auto roomid = roomid_.toStdString();

        if (!asus->globalExpiry && !getExpEv(roomid))
            continue;

        if (auto pl = cache::getStateEvent<mtx::events::state::PowerLevels>(roomid)
                        .value_or(mtx::events::StateEvent<mtx::events::state::PowerLevels>{})
                        .content;
            pl.user_level(us) < pl.event_level(to_string(mtx::events::EventType::RoomRedaction))) {
            nhlog::net()->warn("Can't react events in {}, not running expiration.", roomid);
            continue;
        }

        asus->roomsToUpdate.push_back(roomid);
    }

    nhlog::db()->info("Running expiration in {} rooms", asus->roomsToUpdate.size());

    ApplyEventExpiration::next(std::move(asus));
}
