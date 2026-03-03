// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <QDateTime>

#include "Logging.h"
#include "TimelineViewManager.h"

void
RoomlistModel::logRoomPrewarm(const QString &trigger,
                              const QString &roomid,
                              const QString &action,
                              const QString &reason) const
{
    const bool verboseInfo = manager && manager->roomSwitchPerfEnabled();

    if (reason.isEmpty()) {
        if (verboseInfo)
            nhlog::ui()->info("[prewarm][room-list] trigger={} room_id={} action={}",
                              trigger.toStdString(),
                              roomid.toStdString(),
                              action.toStdString());
        else
            nhlog::ui()->debug("[prewarm][room-list] trigger={} room_id={} action={}",
                               trigger.toStdString(),
                               roomid.toStdString(),
                               action.toStdString());
        return;
    }

    if (verboseInfo)
        nhlog::ui()->info("[prewarm][room-list] trigger={} room_id={} action={} reason={}",
                          trigger.toStdString(),
                          roomid.toStdString(),
                          action.toStdString(),
                          reason.toStdString());
    else
        nhlog::ui()->debug("[prewarm][room-list] trigger={} room_id={} action={} reason={}",
                           trigger.toStdString(),
                           roomid.toStdString(),
                           action.toStdString(),
                           reason.toStdString());
}

void
RoomlistModel::scheduleRoomPrewarm(const QString &roomid, const QString &trigger)
{
    if (roomid.isEmpty())
        return;

    if (scheduledPrewarms_.contains(roomid))
        return;

    scheduledPrewarms_.insert(roomid);
    logRoomPrewarm(trigger, roomid, QStringLiteral("scheduled"));
}

void
RoomlistModel::cancelRoomPrewarm(const QString &roomid,
                                 const QString &trigger,
                                 const QString &reason)
{
    if (roomid.isEmpty())
        return;

    if (!scheduledPrewarms_.contains(roomid))
        return;

    scheduledPrewarms_.remove(roomid);
    logRoomPrewarm(trigger, roomid, QStringLiteral("cancel"), reason);
}

void
RoomlistModel::prewarmRoom(const QString &roomid, const QString &trigger)
{
    if (roomid.isEmpty())
        return;

    scheduledPrewarms_.remove(roomid);

    if (models.contains(roomid)) {
        logRoomPrewarm(
          trigger, roomid, QStringLiteral("skip"), QStringLiteral("already_materialized"));
        return;
    }

    if (invites.contains(roomid)) {
        logRoomPrewarm(trigger, roomid, QStringLiteral("skip"), QStringLiteral("invite"));
        return;
    }

    if (previewedRooms.contains(roomid)) {
        logRoomPrewarm(trigger, roomid, QStringLiteral("skip"), QStringLiteral("preview_room"));
        return;
    }

    if (!cachedJoinedRooms_.contains(roomid)) {
        logRoomPrewarm(
          trigger, roomid, QStringLiteral("skip"), QStringLiteral("not_cached_joined_room"));
        return;
    }

    if (!activePrewarms_.isEmpty()) {
        logRoomPrewarm(
          trigger, roomid, QStringLiteral("skip"), QStringLiteral("another_prewarm_active"));
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // Debounce repeated prewarm attempts for the same room while keeping hover-triggered
    // prewarm responsive during quick pointer movement in the room list.
    constexpr qint64 cooldownMs    = 400;
    const qint64 previousAttemptMs = prewarmLastAttemptMs_.value(roomid, 0);
    if (previousAttemptMs > 0 && (now - previousAttemptMs) < cooldownMs) {
        logRoomPrewarm(trigger, roomid, QStringLiteral("skip"), QStringLiteral("cooldown"));
        return;
    }
    prewarmLastAttemptMs_.insert(roomid, now);

    activePrewarms_.insert(roomid);
    logRoomPrewarm(trigger, roomid, QStringLiteral("start"));

    auto roomModel = ensureRoomModel(roomid, true, "prewarm");
    if (roomModel.isNull())
        logRoomPrewarm(
          trigger, roomid, QStringLiteral("skip"), QStringLiteral("materialization_failed"));
    else
        logRoomPrewarm(trigger, roomid, QStringLiteral("done"));

    activePrewarms_.remove(roomid);
}
