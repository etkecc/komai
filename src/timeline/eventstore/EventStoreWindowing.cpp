// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventStore.h"

#include <limits>

#include "logging/Logging.h"

namespace {
bool
isPerfRoomSwitchTracingEnabled()
{
    return qEnvironmentVariableIsSet("KOMAI_ROOM_SWITCH_PERF") ||
           qEnvironmentVariableIsSet("KOMAI_PERF_ROOM_SWITCH");
}
}

bool
EventStore::canExpandWindow() const
{
    return first > dbFirst && last != std::numeric_limits<uint64_t>::max();
}

void
EventStore::applyInitialWindowFromRange(uint64_t rangeFirst, uint64_t rangeLast)
{
    this->dbFirst = rangeFirst;
    this->last    = rangeLast;

    const auto fullSize = static_cast<int>(rangeLast - rangeFirst) + 1;
    if (fullSize > initialWindowSize_) {
        this->first = rangeLast - static_cast<uint64_t>(initialWindowSize_) + 1;
    } else {
        this->first = rangeFirst;
    }

    if (isPerfRoomSwitchTracingEnabled()) {
        nhlog::ui()->info(
          "EventStore[{}]: initial window applied visible={} total={} configured_initial={} "
          "expand_chunk={}",
          room_id_,
          this->size(),
          fullSize,
          initialWindowSize_,
          expandChunkSize_);
    }
}

void
EventStore::expandWindow()
{
    // Expand the virtual window to reveal more cached messages from local storage.
    // Called when the user scrolls up and there are still unrevealed
    // messages in the database (first > dbFirst). Instant, no HTTP.
    if (first <= dbFirst || last == std::numeric_limits<uint64_t>::max()) {
        return;
    }

    auto expandBy = static_cast<uint64_t>(expandChunkSize_);
    uint64_t newFirst;
    if (first - dbFirst > expandBy) {
        newFirst = first - expandBy;
    } else {
        newFirst = dbFirst;
    }

    nhlog::ui()->info("EventStore[{}]: expanding window {} -> {} (+{} msgs, {} left in cache)",
                      room_id_,
                      this->size(),
                      static_cast<int>(last - newFirst) + 1,
                      static_cast<int>(first - newFirst),
                      static_cast<int>(newFirst - dbFirst));

    auto oldFirst = this->first;
    emit beginInsertRows(toExternalIdx(newFirst), toExternalIdx(this->first - 1));
    this->first = newFirst;
    emit endInsertRows();
    emit dataChanged(toExternalIdx(oldFirst), toExternalIdx(oldFirst));
}

void
EventStore::resetWindowToInitial()
{
    // Search/filter flows may expand the virtual window dramatically.
    // Resetting to the configured tail window keeps normal timeline browsing
    // responsive without touching persisted cache state.
    if (last == std::numeric_limits<uint64_t>::max() ||
        dbFirst == std::numeric_limits<uint64_t>::max())
        return;

    const auto fullSize = static_cast<int>(last - dbFirst) + 1;
    uint64_t newFirst   = dbFirst;
    if (fullSize > initialWindowSize_)
        newFirst = last - static_cast<uint64_t>(initialWindowSize_) + 1;

    if (newFirst == first)
        return;

    nhlog::ui()->info("EventStore[{}]: resetting window {} -> {} (configured_initial={})",
                      room_id_,
                      this->size(),
                      static_cast<int>(last - newFirst) + 1,
                      initialWindowSize_);

    emit beginResetModel();
    first = newFirst;
    emit endResetModel();
}

void
EventStore::fetchMore()
{
    if (noMoreMessages) {
        emit fetchedMore();
        return;
    }

    nhlog::ui()->warn(
      "Refusing to paginate the legacy EventStore timeline for room '{}'; this flow is not "
      "migrated to the matrix-sdk backend yet",
      room_id_);
    emit fetchedMore();
}
