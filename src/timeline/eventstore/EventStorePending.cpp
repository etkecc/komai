// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventStore.h"

#include <QThread>

#include "cache/Cache.h"
#include "logging/Logging.h"

void
EventStore::setupPendingPipeline()
{
    const auto pendingEventIds = cache::pendingEvents(room_id_);
    if (pendingEventIds.empty())
        return;

    nhlog::ui()->warn(
      "Dropping {} cached legacy pending timeline event(s) for room '{}'; this flow is not "
      "migrated to the matrix-sdk backend yet",
      pendingEventIds.size(),
      room_id_);

    for (const auto &pendingEventId : pendingEventIds)
        cache::removePendingStatus(room_id_, pendingEventId);
}

void
EventStore::addPending(const mtx::events::collections::TimelineEvents &event)
{
    (void)event;

    if (this->thread() != QThread::currentThread())
        nhlog::db()->warn("{} called from a different thread!", __func__);

    nhlog::ui()->warn(
      "Ignoring legacy pending timeline event for room '{}'; this flow is not migrated to the "
      "matrix-sdk backend yet",
      room_id_);
}
