// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/api/CacheApiTimeline.h"
#include "cache/api/CacheApiContext.h"
#include "cache/api/CacheApiLifecycle.h"
#include "cache/core/Cache_p.h"

namespace cache {

std::optional<mtx::events::collections::TimelineEvents>
getEvent(const std::string &room_id, std::string_view event_id)
{
    return cacheInstance()->getEvent(room_id, event_id);
}
void
storeEvent(const std::string &room_id,
           const std::string &event_id,
           const mtx::events::collections::TimelineEvents &event)
{
    cacheInstance()->storeEvent(room_id, event_id, event);
}
void
replaceEvent(const std::string &room_id,
             const std::string &event_id,
             const mtx::events::collections::TimelineEvents &event)
{
    cacheInstance()->replaceEvent(room_id, event_id, event);
}
std::vector<std::string>
relatedEvents(const std::string &room_id, const std::string &event_id)
{
    return cacheInstance()->relatedEvents(room_id, event_id);
}
std::optional<TimelineRange>
getTimelineRange(const std::string &room_id)
{
    auto range = cacheInstance()->getTimelineRange(room_id);
    if (!range)
        return std::nullopt;

    return TimelineRange{.first = range->first, .last = range->last};
}
std::optional<uint64_t>
getTimelineIndex(const std::string &room_id, std::string_view event_id)
{
    return cacheInstance()->getTimelineIndex(room_id, event_id);
}
std::optional<std::string>
getTimelineEventId(const std::string &room_id, uint64_t index)
{
    return cacheInstance()->getTimelineEventId(room_id, index);
}
uint64_t
saveOldMessages(const std::string &room_id, const mtx::responses::Messages &res)
{
    return cacheInstance()->saveOldMessages(room_id, res);
}
void
savePendingMessage(const std::string &room_id,
                   const mtx::events::collections::TimelineEvents &message)
{
    cacheInstance()->savePendingMessage(room_id, message);
}
std::vector<std::string>
pendingEvents(const std::string &room_id)
{
    return cacheInstance()->pendingEvents(room_id);
}
std::optional<mtx::events::collections::TimelineEvents>
firstPendingMessage(const std::string &room_id)
{
    return cacheInstance()->firstPendingMessage(room_id);
}
void
removePendingStatus(const std::string &room_id, const std::string &txn_id)
{
    cacheInstance()->removePendingStatus(room_id, txn_id);
}
void
clearTimeline(const std::string &room_id)
{
    cacheInstance()->clearTimeline(room_id);
}

std::optional<uint64_t>
getEventIndex(const std::string &room_id, std::string_view event_id)
{
    return cacheInstance()->getEventIndex(room_id, event_id);
}

std::optional<std::pair<uint64_t, std::string>>
lastInvisibleEventAfter(const std::string &room_id, std::string_view event_id)
{
    return cacheInstance()->lastInvisibleEventAfter(room_id, event_id);
}

std::optional<std::pair<uint64_t, std::string>>
lastVisibleEvent(const std::string &room_id, std::string_view event_id)
{
    return cacheInstance()->lastVisibleEvent(room_id, event_id);
}

void
markSentNotification(const std::string &event_id)
{
    cacheInstance()->markSentNotification(event_id);
}
//! Removes an event from the sent notifications.
void
removeReadNotification(const std::string &event_id)
{
    cacheInstance()->removeReadNotification(event_id);
}
//! Check if we have sent a desktop notification for the given event id.
bool
isNotificationSent(const std::string &event_id)
{
    return cacheInstance()->isNotificationSent(event_id);
}

//! Remove old unused data.
void
deleteOldMessages()
{
    cacheInstance()->deleteOldMessages();
}
void
deleteOldData() noexcept
{
    cacheInstance()->deleteOldData();
}
void
storeEventExpirationProgress(const std::string &room,
                             const std::string &expirationSettings,
                             const std::string &event_id)
{
    cacheInstance()->storeEventExpirationProgress(room, expirationSettings, event_id);
}
std::string
loadEventExpirationProgress(const std::string &room, const std::string &expirationSettings)
{
    return cacheInstance()->loadEventExpirationProgress(room, expirationSettings);
}
} // namespace cache
