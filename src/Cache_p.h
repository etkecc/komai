// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <exception>
#include <optional>

#include <QDateTime>
#include <QString>

#include <mtx/events/collections.hpp>
#include <mtx/responses/notifications.hpp>
#include <mtx/responses/sync.hpp>
#include <mtxclient/crypto/types.hpp>
#include <mtxclient/http/errors.hpp>

#include "CacheCryptoStructs.h"
#include "CacheStructs.h"
#include "db/Flags.h"

namespace mtx::responses {
struct Messages;
struct StateEvents;
}

namespace db {
class Backend;
class Txn;
class Dbi;
enum class DupsortComparator;
}

struct CacheDb;

class Cache final : public QObject
{
    Q_OBJECT

public:
    Cache(const QString &userId, QObject *parent = nullptr);
    ~Cache() noexcept;

    std::string displayName(const std::string &room_id, const std::string &user_id);
    QString displayName(const QString &room_id, const QString &user_id);
    QString avatarUrl(const QString &room_id, const QString &user_id);

    // presence
    mtx::events::presence::Presence presence(const std::string &user_id);

    // user cache stores user keys
    std::map<std::string, std::optional<UserKeyCache>>
    getMembersWithKeys(const std::string &room_id, bool verified_only);
    void updateUserKeys(const std::string &sync_token, const mtx::responses::QueryKeys &keyQuery);
    void markUserKeysOutOfDate(const std::vector<std::string> &user_ids);
    void markUserKeysOutOfDate(db::Txn &txn,
                               db::Dbi &db,
                               const std::vector<std::string> &user_ids,
                               const std::string &sync_token);
    void query_keys(
      const std::string &user_id,
      std::function<void(const UserKeyCache &, const std::optional<mtx::http::ClientError> &)> cb);

    // device & user verification cache
    std::optional<UserKeyCache> userKeys(const std::string &user_id);
    VerificationStatus verificationStatus(const std::string &user_id);
    void markDeviceVerified(const std::string &user_id, const std::string &device);
    void markDeviceUnverified(const std::string &user_id, const std::string &device);
    crypto::Trust roomVerificationStatus(const std::string &room_id);

    std::vector<std::string> joinedRooms();
    std::map<std::string, RoomInfo> getCommonRooms(const std::string &user_id);

    QMap<QString, RoomInfo> roomInfo(bool withInvites = true);
    QHash<QString, RoomInfo> invites();
    std::optional<RoomInfo> invite(std::string_view roomid);
    QMap<QString, std::optional<RoomInfo>> spaces();

    //! Calculate & return the name of the room.
    QString getRoomName(db::Txn &txn, db::Dbi &statesdb, db::Dbi &membersdb);
    //! Get room join rules
    mtx::events::state::JoinRule getRoomJoinRule(db::Txn &txn, db::Dbi &statesdb);
    bool getRoomGuestAccess(db::Txn &txn, db::Dbi &statesdb);
    //! Retrieve the topic of the room if any.
    QString getRoomTopic(db::Txn &txn, db::Dbi &statesdb);
    //! Retrieve the room avatar's url if any.
    QString getRoomAvatarUrl(db::Txn &txn, db::Dbi &statesdb, db::Dbi &membersdb);
    //! Retrieve the version of the room if any.
    QString getRoomVersion(db::Txn &txn, db::Dbi &statesdb);
    //! Retrieve if the room is a space
    bool getRoomIsSpace(db::Txn &txn, db::Dbi &statesdb);
    //! Retrieve if the room is tombstoned (closed or replaced by a different room)
    bool getRoomIsTombstoned(db::Txn &txn, db::Dbi &statesdb);

    // for the event expiry background job
    void storeEventExpirationProgress(const std::string &room,
                                      const std::string &expirationSettings,
                                      const std::string &stopMarker);
    std::string
    loadEventExpirationProgress(const std::string &room, const std::string &expirationSettings);

