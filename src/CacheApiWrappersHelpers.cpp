// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "CacheApiWrappers.h"
#include "Cache_p.h"

#include <memory>
#include <utility>
#include <vector>

#include <QObject>

namespace {
std::unique_ptr<Cache> instance_ = nullptr;
}

namespace cache {

std::unique_ptr<Cache> &
cacheInstance()
{
    return instance_;
}

void
init(const QString &user_id)
{
    instance_ = std::make_unique<Cache>(user_id);
}

bool
isAvailable() noexcept
{
    return instance_ != nullptr;
}

bool
isDatabaseReady()
{
    return instance_ && instance_->isDatabaseReady();
}

bool
isMapFullError(const std::exception &e) noexcept
{
    return instance_ && instance_->isMapFullError(e);
}

std::string
displayName(const std::string &room_id, const std::string &user_id)
{
    return instance_->displayName(room_id, user_id);
}

QString
displayName(const QString &room_id, const QString &user_id)
{
    return instance_->displayName(room_id, user_id);
}
QString
avatarUrl(const QString &room_id, const QString &user_id)
{
    return instance_->avatarUrl(room_id, user_id);
}

mtx::events::presence::Presence
presence(const std::string &user_id)
{
    if (!instance_)
        return {};
    return instance_->presence(user_id);
}

// user cache stores user keys
std::optional<UserKeyCache>
userKeys(const std::string &user_id)
{
    return instance_->userKeys(user_id);
}

std::map<std::string, RoomInfo>
getCommonRooms(const std::string &user_id)
{
    return instance_->getCommonRooms(user_id);
}

void
markUserKeysOutOfDate(const std::vector<std::string> &user_ids)
{
    instance_->markUserKeysOutOfDate(user_ids);
}

void
queryKeys(
  const std::string &user_id,
  std::function<void(const UserKeyCache &, const std::optional<mtx::http::ClientError> &)> callback)
{
    instance_->query_keys(user_id, std::move(callback));
}

void
updateUserKeys(const std::string &sync_token, const mtx::responses::QueryKeys &keyQuery)
{
    instance_->updateUserKeys(sync_token, keyQuery);
}

// device & user verification cache
std::optional<VerificationStatus>
verificationStatus(const std::string &user_id)
{
    return instance_->verificationStatus(user_id);
}

void
markDeviceVerified(const std::string &user_id, const std::string &device)
{
    instance_->markDeviceVerified(user_id, device);
}

void
markDeviceUnverified(const std::string &user_id, const std::string &device)
{
    instance_->markDeviceUnverified(user_id, device);
}

std::vector<std::string>
joinedRooms()
{
    return instance_->joinedRooms();
}

QMap<QString, RoomInfo>
roomInfo(bool withInvites)
{
    return instance_->roomInfo(withInvites);
}
QHash<QString, RoomInfo>
invites()
{
    return instance_->invites();
}

std::optional<mtx::events::collections::RoomAccountDataEvents>
getAccountData(mtx::events::EventType type, const std::string &room_id)
{
    return instance_->getAccountData(type, room_id);
}

std::vector<RoomNameAlias>
roomNamesAndAliases()
{
    return instance_->roomNamesAndAliases();
}

std::optional<RoomInfo>
invite(std::string_view roomid)
{
    return instance_->invite(roomid);
}

std::optional<MemberInfo>
getInviteMember(const std::string &room_id, const std::string &user_id)
{
    return instance_->getInviteMember(room_id, user_id);
}

std::vector<std::string>
getParentRoomIds(const std::string &room_id)
{
    return instance_->getParentRoomIds(room_id);
}

std::vector<std::string>
getChildRoomIds(const std::string &room_id)
{
    return instance_->getChildRoomIds(room_id);
}

} // namespace cache
