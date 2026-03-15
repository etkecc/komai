// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <QDateTime>
#ifdef Q_OS_LINUX
#include <malloc.h>
#endif

#include "TimelineModel.h"
#include "logging/Logging.h"

namespace {
constexpr int kDefaultLruCapacity      = 8;
constexpr int kDefaultLruGracePeriodMs = 15000;

int
parsePositiveIntEnv(const char *name, int fallbackValue)
{
    const auto value = qEnvironmentVariable(name).trimmed();
    if (value.isEmpty())
        return fallbackValue;

    bool ok          = false;
    const auto asInt = value.toInt(&ok);
    if (!ok || asInt < 1)
        return fallbackValue;

    return asInt;
}
}

void
RoomlistModel::initLruEviction()
{
    lruCapacity_      = parsePositiveIntEnv("KOMAI_LRU_CAPACITY", kDefaultLruCapacity);
    lruGracePeriodMs_ = parsePositiveIntEnv("KOMAI_LRU_GRACE_PERIOD_MS", kDefaultLruGracePeriodMs);

    nhlog::ui()->info(
      "[lru] initialized capacity={} grace_period_ms={}", lruCapacity_, lruGracePeriodMs_);
}

void
RoomlistModel::touchRoomLru(const QString &room_id)
{
    if (room_id.isEmpty())
        return;

    roomLruAccessMs_[room_id] = QDateTime::currentMSecsSinceEpoch();
}

void
RoomlistModel::scheduleLruEviction()
{
    if (!lruEvictionTimer_) {
        lruEvictionTimer_ = new QTimer(this);
        lruEvictionTimer_->setSingleShot(true);
        connect(lruEvictionTimer_, &QTimer::timeout, this, &RoomlistModel::performLruEviction);
    }

    lruEvictionTimer_->start(lruGracePeriodMs_);
}

void
RoomlistModel::performLruEviction()
{
    const QString currentRoomId = currentRoom_ ? currentRoom_->roomId() : QString();
    const qint64 now            = QDateTime::currentMSecsSinceEpoch();

    // Collect eviction candidates: all materialized rooms except current and pending.
    struct Candidate
    {
        QString roomId;
        qint64 lastAccess;
    };
    QVector<Candidate> candidates;
    candidates.reserve(models.size());

    for (auto it = models.constBegin(); it != models.constEnd(); ++it) {
        const auto &roomId = it.key();
        if (roomId == currentRoomId || roomId == pendingCurrentRoomId_)
            continue;

        candidates.append({roomId, roomLruAccessMs_.value(roomId, 0)});
    }

    // Sort by last access ascending — oldest (and never-accessed prewarms at 0) first.
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        return a.lastAccess < b.lastAccess;
    });

    int evicted = 0;
    for (const auto &candidate : candidates) {
        // Over capacity: always evict the oldest.
        const bool overCapacity = (models.size() > lruCapacity_);
        // Under capacity: evict only if idle longer than grace period.
        const bool idleTooLong =
          candidate.lastAccess > 0 && (now - candidate.lastAccess) >= lruGracePeriodMs_;
        // Never-accessed (prewarmed but never opened): always eligible.
        const bool neverAccessed = (candidate.lastAccess == 0);

        if (overCapacity || idleTooLong || neverAccessed) {
            evictRoomModel(candidate.roomId);
            ++evicted;
        }
    }

    if (evicted > 0) {
#ifdef Q_OS_LINUX
        // glibc keeps freed memory in arenas; nudge it to return pages to the OS.
        malloc_trim(0);
        nhlog::ui()->info(
          "[lru] eviction done: evicted={} models_remaining={} (malloc_trim called)",
          evicted,
          models.size());
#else
        nhlog::ui()->info(
          "[lru] eviction done: evicted={} models_remaining={}", evicted, models.size());
#endif
    }
}

void
RoomlistModel::evictRoomModel(const QString &room_id)
{
    if (!models.contains(room_id))
        return;

    nhlog::ui()->info(
      "[lru] evicting room_id={} models_count={}", room_id.toStdString(), models.size());

    models.remove(room_id);
    roomLruAccessMs_.remove(room_id);
    emitRoomRowUpdate(room_id);
}
