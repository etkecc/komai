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
