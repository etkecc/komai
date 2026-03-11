// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "cache/api/CacheApiTypes.h"

namespace cache {
std::optional<mtx::events::collections::TimelineEvents>
getEvent(const std::string &room_id, std::string_view event_id);
void
storeEvent(const std::string &room_id,
           const std::string &event_id,
           const mtx::events::collections::TimelineEvents &event);
void
replaceEvent(const std::string &room_id,
             const std::string &event_id,
             const mtx::events::collections::TimelineEvents &event);
std::vector<std::string>
relatedEvents(const std::string &room_id, const std::string &event_id);

struct TimelineRange
{
    uint64_t first, last;
};
std::optional<TimelineRange>
getTimelineRange(const std::string &room_id);
std::optional<uint64_t>
getTimelineIndex(const std::string &room_id, std::string_view event_id);
std::optional<std::string>
getTimelineEventId(const std::string &room_id, uint64_t index);
uint64_t
saveOldMessages(const std::string &room_id, const mtx::responses::Messages &res);
void
savePendingMessage(const std::string &room_id,
                   const mtx::events::collections::TimelineEvents &message);
std::vector<std::string>
pendingEvents(const std::string &room_id);
std::optional<mtx::events::collections::TimelineEvents>
firstPendingMessage(const std::string &room_id);
void
removePendingStatus(const std::string &room_id, const std::string &txn_id);
void
clearTimeline(const std::string &room_id);

//! get index of the event in the event db, not representing the visual index
std::optional<uint64_t>
getEventIndex(const std::string &room_id, std::string_view event_id);
std::optional<std::pair<uint64_t, std::string>>
lastInvisibleEventAfter(const std::string &room_id, std::string_view event_id);
std::optional<std::pair<uint64_t, std::string>>
lastVisibleEvent(const std::string &room_id, std::string_view event_id);

void
markSentNotification(const std::string &event_id);
//! Removes an event from the sent notifications.
void
removeReadNotification(const std::string &event_id);
//! Check if we have sent a desktop notification for the given event id.
bool
isNotificationSent(const std::string &event_id);

void
storeEventExpirationProgress(const std::string &room,
                             const std::string &expirationSettings,
                             const std::string &event_id);
std::string
loadEventExpirationProgress(const std::string &room, const std::string &expirationSettings);
std::vector<std::string>
topUserReactions(const std::string &room_id, int lookbackDays, int maxResults);
} // namespace cache
