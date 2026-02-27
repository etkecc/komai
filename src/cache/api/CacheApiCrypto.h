// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "cache/api/CacheApiTypes.h"

namespace cache {
//
// Outbound Megolm Sessions
//
void
saveOutboundMegolmSession(const std::string &room_id,
                          const GroupSessionData &data,
                          mtx::crypto::OutboundGroupSessionPtr &session);
OutboundGroupSessionDataRef
getOutboundMegolmSession(const std::string &room_id);
bool
outboundMegolmSessionExists(const std::string &room_id) noexcept;
void
updateOutboundMegolmSession(const std::string &room_id,
                            const GroupSessionData &data,
                            mtx::crypto::OutboundGroupSessionPtr &session);
void
dropOutboundMegolmSession(const std::string &room_id);

void
importSessionKeys(const mtx::crypto::ExportedSessionKeys &keys);
mtx::crypto::ExportedSessionKeys
exportSessionKeys();

//
// Inbound Megolm Sessions
//
void
saveInboundMegolmSession(const MegolmSessionIndex &index,
                         mtx::crypto::InboundGroupSessionPtr session,
                         const GroupSessionData &data);
mtx::crypto::InboundGroupSessionPtr
getInboundMegolmSession(const MegolmSessionIndex &index);
bool
inboundMegolmSessionExists(const MegolmSessionIndex &index);
std::optional<GroupSessionData>
getMegolmSessionData(const MegolmSessionIndex &index);

//
// Olm Sessions
//
void
saveOlmSession(const std::string &curve25519,
               mtx::crypto::OlmSessionPtr session,
               uint64_t timestamp);
void
saveOlmSessions(std::vector<std::pair<std::string, mtx::crypto::OlmSessionPtr>> sessions,
                uint64_t timestamp);
std::vector<std::string>
getOlmSessions(const std::string &curve25519);
std::optional<mtx::crypto::OlmSessionPtr>
getOlmSession(const std::string &curve25519, const std::string &session_id);
std::optional<mtx::crypto::OlmSessionPtr>
getLatestOlmSession(const std::string &curve25519);

void
saveOlmAccount(const std::string &pickled);

std::string
restoreOlmAccount();
std::string
pickleSecret();
std::string
createPickleSecret();
void
saveBackupVersion(const OnlineBackupVersion &data);
void
deleteBackupVersion();
std::optional<OnlineBackupVersion>
backupVersion();

void
storeSecret(std::string_view name, const std::string &secret);
std::optional<std::string>
secret(std::string_view name);
} // namespace cache
