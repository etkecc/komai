// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

#include <exception>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <mtx/responses/common.hpp>
#include <mtx/responses/messages.hpp>
#include <mtxclient/utils.hpp>
#include <nlohmann/json.hpp>

#include "EventAccessors.h"
#include <spdlog/logger.h>

#include "CacheApiWrappers.h"
#include "UserSettingsPage.h"
#include "Utils.h"
#include "db/Maintenance.h"

template<typename T>
std::optional<mtx::events::StateEvent<T>>
Cache::getStateEvent(db::Transaction &txn, const std::string &room_id, std::string_view state_key)
{
    try {
        constexpr auto type = mtx::events::state_content_to_type<T>;
        static_assert(type != mtx::events::EventType::Unsupported,
                      "Not a supported type in state events.");

        if (room_id.empty())
            return std::nullopt;
        const auto typeStr = to_string(type);

        if (state_key.empty()) {
            auto db_ = getStatesDb(txn, room_id);
            return db::getJsonValue<mtx::events::StateEvent<T>>(txn, db_, typeStr);
        } else {
            try {
                auto statesKeyDb = getStatesKeyDb(txn, room_id);
                auto eventsDb    = getEventsDb(txn, room_id);
                auto eventId     = db::findStateEventId(txn, statesKeyDb, typeStr, state_key);
                if (!eventId) {
                    return std::nullopt;
                }
                return db::getJsonValue<mtx::events::StateEvent<T>>(txn, eventsDb, *eventId);

            } catch (std::exception &) {
                return std::nullopt;
            }
        }
    } catch (std::exception &) {
        return std::nullopt;
    }
}

template<typename T>
std::vector<mtx::events::StateEvent<T>>
Cache::getStateEventsWithType(db::Transaction &txn,
                              const std::string &room_id,
                              mtx::events::EventType type)

{
    if (room_id.empty())
        return {};

    std::vector<mtx::events::StateEvent<T>> events;

    {
        auto statesKeyDb   = getStatesKeyDb(txn, room_id);
        auto eventsDb      = getEventsDb(txn, room_id);
        const auto typeStr = to_string(type);
        std::string_view value;

        for (const auto &eventId : db::listStateEventIds(txn, statesKeyDb, typeStr)) {
            try {
                if (auto event =
                      db::getJsonValue<mtx::events::StateEvent<T>>(txn, eventsDb, eventId))
                    events.push_back(std::move(*event));
            } catch (std::exception &e) {
                if (const auto logger = cache::activeLoggers().db)
                    logger->warn("Failed to parse state event: {}", e.what());
            }
        }
    }

    return events;
}

#define NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(Content)                                        \
    template std::optional<mtx::events::StateEvent<Content>> Cache::getStateEvent<Content>(        \
      db::Transaction & txn, const std::string &room_id, std::string_view state_key);              \
                                                                                                   \
    template std::vector<mtx::events::StateEvent<Content>> Cache::getStateEventsWithType<Content>( \
      db::Transaction & txn, const std::string &room_id, mtx::events::EventType type);

NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::msc2545::ImagePack)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::Aliases)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::Avatar)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::CanonicalAlias)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::Create)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::Encryption)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::GuestAccess)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::HistoryVisibility)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::JoinRules)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::Member)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::Name)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::PinnedEvents)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::PowerLevels)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::Tombstone)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::ServerAcl)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::Topic)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::Widget)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::policy_rule::UserRule)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::policy_rule::RoomRule)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::policy_rule::ServerRule)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::space::Child)
NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION(mtx::events::state::space::Parent)

#undef NHEKO_CACHE_GET_STATE_EVENT_TXN_DEFINITION
