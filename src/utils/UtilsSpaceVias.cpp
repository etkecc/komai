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
utils::updateSpaceVias()
{
    if (!UserSettings::instance()->networkSpacesMaintainJoinMetadata())
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
