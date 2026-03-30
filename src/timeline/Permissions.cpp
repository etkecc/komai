// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Permissions.h"

#include "logging/Logging.h"

Permissions::Permissions(QString roomId, QObject *parent)
  : AbstractPermissions(parent)
  , roomId_(std::move(roomId))
{
    invalidate();
}

void
Permissions::invalidate()
{
    nhlog::ui()->warn("Using conservative default room permissions for '{}' until matrix-sdk "
                      "power-level fetch is migrated",
                      roomId_.toStdString());
    pl     = {};
    create = {};
}

bool
Permissions::canInvite()
{
    return false;
}

bool
Permissions::canBan()
{
    return false;
}

bool
Permissions::canKick()
{
    return false;
}

bool
Permissions::canRedact()
{
    return false;
}

bool
Permissions::canChange(int eventType)
{
    Q_UNUSED(eventType);
    return false;
}

bool
Permissions::canSend(int eventType)
{
    Q_UNUSED(eventType);
    return true;
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
    Q_UNUSED(eventType);
    return static_cast<int>(pl.state_default);
}

int
Permissions::sendLevel(int eventType)
{
    Q_UNUSED(eventType);
    return static_cast<int>(pl.events_default);
}

bool
Permissions::canPingRoom()
{
    return true;
}

#include "moc_Permissions.cpp"
