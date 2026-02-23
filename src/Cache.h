// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <map>
#include <optional>

#include <QDateTime>
#include <QString>

#include <mtx/events/collections.hpp>
#include <mtx/events/event_type.hpp>
#include <mtx/events/presence.hpp>
#include <mtx/responses/crypto.hpp>
#include <mtx/responses/messages.hpp>
#include <mtx/responses/sync.hpp>
#include <mtxclient/crypto/types.hpp>
#include <mtxclient/http/errors.hpp>

#include "CacheCryptoStructs.h"
#include "CacheStructs.h"

class QObject;

namespace mtx::responses {
struct Notifications;
struct StateEvents;
}

namespace cache {
void
setNeedsCompactFlag();

void
init(const QString &user_id);

bool
isAvailable() noexcept;
bool
isDatabaseReady();

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

QMap<QString, RoomInfo>
roomInfo(bool withInvites = true);
QHash<QString, RoomInfo>
invites();
std::optional<mtx::events::collections::RoomAccountDataEvents>
getAccountData(mtx::events::EventType type, const std::string &room_id = "");
std::vector<RoomNameAlias>
roomNamesAndAliases();
std::vector<QString>
roomIds();
std::optional<RoomInfo>
invite(std::string_view roomid);
std::optional<MemberInfo>
getInviteMember(const std::string &room_id, const std::string &user_id);
std::vector<std::string>
getParentRoomIds(const std::string &room_id);
std::vector<std::string>
getChildRoomIds(const std::string &room_id);
void
onReadReceiptsChanged(QObject *receiver, std::function<void()> callback);
void
onReadReceiptsChanged(QObject *receiver,
                      std::function<void(const QString &, const std::vector<QString> &)> callback);
void
onRoomReadStatusChanged(QObject *receiver,
                        std::function<void(const std::map<QString, bool> &)> callback);
void
disconnectFromCache(QObject *receiver);
void
onDatabaseReady(QObject *receiver, std::function<void()> callback);
void
onSecretChanged(QObject *receiver, std::function<void(const std::string &)> callback);
void
onVerificationStatusChanged(QObject *receiver, std::function<void(const std::string &)> callback);
void
onSelfVerificationStatusChanged(QObject *receiver, std::function<void()> callback);

//! Retrieve member info from a room.
std::vector<RoomMember>
getMembers(const std::string &room_id, std::size_t startIndex = 0, std::size_t len = 30);
//! Retrive member info from an invite.
std::vector<RoomMember>
getMembersFromInvite(const std::string &room_id, std::size_t start_index = 0, std::size_t len = 30);
size_t
memberCount(const std::string &room_id);

template<typename T>
std::optional<mtx::events::StateEvent<T>>
getStateEvent(const std::string &room_id, std::string_view state_key = "");
template<typename T>
std::vector<mtx::events::StateEvent<T>>
getStateEventsWithType(const std::string &room_id,
                       mtx::events::EventType type = mtx::events::state_content_to_type<T>);

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

//! Retrieve all the user ids from a room.
std::vector<std::string>
roomMembers(const std::string &room_id);

//! Check if the given user has power level greater than than
//! lowest power level of the given events.
bool
hasEnoughPowerLevel(const std::vector<mtx::events::EventType> &eventTypes,
                    const std::string &room_id,
                    const std::string &user_id);

//! Adds a user to the read list for the given event.
//!
//! There should be only one user id present in a receipt list per room.
//! The user id should be removed from any other lists.
using UserReceipts = std::multimap<uint64_t, std::string, std::greater<uint64_t>>;
UserReceipts
readReceipts(const QString &event_id, const QString &room_id);

std::optional<mtx::events::collections::TimelineEvents>
getEvent(const std::string &room_id, std::string_view event_id);
void
storeEvent(const std::string &room_id,
           const std::string &event_id,
           const mtx::events::collections::TimelineEvents &event);
void
replaceEvent(const std::string &room_id,
             const std::string &event_id,
             const mtx::events::collections::TimelineEvents &event);
std::vector<std::string>
relatedEvents(const std::string &room_id, const std::string &event_id);

struct TimelineRange
{
    uint64_t first, last;
};
std::optional<TimelineRange>
getTimelineRange(const std::string &room_id);
std::optional<uint64_t>
getTimelineIndex(const std::string &room_id, std::string_view event_id);
std::optional<std::string>
getTimelineEventId(const std::string &room_id, uint64_t index);
uint64_t
saveOldMessages(const std::string &room_id, const mtx::responses::Messages &res);
void
savePendingMessage(const std::string &room_id,
                   const mtx::events::collections::TimelineEvents &message);
std::vector<std::string>
pendingEvents(const std::string &room_id);
std::optional<mtx::events::collections::TimelineEvents>
firstPendingMessage(const std::string &room_id);
void
removePendingStatus(const std::string &room_id, const std::string &txn_id);
void
clearTimeline(const std::string &room_id);

//! get index of the event in the event db, not representing the visual index
std::optional<uint64_t>
getEventIndex(const std::string &room_id, std::string_view event_id);
std::optional<std::pair<uint64_t, std::string>>
lastInvisibleEventAfter(const std::string &room_id, std::string_view event_id);
std::optional<std::pair<uint64_t, std::string>>
lastVisibleEvent(const std::string &room_id, std::string_view event_id);

RoomInfo
singleRoomInfo(const std::string &room_id);
std::map<QString, RoomInfo>
getRoomInfo(const std::vector<std::string> &rooms);
QString
roomAvatarUrl(const std::string &room_id);

//! Calculates which the read status of a room.
//! Whether all the events in the timeline have been read.
std::string
getFullyReadEventId(const std::string &room_id);
bool
calculateRoomReadStatus(const std::string &room_id);
void
calculateRoomReadStatus();
void
updateLastMessageTimestamp(const std::string &room_id, uint64_t ts);
crypto::Trust
roomVerificationStatus(const std::string &room_id);

void
markSentNotification(const std::string &event_id);
//! Removes an event from the sent notifications.
void
removeReadNotification(const std::string &event_id);
//! Check if we have sent a desktop notification for the given event id.
bool
isNotificationSent(const std::string &event_id);

bool
isMapFullError(const std::exception &e) noexcept;

//! Remove old unused data.
void
deleteOldMessages();
void
deleteOldData() noexcept;
void
storeEventExpirationProgress(const std::string &room,
                             const std::string &expirationSettings,
                             const std::string &event_id);
std::string
loadEventExpirationProgress(const std::string &room, const std::string &expirationSettings);

bool
isRoomEncrypted(const std::string &room_id);
std::optional<mtx::events::state::Encryption>
roomEncryptionSettings(const std::string &room_id);
std::map<std::string, std::optional<UserKeyCache>>
getMembersWithKeys(const std::string &room_id, bool verified_only);

//! Check if a user is a member of the room.
bool
isRoomMember(const std::string &user_id, const std::string &room_id);

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

std::vector<ImagePackInfo>
getImagePacks(const std::string &room_id, std::optional<bool> stickers);

#define KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(Content)                                       \
    extern template std::optional<mtx::events::StateEvent<Content>> cache::getStateEvent<Content>( \
      const std::string &room_id, std::string_view state_key);                                     \
                                                                                                   \
    extern template std::vector<mtx::events::StateEvent<Content>>                                  \
    cache::getStateEventsWithType<Content>(const std::string &room_id,                             \
                                           mtx::events::EventType type);

KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Aliases)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Avatar)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::CanonicalAlias)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Create)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Encryption)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::GuestAccess)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::HistoryVisibility)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::JoinRules)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Member)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Name)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::PinnedEvents)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::PowerLevels)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Tombstone)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::ServerAcl)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Topic)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Widget)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::policy_rule::UserRule)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::policy_rule::RoomRule)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::policy_rule::ServerRule)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::space::Child)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::space::Parent)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::msc2545::ImagePack)

#undef KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD
}
