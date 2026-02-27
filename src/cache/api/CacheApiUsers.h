// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "cache/api/CacheApiTypes.h"

namespace cache {
std::string
displayName(const std::string &room_id, const std::string &user_id);
QString
displayName(const QString &room_id, const QString &user_id);
QString
avatarUrl(const QString &room_id, const QString &user_id);

// presence
mtx::events::presence::Presence
presence(const std::string &user_id);

// user cache stores user keys
std::optional<UserKeyCache>
userKeys(const std::string &user_id);
std::map<std::string, RoomInfo>
getCommonRooms(const std::string &user_id);
void
markUserKeysOutOfDate(const std::vector<std::string> &user_ids);
void
queryKeys(const std::string &user_id,
          std::function<void(const UserKeyCache &, const std::optional<mtx::http::ClientError> &)>
            callback);
void
updateUserKeys(const std::string &sync_token, const mtx::responses::QueryKeys &keyQuery);

// device & user verification cache
std::optional<VerificationStatus>
verificationStatus(const std::string &user_id);
void
markDeviceVerified(const std::string &user_id, const std::string &device);
void
markDeviceUnverified(const std::string &user_id, const std::string &device);

std::vector<std::string>
joinedRooms();
} // namespace cache