    //! Get a specific state event
    template<typename T>
    std::optional<mtx::events::StateEvent<T>>
    getStateEvent(const std::string &room_id, std::string_view state_key = "");
    template<typename T>
    std::vector<mtx::events::StateEvent<T>>
    getStateEventsWithType(const std::string &room_id,
                           mtx::events::EventType type = mtx::events::state_content_to_type<T>);

    //! retrieve a specific event from account data
    //! pass empty room_id for global account data
    std::optional<mtx::events::collections::RoomAccountDataEvents>
    getAccountData(mtx::events::EventType type, const std::string &room_id = "");

    //! Retrieve member info from a room.
    std::vector<RoomMember>
    getMembers(const std::string &room_id, std::size_t startIndex = 0, std::size_t len = 30);

    std::vector<RoomMember> getMembersFromInvite(const std::string &room_id,
                                                 std::size_t startIndex = 0,
                                                 std::size_t len        = 30);
    size_t memberCount(const std::string &room_id);

    void updateState(const std::string &room,
                     const mtx::responses::StateEvents &state,
                     bool wipe = false);
    void saveState(const mtx::responses::Sync &res);
    bool isInitialized();
    bool isDatabaseReady() { return databaseReady_ && isInitialized(); }
    bool isMapFullError(const std::exception &e) const noexcept;

    std::string nextBatchToken();

    void deleteData();

    void removeInvite(db::Txn &txn, const std::string &room_id);
    void removeInvite(const std::string &room_id);
    void removeRoom(db::Txn &txn, const std::string &roomid);
    void removeRoom(const std::string &roomid);
    void setup();

    cache::CacheVersion formatVersion();
    void setCurrentFormat();
    bool runMigrations();

    std::vector<QString> roomIds();

    //! Retrieve all the user ids from a room.
    std::vector<std::string> roomMembers(const std::string &room_id);

    //! Check if the given user has power leve greater than than
    //! lowest power level of the given events.
    bool hasEnoughPowerLevel(const std::vector<mtx::events::EventType> &eventTypes,
                             const std::string &room_id,
                             const std::string &user_id);

    //! Adds a user to the read list for the given event.
    //!
    //! There should be only one user id present in a receipt list per room.
    //! The user id should be removed from any other lists.
    using Receipts = std::map<std::string, std::map<std::string, uint64_t>>;
    void updateReadReceipt(db::Txn &txn, const std::string &room_id, const Receipts &receipts);

    //! Retrieve all the read receipts for the given event id and room.
    //!
    //! Returns a map of user ids and the time of the read receipt in milliseconds.
    using UserReceipts = std::multimap<uint64_t, std::string, std::greater<uint64_t>>;
    UserReceipts readReceipts(const QString &event_id, const QString &room_id);

    RoomInfo singleRoomInfo(const std::string &room_id);
    std::map<QString, RoomInfo> getRoomInfo(const std::vector<std::string> &rooms);

    std::vector<RoomNameAlias> roomNamesAndAliases();

    void updateLastMessageTimestamp(const std::string &room_id, uint64_t ts);

    //! Calculates which the read status of a room.
    //! Whether all the events in the timeline have been read.
    std::string getFullyReadEventId(const std::string &room_id);
    bool calculateRoomReadStatus(const std::string &room_id);
    void calculateRoomReadStatus();

    void markSentNotification(const std::string &event_id);
    //! Removes an event from the sent notifications.
    void removeReadNotification(const std::string &event_id);
    //! Check if we have sent a desktop notification for the given event id.
    bool isNotificationSent(const std::string &event_id);

    std::optional<mtx::events::collections::TimelineEvents>
    getEvent(const std::string &room_id, std::string_view event_id);
    void storeEvent(const std::string &room_id,
                    const std::string &event_id,
                    const mtx::events::collections::TimelineEvents &event);
    void replaceEvent(const std::string &room_id,
                      const std::string &event_id,
                      const mtx::events::collections::TimelineEvents &event);
    std::vector<std::string> relatedEvents(const std::string &room_id, const std::string &event_id);

