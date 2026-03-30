// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include "TimelineViewManager.h"
#include "logging/Logging.h"

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

    Q_UNUSED(trigger);
    logRoomPrewarm(QStringLiteral("disabled"),
                   roomid,
                   QStringLiteral("schedule_skip"),
                   QStringLiteral("timeline materialization removed on migration branch"));
}

void
RoomlistModel::cancelRoomPrewarm(const QString &roomid,
                                 const QString &trigger,
                                 const QString &reason)
{
    if (roomid.isEmpty())
        return;

    Q_UNUSED(trigger);
    Q_UNUSED(reason);
}

void
RoomlistModel::prewarmRoom(const QString &roomid, const QString &trigger)
{
    if (roomid.isEmpty())
        return;

    Q_UNUSED(trigger);
    logRoomPrewarm(QStringLiteral("disabled"),
                   roomid,
                   QStringLiteral("skip"),
                   QStringLiteral("timeline materialization removed on migration branch"));
}
