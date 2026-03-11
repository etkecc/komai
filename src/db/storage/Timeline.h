// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/OrderEntry.h"
#include "db/TimelineIndex.h"
#include "db/storage/Core.h"

namespace db::storage {

using db::appendEventOrderEntry;
using db::appendMessageOrderEntry;
using db::cleanupTimelineBeforePrevBatchMarker;
using db::eventIndexForEvent;
using db::firstOrderedIndex;
using db::firstPrevBatchToken;
using db::lastInvisibleEventAfter;
using db::lastOrderedIndex;
using db::lastTimelineEventId;
using db::lastVisibleEvent;
using db::listOrderEntriesAfterPrevBatchMarker;
using db::listOrderEntryEventIds;
using db::prependEventOrderEntry;
using db::prependMessageOrderEntry;
using db::putEventOrderMapping;
using db::putEventOrderMappingForEvent;
using db::putMessageOrderMapping;
using db::putOrderEntry;
using db::removeMessageOrderMapping;
using db::removeMessageOrderMappingsNotInOrderEntries;
using db::removeOrderEntryReferences;
using db::removeOrderEntryWithReferences;
using db::removePendingEntriesByTxnId;
using db::removeRelationSourceReferences;
using db::removeTimelineEventReferences;
using db::replaceTimelineEventId;
using db::rewriteRelationSourceReferences;
using db::serializeOrderEntry;
using db::setOrderEntryPrevBatch;
using db::timelineEventIdAtIndex;
using db::timelineIndexForEvent;
using db::timelineRange;
using db::trimOldestOrderEntriesWithReferences;

} // namespace db::storage
