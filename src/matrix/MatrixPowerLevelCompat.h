// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <mtx/events.hpp>
#include <mtx/events/create.hpp>
#include <mtx/events/power_levels.hpp>

namespace komai::matrix {

inline constexpr auto CreatorPowerLevel =
  std::numeric_limits<mtx::events::state::power_level_t>::max();

inline bool
roomVersionCreatorsHaveInfinitePower(std::string_view roomVersion)
{
    // Matches the legacy mtxclient behavior for room versions where create-event
    // senders implicitly had infinite power.
    return roomVersion.length() > 1 && roomVersion != "10" && roomVersion != "11";
}

inline bool
createEventCreatorsHaveInfinitePower(const mtx::events::state::Create &create)
{
    return roomVersionCreatorsHaveInfinitePower(create.room_version);
}

template<typename CreateEvent>
bool
createEventCreatorsHaveInfinitePower(const CreateEvent &createEvent)
{
    return createEventCreatorsHaveInfinitePower(createEvent.content);
}

template<typename CreateEvent>
std::vector<std::string>
createEventCreators(const CreateEvent &createEvent)
{
    std::vector<std::string> creators;
    creators.push_back(createEvent.sender);

    if constexpr (requires { createEvent.content.additional_creators; }) {
        creators.insert(creators.end(),
                        createEvent.content.additional_creators.begin(),
                        createEvent.content.additional_creators.end());
    }

    return creators;
}

template<typename CreateEvent>
bool
isCreateEventCreator(const CreateEvent &createEvent, std::string_view userId)
{
    const auto creators = createEventCreators(createEvent);
    return std::find(creators.begin(), creators.end(), userId) != creators.end();
}

template<typename CreateEvent>
mtx::events::state::power_level_t
effectiveUserPowerLevel(const mtx::events::state::PowerLevels &powerLevels,
                        const CreateEvent &createEvent,
                        const std::string &userId)
{
    if (createEventCreatorsHaveInfinitePower(createEvent) &&
        isCreateEventCreator(createEvent, userId)) {
        return CreatorPowerLevel;
    }

    return powerLevels.user_level(userId);
}

} // namespace komai::matrix
