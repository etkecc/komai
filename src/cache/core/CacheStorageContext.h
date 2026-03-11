// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include "db/storage/Core.h"

struct CacheDb
{
    std::unique_ptr<db::Database> storage = db::createDefaultDatabase();
    db::Store syncState;
    db::Store rooms;
    db::Store spacesChildren, spacesParents;
    db::Store invites;
    db::Store sharedRoomPlain;
    db::Store sharedRoomOrdered;
    db::Store sharedRoomDupsort;
    db::Store readReceipts;
    db::Store notifications;
    db::Store presence;

    db::Store inboundMegolmSessions;
    db::Store outboundMegolmSessions;
    db::Store megolmSessionsData;
    db::Store olmSessions;

    db::Store encryptedRooms_;

    db::Store eventExpiryBgJob_;
};
