// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <optional>
#include <variant>

#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVersionNumber>

namespace komai::ipc {

/// The IPC API version. Kept in sync with komai::dbus::dbusApiVersion in Api.h.
inline const QVersionNumber apiVersionNumber{1, 0, 0};

/// Shared business logic called by both D-Bus adaptors and IPC server.
/// These functions do NOT perform access checks -- callers are responsible.

// -- app --

QString
apiVersion();
QString
appVersion();

// -- rooms --

struct RoomInfo
{
    QString roomId;
    QString alias;
    QString name;
    QString avatarUrl;
    bool read                             = true;
    int unreadCount                       = 0;
    int memberCount                       = 0;
    qulonglong mostRecentEventTimestampMs = 0;
    bool highlighted                      = false;
    QStringList categories                = {};
    QStringList tags                      = {};
    QStringList parentSpaces;
    QString directUserId;
    bool encrypted = false;

    /// Serializes the room. `fields` projects the result down to the named
    /// keys; empty means every key.
    QJsonObject toJson(const QStringList &fields = {}) const;

    /// Every key toJson() can emit, which is also the set `fields` accepts.
    static QStringList fieldNames();
};

/// Filters and paging for roomList(). Every member is optional; a default
/// constructed query returns every room, unprojected.
struct RoomListQuery
{
    /// Room IDs or aliases to restrict the result to.
    QStringList ids;
    /// Case-insensitive substring matched against the room name and alias.
    QString query;
    std::optional<bool> isDm;
    std::optional<bool> encrypted;
    /// Room ID of a space the room must be a child of.
    QString parentSpace;
    /// Matrix room tag the room must carry.
    QString tag;
    std::optional<int> minMemberCount;
    /// Page size. Negative means no limit.
    int limit  = -1;
    int offset = 0;
    /// Keys to keep in the serialized rooms; empty means all of them.
    QStringList fields;
};

struct RoomListPage
{
    QVector<RoomInfo> rooms;
    /// Rooms matching the filters, counted before `limit` and `offset` are
    /// applied, so a caller can tell it is looking at a subset. Not the total
    /// number of joined rooms unless the query has no filters.
    int matchCount = 0;
};

QVector<RoomInfo>
roomList();

/// Filtered, paged room list. Returns an error string for an unknown field
/// name or a negative offset, otherwise the page.
std::variant<RoomListPage, QString>
roomList(const RoomListQuery &query);
void
joinRoom(const QString &roomIdOrAlias);
void
newDirectChat(const QString &userId);

// -- user --

QString
userId();
QString
homeserverUrl();
QString
deviceId();
QString
statusMessage();
void
setStatusMessage(const QString &message);

// -- settings.ui --

QString
uiTheme();
void
setUiTheme(const QString &theme);

// -- rooms (messaging) --

/// Callback for async send results.
/// On success: eventId is set, error is empty.
/// On failure: eventId is empty, error describes what went wrong.
using SendMessageCallback = std::function<void(const QString &eventId, const QString &error)>;

/// Callback for async timeline retrieval.
/// On success: result is set, error is empty.
/// On failure: result is empty, error describes what went wrong.
using ReadTimelineCallback = std::function<void(const QJsonObject &result, const QString &error)>;

/// Resolves a room ID or alias to a room ID.
/// If the input starts with '!', returns it directly.
/// If it starts with '#', searches the local cache for a match.
/// Returns empty string if the room is not found among joined rooms.
QString
resolveRoomId(const QString &roomIdOrAlias);

/// Reads visible timeline events for a room, newest first.
void
readTimeline(const QString &roomIdOrAlias,
             int limit,
             const QString &beforeEventId,
             bool includeUnsignedFields,
             const QString &fetchMode,
             ReadTimelineCallback callback);

/// Sends a text or notice message to a room.
void
sendMessage(const QString &roomIdOrAlias,
            const QString &body,
            const QString &msgtype,
            const QString &format,
            SendMessageCallback callback);

/// Uploads an image from disk and sends it to a room in one step.
/// Handles encryption transparently for encrypted rooms.
void
sendImageFromFile(const QString &roomIdOrAlias,
                  const QString &filePath,
                  const QString &body,
                  SendMessageCallback callback);

/// Sends an image message using an already-uploaded mxc:// URI.
/// Only works for unencrypted rooms.
void
sendImage(const QString &roomIdOrAlias,
          const QString &mxcUri,
          const QString &body,
          const QString &filename,
          const QJsonObject &info,
          SendMessageCallback callback);

// -- rooms (creation) --

/// Inputs for a Matrix createRoom call.
///
/// The three JSON members are passed to the homeserver untouched: Komai does
/// not model their schemas, so anything Matrix defines for them now or later
/// works without a change here. Empty means "not sent".
struct CreateRoomRequest
{
    QString name;
    QString topic;
    QString aliasLocalpart;
    QStringList inviteUserIds;
    /// One of: private_chat, public_chat, trusted_private_chat.
    QString preset;
    bool isDirect    = false;
    bool isEncrypted = false;
    bool isSpace     = false;
    bool isPublic    = false;
    QString roomVersion;
    QJsonObject powerLevelContentOverride;
    QJsonArray initialState;
    QJsonObject creationContent;
};

/// Callback for async room creation.
/// On success: roomId is set, error is empty.
/// On failure: roomId is empty, error describes what went wrong.
using CreateRoomCallback = std::function<void(const QString &roomId, const QString &error)>;

/// Creates a room. Unlike the GUI path, this does not switch the active room.
void
createRoom(const CreateRoomRequest &request, CreateRoomCallback callback);

// -- rooms (membership) --

/// Callback for async room actions that carry no payload.
/// On success: error is empty.
/// On failure: error describes what went wrong.
using RoomActionCallback = std::function<void(const QString &error)>;

/// Invites a user to a room.
void
inviteUser(const QString &roomIdOrAlias,
           const QString &userId,
           const QString &reason,
           RoomActionCallback callback);

/// Removes a user from a room. The user may rejoin unless they are also banned.
void
kickUser(const QString &roomIdOrAlias,
         const QString &userId,
         const QString &reason,
         RoomActionCallback callback);

/// Bans a user from a room, removing them if they are currently joined.
void
banUser(const QString &roomIdOrAlias,
        const QString &userId,
        const QString &reason,
        RoomActionCallback callback);

/// Lifts a ban, allowing the user to rejoin. Does not re-invite them.
void
unbanUser(const QString &roomIdOrAlias,
          const QString &userId,
          const QString &reason,
          RoomActionCallback callback);

/// Leaves a room, or rejects a pending invite. Does not forget the room.
void
leaveRoom(const QString &roomIdOrAlias, const QString &reason, RoomActionCallback callback);

// -- rooms (state) --

struct StateEventResult
{
    /// False when the room has no such state event, which is an answer rather
    /// than a failure.
    bool exists = false;
    QJsonObject content;
};

/// Callback for async state reads.
using ReadStateCallback = std::function<void(const StateEventResult &result, const QString &error)>;

/// Reads one state event's content from the homeserver.
///
/// Always a server round trip: Komai syncs via sliding sync, so the local
/// state store only holds the types the room list asked for, and any other
/// type would read back as missing even when the room has it.
void
readStateEvent(const QString &roomIdOrAlias,
               const QString &eventType,
               const QString &stateKey,
               ReadStateCallback callback);

/// Sends a state event with caller-supplied content, yielding its event ID.
///
/// `content` replaces the event wholesale rather than merging into it. For
/// m.room.power_levels that means an incomplete object silently drops every
/// level it omits, which is why setUserPowerLevel exists.
void
sendStateEvent(const QString &roomIdOrAlias,
               const QString &eventType,
               const QString &stateKey,
               const QJsonObject &content,
               SendMessageCallback callback);

/// Sets the room name (m.room.name).
void
setRoomName(const QString &roomIdOrAlias, const QString &name, RoomActionCallback callback);

/// Sets the room topic (m.room.topic).
void
setRoomTopic(const QString &roomIdOrAlias, const QString &topic, RoomActionCallback callback);

/// Sets one user's power level, preserving every other entry in
/// m.room.power_levels.
void
setUserPowerLevel(const QString &roomIdOrAlias,
                  const QString &userId,
                  int powerLevel,
                  RoomActionCallback callback);

// -- rooms (moderation and read state) --

/// Redacts an event, yielding the redaction's own event ID.
void
redactEvent(const QString &roomIdOrAlias,
            const QString &eventId,
            const QString &reason,
            SendMessageCallback callback);

/// Marks a room read up to `eventId`, or up to its latest event when
/// `eventId` is empty.
///
/// `publicReceipt` chooses between m.read and m.read.private; when it is
/// nullopt the user's own read-receipt preference for the room decides, so an
/// automation call does not quietly broadcast what the app would have kept
/// private.
void
markRoomRead(const QString &roomIdOrAlias,
             const QString &eventId,
             std::optional<bool> publicReceipt,
             RoomActionCallback callback);

/// Sets or clears the room's marked-unread flag.
void
markRoomUnread(const QString &roomIdOrAlias, bool unread, RoomActionCallback callback);

struct ReadReceipt
{
    QString userId;
    QString displayName;
    qulonglong timestampMs = 0;

    QJsonObject toJson() const;
};

using ReadReceiptsCallback =
  std::function<void(const QVector<ReadReceipt> &receipts, const QString &error)>;

/// Lists who has a read receipt at or past a given event.
///
/// The active account's own receipt is never listed, so an empty result means
/// nobody else has read that far. Receipts are cumulative: reading a later
/// event counts as having read this one.
void
readReceipts(const QString &roomIdOrAlias, const QString &eventId, ReadReceiptsCallback callback);

// -- media --

using MediaFetchCallback = std::function<void(const QImage &)>;
void
mediaFetch(const QString &mxcUri, MediaFetchCallback callback);

struct UploadResult
{
    QString mxcUri;
    QString contentType;
    QString filename;
    uint64_t size = 0;

    QJsonObject toJson() const;
};

using MediaUploadCallback = std::function<void(const UploadResult &result, const QString &error)>;

/// Uploads a file to the homeserver (unencrypted) and returns its mxc:// URI.
void
mediaUpload(const QString &filePath,
            const QString &filename,
            const QString &contentType,
            MediaUploadCallback callback);

} // namespace komai::ipc
