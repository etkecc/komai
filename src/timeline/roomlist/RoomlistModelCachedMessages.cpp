// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <QPointer>
#include <QTimer>

#include "TimelineModel.h"
#include "cache/Cache.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "utils/Utils.h"

namespace {
constexpr uint64_t kCachedLastMessageScanLimit        = 200;
constexpr int kCachedLastMessageBackfillMaxConcurrent = 1;
constexpr int kCurrentRoomWarmupTargetEvents          = 100;
constexpr int kCurrentRoomWarmupMaxRequests           = 3;
constexpr int kCurrentRoomWarmupStartDelayMs          = 260;
constexpr int kCurrentRoomWarmupStepDelayMs           = 80;
}

DescInfo
RoomlistModel::computeCachedLastMessage(const QString &room_id) const
{
    DescInfo result;

    const auto roomId = room_id.toStdString();
    const auto range  = cache::getTimelineRange(roomId);
    if (!range.has_value())
        return result;

    const QString localUser = utils::localUser();

    uint64_t scanned = 0;
    for (uint64_t idx = range->last;; --idx) {
        const auto eventId = cache::getTimelineEventId(roomId, idx);
        if (!eventId.has_value())
            break;

        const auto event = cache::getEvent(roomId, *eventId);
        if (event.has_value() && mtx::accessors::is_message(*event)) {
            const auto sender = QString::fromStdString(mtx::accessors::sender(*event));
            return utils::getMessageDescription(
              *event, localUser, cache::displayName(room_id, sender));
        }

        if (idx == range->first || ++scanned >= kCachedLastMessageScanLimit)
            break;
    }

    return result;
}

void
RoomlistModel::ensureCachedLastMessage(const QString &room_id)
{
    if (cachedLastMessagesComputed_.contains(room_id))
        return;

    cachedLastMessagesComputed_.insert(room_id);
    cachedLastMessages_.insert(room_id, computeCachedLastMessage(room_id));
}

void
RoomlistModel::scheduleCurrentRoomTimelineWarmup(const QString &roomid)
{
    if (roomid.isEmpty())
        return;

    QTimer::singleShot(kCurrentRoomWarmupStartDelayMs, this, [this, roomid]() {
        warmupCurrentRoomTimeline(roomid);
    });
}

void
RoomlistModel::warmupCurrentRoomTimeline(const QString &roomid, int requestsDone)
{
    if (!currentRoom_ || currentRoom_->roomId() != roomid)
        return;

    if (currentRoom_->isSpace())
        return;

    if (requestsDone >= kCurrentRoomWarmupMaxRequests)
        return;

    if (currentRoom_->rowCount() >= kCurrentRoomWarmupTargetEvents)
        return;

    if (!currentRoom_->canPaginateBack())
        return;

    // If the virtual window can still expand from cached DB entries, the room
    // already has enough messages locally. Skip warmup expansion to avoid
    // unnecessary re-renders — the pagination controller will expand on demand.
    if (currentRoom_->canExpandWindow())
        return;

    if (currentRoom_->paginationInProgress()) {
        connect(
          currentRoom_.data(),
          &TimelineModel::fetchedMore,
          this,
          [this, roomid, requestsDone]() {
              QTimer::singleShot(
                kCurrentRoomWarmupStepDelayMs, this, [this, roomid, requestsDone]() {
                    warmupCurrentRoomTimeline(roomid, requestsDone);
                });
          },
          Qt::SingleShotConnection);
        return;
    }

    connect(
      currentRoom_.data(),
      &TimelineModel::fetchedMore,
      this,
      [this, roomid, requestsDone]() {
          QTimer::singleShot(kCurrentRoomWarmupStepDelayMs, this, [this, roomid, requestsDone]() {
              warmupCurrentRoomTimeline(roomid, requestsDone + 1);
          });
      },
      Qt::SingleShotConnection);
    currentRoom_->requestMore();
}

void
RoomlistModel::maybeBackfillCachedLastMessage(const QString &room_id)
{
    if (room_id.isEmpty() || models.contains(room_id))
        return;

    if (cachedLastMessageBackfillAttempted_.contains(room_id) ||
        cachedLastMessageBackfillQueued_.contains(room_id) ||
        cachedLastMessageBackfillInProgress_.contains(room_id))
        return;

    const auto cachedDescription = cachedLastMessages_.value(room_id);
    if (!cachedDescription.body.isEmpty())
        return;

    // One backfill campaign per room list entry to avoid repeated fetches on re-render.
    cachedLastMessageBackfillAttempted_.insert(room_id);
    cachedLastMessageBackfillQueued_.insert(room_id);
    startQueuedCachedLastMessageBackfills();
}

void
RoomlistModel::startQueuedCachedLastMessageBackfills()
{
    while (cachedLastMessageBackfillInProgress_.size() < kCachedLastMessageBackfillMaxConcurrent &&
           !cachedLastMessageBackfillQueued_.isEmpty()) {
        const auto room_id = *cachedLastMessageBackfillQueued_.constBegin();
        cachedLastMessageBackfillQueued_.remove(room_id);

        const auto roomId    = room_id.toStdString();
        const auto fromToken = cache::previousBatchToken(roomId);
        if (roomids.empty() || roomidToIndex(room_id) == -1 || fromToken.empty())
            continue;

        cachedLastMessageBackfillInProgress_.insert(room_id);
        backfillCachedLastMessage(room_id, fromToken, 0);
    }
}

void
RoomlistModel::backfillCachedLastMessage(const QString &room_id,
                                         const std::string &fromToken,
                                         int requestsDone)
{
    Q_UNUSED(fromToken);
    Q_UNUSED(requestsDone);

    nhlog::ui()->debug("Skipping legacy room-list last-message backfill for '{}'; old "
                       "/messages pagination path has been removed",
                       room_id.toStdString());
    finalizeCachedLastMessageBackfill(room_id);
}

void
RoomlistModel::finalizeCachedLastMessageBackfill(const QString &room_id)
{
    cachedLastMessageBackfillQueued_.remove(room_id);
    cachedLastMessageBackfillInProgress_.remove(room_id);
    startQueuedCachedLastMessageBackfills();
}

void
RoomlistModel::invalidateCachedLastMessage(const QString &room_id)
{
    cachedLastMessagesComputed_.remove(room_id);
    cachedLastMessages_.remove(room_id);
}