    struct TimelineRange
    {
        uint64_t first, last;
    };
    std::optional<TimelineRange> getTimelineRange(const std::string &room_id);
    std::optional<uint64_t> getTimelineIndex(const std::string &room_id, std::string_view event_id);
    std::optional<uint64_t> getEventIndex(const std::string &room_id, std::string_view event_id);
    std::optional<std::pair<uint64_t, std::string>>
    lastInvisibleEventAfter(const std::string &room_id, std::string_view event_id);
    std::optional<std::pair<uint64_t, std::string>>
    lastVisibleEvent(const std::string &room_id, std::string_view event_id);
    std::optional<std::string> getTimelineEventId(const std::string &room_id, uint64_t index);

    std::string previousBatchToken(const std::string &room_id);
    uint64_t saveOldMessages(const std::string &room_id, const mtx::responses::Messages &res);
    void savePendingMessage(const std::string &room_id,
                            const mtx::events::collections::TimelineEvents &message);
    std::vector<std::string> pendingEvents(const std::string &room_id);
    std::optional<mtx::events::collections::TimelineEvents>
    firstPendingMessage(const std::string &room_id);
    void removePendingStatus(const std::string &room_id, const std::string &txn_id);

    //! clear timeline keeping only the latest batch
    void clearTimeline(const std::string &room_id);

    //! Remove old unused data.
    void deleteOldMessages();
    void deleteOldData() noexcept;
    //! Retrieve all saved room ids.
    std::vector<std::string> getRoomIds(db::Txn &txn);
    std::vector<std::string> getParentRoomIds(const std::string &room_id);
    std::vector<std::string> getChildRoomIds(const std::string &room_id);

    std::vector<ImagePackInfo>
    getImagePacks(const std::string &room_id, std::optional<bool> stickers);

    //! Mark a room that uses e2e encryption.
    void setEncryptedRoom(db::Txn &txn, const std::string &room_id);
    bool isRoomEncrypted(const std::string &room_id);
    std::optional<mtx::events::state::Encryption>
    roomEncryptionSettings(const std::string &room_id);

    //! Check if a user is a member of the room.
    bool isRoomMember(const std::string &user_id, const std::string &room_id);
    std::optional<MemberInfo>
    getInviteMember(const std::string &room_id, const std::string &user_id);

    //
    // Outbound Megolm Sessions
    //
    void saveOutboundMegolmSession(const std::string &room_id,
                                   const GroupSessionData &data,
                                   mtx::crypto::OutboundGroupSessionPtr &session);
    OutboundGroupSessionDataRef getOutboundMegolmSession(const std::string &room_id);
    bool outboundMegolmSessionExists(const std::string &room_id) noexcept;
    void updateOutboundMegolmSession(const std::string &room_id,
                                     const GroupSessionData &data,
                                     mtx::crypto::OutboundGroupSessionPtr &session);
    void dropOutboundMegolmSession(const std::string &room_id);

    void importSessionKeys(const mtx::crypto::ExportedSessionKeys &keys);
    mtx::crypto::ExportedSessionKeys exportSessionKeys();

    //
    // Inbound Megolm Sessions
    //
    void saveInboundMegolmSession(const MegolmSessionIndex &index,
                                  mtx::crypto::InboundGroupSessionPtr session,
                                  const GroupSessionData &data);
    mtx::crypto::InboundGroupSessionPtr getInboundMegolmSession(const MegolmSessionIndex &index);
    bool inboundMegolmSessionExists(const MegolmSessionIndex &index);
    std::optional<GroupSessionData> getMegolmSessionData(const MegolmSessionIndex &index);

    //
    // Olm Sessions
    //
    void saveOlmSession(const std::string &curve25519,
                        mtx::crypto::OlmSessionPtr session,
                        uint64_t timestamp);
    void saveOlmSessions(std::vector<std::pair<std::string, mtx::crypto::OlmSessionPtr>> sessions,
                         uint64_t timestamp);
    std::vector<std::string> getOlmSessions(const std::string &curve25519);
    std::optional<mtx::crypto::OlmSessionPtr>
    getOlmSession(const std::string &curve25519, const std::string &session_id);
    std::optional<mtx::crypto::OlmSessionPtr> getLatestOlmSession(const std::string &curve25519);

