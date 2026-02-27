// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "cache/api/CacheApiTypes.h"

namespace cache {
void
setNeedsCompactFlag();

void
init(const QString &user_id);

bool
isAvailable() noexcept;
bool
isDatabaseReady();

bool
isInitialized();

std::string
nextBatchToken();
std::string
previousBatchToken(const std::string &room_id);

void
deleteData();

void
removeInvite(const std::string &room_id);
void
removeRoom(const std::string &roomid);
void
removeRoom(const QString &roomid);
void
setup();
void
saveState(const mtx::responses::Sync &res);
void
updateState(const std::string &room, const mtx::responses::StateEvents &state, bool wipe = false);

//! returns if the format is current, older or newer
cache::CacheVersion
formatVersion();
//! set the format version to the current version
void
setCurrentFormat();
//! migrates db to the current format
bool
runMigrations();

bool
isMapFullError(const std::exception &e) noexcept;

//! Remove old unused data.
void
deleteOldMessages();
void
deleteOldData() noexcept;
} // namespace cache
