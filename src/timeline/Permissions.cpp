// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Permissions.h"

#include "TimelineModel.h"
#include "cache/Cache.h"
#include "matrix/MatrixClient.h"
#include "utils/Utils.h"

Permissions::Permissions(QString roomId, QObject *parent)
  : QObject(parent)
  , roomId_(std::move(roomId))
{
    invalidate();
}

void
Permissions::invalidate()
{
    pl = cache::getStateEvent<mtx::events::state::PowerLevels>(roomId_.toStdString())
           .value_or(mtx::events::StateEvent<mtx::events::state::PowerLevels>{})
           .content;
}

bool
Permissions::canInvite()
{
    const bool plCheck = pl.user_level(utils::localUser().toStdString()) >= pl.invite;
    return plCheck || isV12Creator();
}

bool
Permissions::canBan()
{
    const bool plCheck = pl.user_level(utils::localUser().toStdString()) >= pl.ban;
    return plCheck || isV12Creator();
}

bool
Permissions::canKick()
{
    const bool plCheck = pl.user_level(utils::localUser().toStdString()) >= pl.kick;
    return plCheck || isV12Creator();
}

bool
Permissions::canRedact()
{
    const bool plCheck = pl.user_level(utils::localUser().toStdString()) >= pl.redact;
    return plCheck || isV12Creator();
}
bool
Permissions::canChange(int eventType)
{
    const bool plCheck = pl.user_level(utils::localUser().toStdString()) >=
                         pl.state_level(to_string(qml_mtx_events::fromRoomEventType(
                           static_cast<qml_mtx_events::EventType>(eventType))));
    return plCheck || isV12Creator();
}
bool
Permissions::canSend(int eventType)
{
    const bool plCheck = pl.user_level(utils::localUser().toStdString()) >=
                         pl.event_level(to_string(qml_mtx_events::fromRoomEventType(
                           static_cast<qml_mtx_events::EventType>(eventType))));
    return plCheck || isV12Creator();
}

int
Permissions::defaultLevel()
{
    return static_cast<int>(pl.users_default);
}
int
Permissions::redactLevel()
{
    return static_cast<int>(pl.redact);
}
int
Permissions::changeLevel(int eventType)
{
    return static_cast<int>(pl.state_level(to_string(
      qml_mtx_events::fromRoomEventType(static_cast<qml_mtx_events::EventType>(eventType)))));
}
int
Permissions::sendLevel(int eventType)
{
    return static_cast<int>(pl.event_level(to_string(
      qml_mtx_events::fromRoomEventType(static_cast<qml_mtx_events::EventType>(eventType)))));
}

bool
Permissions::canPingRoom()
{
    const bool plCheck = pl.user_level(utils::localUser().toStdString()) >=
                         pl.notification_level(mtx::events::state::notification_keys::room);
    return plCheck || isV12Creator();
}

bool
Permissions::isV12Creator()
{
    return cache::isV12Creator(roomId_.toStdString(), utils::localUser().toStdString());
}

#include "moc_Permissions.cpp"
