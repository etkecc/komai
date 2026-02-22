// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "CacheApiWrappers.h"
#include "Cache_p.h"
#include "Logging.h"

#include <utility>
#include <vector>

namespace cache {

bool
isRoomMember(const std::string &user_id, const std::string &room_id)
{
    return cacheInstance()->isRoomMember(user_id, room_id);
}

//
// Outbound Megolm Sessions
//
void
saveOutboundMegolmSession(const std::string &room_id,
                          const GroupSessionData &data,
                          mtx::crypto::OutboundGroupSessionPtr &session)
{
    cacheInstance()->saveOutboundMegolmSession(room_id, data, session);
}
OutboundGroupSessionDataRef
getOutboundMegolmSession(const std::string &room_id)
{
    return cacheInstance()->getOutboundMegolmSession(room_id);
}
bool
outboundMegolmSessionExists(const std::string &room_id) noexcept
{
    return cacheInstance()->outboundMegolmSessionExists(room_id);
}
void
updateOutboundMegolmSession(const std::string &room_id,
                            const GroupSessionData &data,
                            mtx::crypto::OutboundGroupSessionPtr &session)
{
    cacheInstance()->updateOutboundMegolmSession(room_id, data, session);
}
void
dropOutboundMegolmSession(const std::string &room_id)
{
    cacheInstance()->dropOutboundMegolmSession(room_id);
}

void
importSessionKeys(const mtx::crypto::ExportedSessionKeys &keys)
{
    cacheInstance()->importSessionKeys(keys);
}
mtx::crypto::ExportedSessionKeys
exportSessionKeys()
{
    return cacheInstance()->exportSessionKeys();
}

//
// Inbound Megolm Sessions
//
void
saveInboundMegolmSession(const MegolmSessionIndex &index,
                         mtx::crypto::InboundGroupSessionPtr session,
                         const GroupSessionData &data)
{
    cacheInstance()->saveInboundMegolmSession(index, std::move(session), data);
}
mtx::crypto::InboundGroupSessionPtr
getInboundMegolmSession(const MegolmSessionIndex &index)
{
    return cacheInstance()->getInboundMegolmSession(index);
}
bool
inboundMegolmSessionExists(const MegolmSessionIndex &index)
{
    return cacheInstance()->inboundMegolmSessionExists(index);
}
std::optional<GroupSessionData>
getMegolmSessionData(const MegolmSessionIndex &index)
{
    return cacheInstance()->getMegolmSessionData(index);
}

//
// Olm Sessions
//
void
saveOlmSession(const std::string &curve25519,
               mtx::crypto::OlmSessionPtr session,
               uint64_t timestamp)
{
    cacheInstance()->saveOlmSession(curve25519, std::move(session), timestamp);
}
void
saveOlmSessions(std::vector<std::pair<std::string, mtx::crypto::OlmSessionPtr>> sessions,
                uint64_t timestamp)
{
    cacheInstance()->saveOlmSessions(std::move(sessions), timestamp);
}
std::vector<std::string>
getOlmSessions(const std::string &curve25519)
{
    return cacheInstance()->getOlmSessions(curve25519);
}
std::optional<mtx::crypto::OlmSessionPtr>
getOlmSession(const std::string &curve25519, const std::string &session_id)
{
    return cacheInstance()->getOlmSession(curve25519, session_id);
}
std::optional<mtx::crypto::OlmSessionPtr>
getLatestOlmSession(const std::string &curve25519)
{
    return cacheInstance()->getLatestOlmSession(curve25519);
}

void
saveOlmAccount(const std::string &pickled)
{
    cacheInstance()->saveOlmAccount(pickled);
}
std::string
restoreOlmAccount()
{
    return cacheInstance()->restoreOlmAccount();
}
std::string
pickleSecret()
{
    return cacheInstance()->pickleSecret();
}
std::string
createPickleSecret()
{
    return cacheInstance()->createPickleSecret();
}
void
saveBackupVersion(const OnlineBackupVersion &data)
{
    cacheInstance()->saveBackupVersion(data);
}
void
deleteBackupVersion()
{
    cacheInstance()->deleteBackupVersion();
}
std::optional<OnlineBackupVersion>
backupVersion()
{
    if (!cacheInstance())
        return std::nullopt;
    try {
        return cacheInstance()->backupVersion();
    } catch (const std::exception &e) {
        if (const auto logger = nhlog::db()) {
            logger->warn("Unable to read backup version: {}", e.what());
        }
        return std::nullopt;
    } catch (...) {
        if (const auto logger = nhlog::db()) {
            logger->warn("Unable to read backup version: unexpected exception");
        }
        return std::nullopt;
    }
}

void
storeSecret(std::string_view name, const std::string &secret)
{
    cacheInstance()->storeSecret(name, secret);
}
std::optional<std::string>
secret(std::string_view name)
{
    if (!cacheInstance())
        return std::nullopt;
    try {
        return cacheInstance()->secret(name);
    } catch (const std::exception &e) {
        if (const auto logger = nhlog::db()) {
            logger->warn("Unable to read secret '{}': {}", name, e.what());
        }
        return std::nullopt;
    } catch (...) {
        if (const auto logger = nhlog::db()) {
            logger->warn("Unable to read secret '{}': unexpected exception", name);
        }
        return std::nullopt;
    }
}

std::vector<ImagePackInfo>
getImagePacks(const std::string &room_id, std::optional<bool> stickers)
{
    return cacheInstance()->getImagePacks(room_id, stickers);
}

} // namespace cache
