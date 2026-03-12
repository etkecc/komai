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
  : AbstractPermissions(parent)
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
    create = cache::getStateEvent<mtx::events::state::Create>(roomId_.toStdString())
               .value_or(mtx::events::StateEvent<mtx::events::state::Create>{});
}

bool
Permissions::canInvite()
{
    return pl.user_level(utils::localUser().toStdString(), create) >= pl.invite;
}

bool
Permissions::canBan()
{
    return pl.user_level(utils::localUser().toStdString(), create) >= pl.ban;
}

bool
Permissions::canKick()
{
    return pl.user_level(utils::localUser().toStdString(), create) >= pl.kick;
}

bool
Permissions::canRedact()
{
    return pl.user_level(utils::localUser().toStdString(), create) >= pl.redact;
}
bool
Permissions::canChange(int eventType)
{
    return pl.user_level(utils::localUser().toStdString(), create) >=
           pl.state_level(to_string(
             qml_mtx_events::fromRoomEventType(static_cast<qml_mtx_events::EventType>(eventType))));
}
bool
Permissions::canSend(int eventType)
{
    return pl.user_level(utils::localUser().toStdString(), create) >=
           pl.event_level(to_string(
             qml_mtx_events::fromRoomEventType(static_cast<qml_mtx_events::EventType>(eventType))));
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
    return pl.user_level(utils::localUser().toStdString(), create) >=
           pl.notification_level(mtx::events::state::notification_keys::room);
}

#include "moc_Permissions.cpp"
