// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/Utils.h"

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

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
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
