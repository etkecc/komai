// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <QPointer>
#include <QTimer>

#include "Logging.h"
#include "MatrixClient.h"
#include "TimelineModel.h"
#include "cache/Cache.h"
#include "events/EventAccessors.h"
#include "utils/Utils.h"

namespace {
constexpr uint64_t kCachedLastMessageScanLimit    = 200;
constexpr int kCachedLastMessageBackfillPageSize  = 200;
constexpr int kCachedLastMessageBackfillMaxEvents = 5000;
constexpr int kCachedLastMessageBackfillMaxRequests =
  kCachedLastMessageBackfillMaxEvents / kCachedLastMessageBackfillPageSize;
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
    if (room_id.isEmpty() || fromToken.empty() ||
        requestsDone >= kCachedLastMessageBackfillMaxRequests) {
        finalizeCachedLastMessageBackfill(room_id);
        return;
    }

    mtx::http::MessagesOpts opts;
    opts.room_id = room_id.toStdString();
    opts.from    = fromToken;
    opts.limit   = kCachedLastMessageBackfillPageSize;

    const QPointer<RoomlistModel> self(this);
    http::client()->messages(
      opts,
      [self, room_id, fromToken, requestsDone](const mtx::responses::Messages &res,
                                               mtx::http::RequestErr err) {
          if (!self)
              return;

          if (!self->cachedLastMessageBackfillInProgress_.contains(room_id))
              return;

          if (err) {
              nhlog::net()->warn(
                "Failed to backfill room list last-message preview for {}: {} - {} - {}",
                room_id.toStdString(),
                mtx::errors::to_string(err->matrix_error.errcode),
                err->matrix_error.error,
                err->parse_error);
              self->finalizeCachedLastMessageBackfill(room_id);
              return;
          }

          const auto roomId = room_id.toStdString();
          if (cache::previousBatchToken(roomId) != fromToken) {
              nhlog::net()->warn(
                "Room list preview backfill token changed for {}, dropping response",
                room_id.toStdString());
              self->finalizeCachedLastMessageBackfill(room_id);
              return;
          }

          const bool noMoreMessages = res.end.empty() || res.end == fromToken;
          if (!res.chunk.empty())
              cache::saveOldMessages(roomId, res);

          self->invalidateCachedLastMessage(room_id);
          self->ensureCachedLastMessage(room_id);

          const auto refreshedDescription = self->cachedLastMessages_.value(room_id);
          if (!refreshedDescription.body.isEmpty() &&
              !isCachedEncryptedPreview(room_id, refreshedDescription)) {
              if (auto idx = self->roomidToIndex(room_id); idx != -1) {
                  emit self->dataChanged(self->index(idx),
                                         self->index(idx),
                                         {Roles::LastMessage, Roles::Time, Roles::Timestamp});
              }
              self->finalizeCachedLastMessageBackfill(room_id);
              return;
          }

          if (noMoreMessages || requestsDone + 1 >= kCachedLastMessageBackfillMaxRequests) {
              self->finalizeCachedLastMessageBackfill(room_id);
              return;
          }

          self->backfillCachedLastMessage(room_id, res.end, requestsDone + 1);
      });
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