    void saveOlmAccount(const std::string &pickled);
    std::string restoreOlmAccount();

    void saveBackupVersion(const OnlineBackupVersion &data);
    void deleteBackupVersion();
    std::optional<OnlineBackupVersion> backupVersion();

    void storeSecret(std::string_view name, const std::string &secret, bool internal = false);
    void deleteSecret(std::string_view name, bool internal = false);
    std::optional<std::string> secret(std::string_view name, bool internal = false);

    std::string pickleSecret();
    std::string createPickleSecret();

    template<class T>
    constexpr static bool isStateEvent_ =
      std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>,
                     mtx::events::StateEvent<decltype(std::declval<T>().content)>>;

signals:
    void newReadReceipts(const QString &room_id, const std::vector<QString> &event_ids);
    void roomReadStatus(const std::map<QString, bool> &status);
    void userKeysUpdate(const std::string &sync_token, const mtx::responses::QueryKeys &keyQuery);
    void userKeysUpdateFinalize(const std::string &user_id);
    void verificationStatusChanged(const std::string &userid);
    void selfVerificationStatusChanged();
    void secretChanged(const std::string name);
    void databaseReady();

private:
    void loadSecretsFromStore(
      std::vector<std::pair<std::string, bool>> toLoad,
      std::function<void(const std::string &name, bool internal, const std::string &value)>
        callback,
      bool databaseReadyOnFinished = false);
    void storeSecretInStore(const std::string name, const std::string secret);
    void deleteSecretFromStore(const std::string name, bool internal);

    //! Save an invited room.
    void saveInvite(db::Txn &txn,
                    db::Dbi &statesdb,
                    db::Dbi &membersdb,
                    const mtx::responses::InvitedRoom &room);

    QString getInviteRoomName(db::Txn &txn, db::Dbi &statesdb, db::Dbi &membersdb);
    QString getInviteRoomTopic(db::Txn &txn, db::Dbi &statesdb);
    QString getInviteRoomAvatarUrl(db::Txn &txn, db::Dbi &statesdb, db::Dbi &membersdb);
    bool getInviteRoomIsSpace(db::Txn &txn, db::Dbi &db);

    std::optional<MemberInfo> getMember(const std::string &room_id, const std::string &user_id);

    std::string getLastEventId(db::Txn &txn, const std::string &room_id);
    void saveTimelineMessages(db::Txn &txn,
                              db::Dbi &eventsDb,
                              const std::string &room_id,
                              const mtx::responses::Timeline &res);

    //! retrieve a specific event from account data
    //! pass empty room_id for global account data
    std::optional<mtx::events::collections::RoomAccountDataEvents>
    getAccountData(db::Txn &txn, mtx::events::EventType type, const std::string &room_id);
    bool isHiddenEvent(db::Txn &txn,
                       mtx::events::collections::TimelineEvents e,
                       const std::string &room_id);

    template<class T>
    void saveStateEvents(db::Txn &txn,
                         db::Dbi &statesdb,
                         db::Dbi &stateskeydb,
                         db::Dbi &membersdb,
                         db::Dbi &eventsDb,
                         const std::string &room_id,
                         const std::vector<T> &events);

    template<class T>
    void saveStateEvent(db::Txn &txn,
                        db::Dbi &statesdb,
                        db::Dbi &stateskeydb,
                        db::Dbi &membersdb,
                        db::Dbi &eventsDb,
                        const std::string &room_id,
                        const T &event);

    template<typename T>
    std::optional<mtx::events::StateEvent<T>>
    getStateEvent(db::Txn &txn, const std::string &room_id, std::string_view state_key = "");

    template<typename T>
    std::vector<mtx::events::StateEvent<T>>
    getStateEventsWithType(db::Txn &txn,
                           const std::string &room_id,
                           mtx::events::EventType type = mtx::events::state_content_to_type<T>);

    void saveInvites(db::Txn &txn, const std::map<std::string, mtx::responses::InvitedRoom> &rooms);

    void savePresence(
      db::Txn &txn,
      const std::vector<mtx::events::Event<mtx::events::presence::Presence>> &presenceUpdates);

    //! Sends signals for the rooms that are removed.
    void
    removeLeftRooms(db::Txn &txn, const std::map<std::string, mtx::responses::LeftRoom> &rooms);

    void updateSpaces(db::Txn &txn,
                      const std::set<std::string> &spaces_with_updates,
                      std::set<std::string> rooms_with_updates);

    db::Dbi getEventsDb(db::Txn &txn, const std::string &room_id);

    db::Dbi getEventOrderDb(db::Txn &txn, const std::string &room_id);

    // inverse of EventOrderDb
    db::Dbi getEventToOrderDb(db::Txn &txn, const std::string &room_id);

    db::Dbi getMessageToOrderDb(db::Txn &txn, const std::string &room_id);

    db::Dbi getOrderToMessageDb(db::Txn &txn, const std::string &room_id);

    db::Dbi getPendingMessagesDb(db::Txn &txn, const std::string &room_id);

    db::Dbi getRelationsDb(db::Txn &txn, const std::string &room_id);

    db::Dbi getInviteStatesDb(db::Txn &txn, const std::string &room_id);

    db::Dbi getInviteMembersDb(db::Txn &txn, const std::string &room_id);

    db::Dbi getStatesDb(db::Txn &txn, const std::string &room_id);

    db::Dbi getStatesKeyDb(db::Txn &txn, const std::string &room_id);

    db::Dbi getAccountDataDb(db::Txn &txn, const std::string &room_id);

    db::Dbi getMembersDb(db::Txn &txn, const std::string &room_id);

    db::Dbi getUserKeysDb(db::Txn &txn);

    db::Dbi getVerificationDb(db::Txn &txn);

    QString getDisplayName(const mtx::events::StateEvent<mtx::events::state::Member> &event);

    std::optional<VerificationCache> verificationCache(const std::string &user_id, db::Txn &txn);
    VerificationStatus verificationStatus_(const std::string &user_id, db::Txn &txn);
    std::optional<UserKeyCache> userKeys_(const std::string &user_id, db::Txn &txn);

    void setNextBatchToken(db::Txn &txn, const std::string &token);
    db::Backend &storage();
    const db::Backend &storage() const;
    db::Txn beginTxn(db::Txn *parent = nullptr, db::TxnFlags flags = db::TxnFlags::None);
    db::Dbi openNamedDbi(db::Txn &txn, std::string_view name, bool create = true);

    QString localUserId_;
    QString cacheDirectory_;

    std::string pickle_secret_;

    VerificationStorage verification_storage;

    bool databaseReady_ = false;

    std::unique_ptr<CacheDb> db;
};

#define NHEKO_CACHE_GET_STATE_EVENT_FORWARD(Content)                                               \
    extern template std::optional<mtx::events::StateEvent<Content>> Cache::getStateEvent<Content>( \
      const std::string &room_id, std::string_view state_key);                                     \
                                                                                                   \
    extern template std::vector<mtx::events::StateEvent<Content>>                                  \
    Cache::getStateEventsWithType<Content>(const std::string &room_id,                             \
                                           mtx::events::EventType type);

NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::Aliases)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::Avatar)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::CanonicalAlias)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::Create)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::Encryption)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::GuestAccess)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::HistoryVisibility)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::JoinRules)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::Member)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::Name)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::PinnedEvents)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::PowerLevels)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::Tombstone)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::ServerAcl)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::Topic)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::Widget)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::policy_rule::UserRule)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::policy_rule::RoomRule)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::policy_rule::ServerRule)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::space::Child)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::state::space::Parent)
NHEKO_CACHE_GET_STATE_EVENT_FORWARD(mtx::events::msc2545::ImagePack)
