// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

#include <optional>
#include <vector>

namespace qml_mtx_events {
Q_NAMESPACE
QML_NAMED_ELEMENT(MtxEvent)

enum EventType
{
    // Unsupported event
    Unsupported,
    /// m.room_key_request
    KeyRequest,
    /// m.reaction,
    Reaction,
    /// m.room.aliases
    Aliases,
    /// m.room.avatar
    Avatar,
    /// m.call.invite
    CallInvite,
    /// m.call.answer
    CallAnswer,
    /// m.call.hangup
    CallHangUp,
    /// m.call.candidates
    CallCandidates,
    /// m.call.select_answer
    CallSelectAnswer,
    /// m.call.reject
    CallReject,
    /// m.call.negotiate
    CallNegotiate,
    /// m.rtc.notification (MatrixRTC / Element Call "a call started")
    CallNotification,
    /// m.room.canonical_alias
    CanonicalAlias,
    /// m.room.create
    RoomCreate,
    /// m.room.encrypted.
    Encrypted,
    /// m.room.encryption.
    Encryption,
    /// m.room.guest_access
    RoomGuestAccess,
    /// m.room.history_visibility
    RoomHistoryVisibility,
    /// m.room.join_rules
    RoomJoinRules,
    /// m.room.member
    Member,
    /// m.room.name
    Name,
    /// m.room.power_levels
    PowerLevels,
    /// m.room.tombstone
    Tombstone,
    /// m.room.server_acl
    ServerAcl,
    /// m.room.topic
    Topic,
    /// m.room.redaction
    Redaction,
    /// m.room.pinned_events
    PinnedEvents,
    // m.sticker
    Sticker,
    // m.tag
    Tag,
    // m.widget
    Widget,
    /// m.room.message
    AudioMessage,
    ElementEffectMessage,
    EmoteMessage,
    FileMessage,
    ImageMessage,
    LocationMessage,
    NoticeMessage,
    TextMessage,
    UnknownMessage,
    VideoMessage,
    Redacted,
    UnknownEvent,
    KeyVerificationRequest,
    KeyVerificationStart,
    KeyVerificationMac,
    KeyVerificationAccept,
    KeyVerificationCancel,
    KeyVerificationKey,
    KeyVerificationDone,
    KeyVerificationReady,
    //! m.image_pack, currently im.ponies.room_emotes
    ImagePackInRoom,
    //! m.image_pack, currently im.ponies.user_emotes
    ImagePackInAccountData,
    //! m.image_pack.rooms, currently im.ponies.emote_rooms
    ImagePackRooms,
    // m.policy.rule.user
    PolicyRuleUser,
    // m.policy.rule.room
    PolicyRuleRoom,
    // m.policy.rule.server
    PolicyRuleServer,
    // m.space.parent
    SpaceParent,
    // m.space.child
    SpaceChild,
};
Q_ENUM_NS(EventType)

enum EventState
{
    //! The message has been committed to the user's homeserver and is part of
    //! the canonical timeline. "Sent" here is the user-facing notion ("it left
    //! your client"), not a claim that any remote homeserver or device has
    //! seen it. Maps to matrix-rust-sdk's post-sync state for own events.
    Sent,
    //! At least one of the participants has read the message.
    Read,
    //! The client is still sending the message: either queued locally, or the
    //! HTTP send succeeded but the server hasn't echoed the event back via
    //! sync yet. Both states are visually identical (clock + "Sending"), so
    //! they collapse into a single user-facing state.
    Pending,
    //! The client sent the message, but it failed.
    Failed,
    //! When the message is loaded from cache or backfill.
    Empty,
};
Q_ENUM_NS(EventState)

enum NotificationLevel
{
    Nothing,
    Notify,
    Highlight,
};
Q_ENUM_NS(NotificationLevel)

/// Event types that Komai hides by default as a local timeline preference.
/// These either have their own rendering paths (e.g. reactions as emoji pills)
/// or tend to be low-level protocol noise unless explicitly enabled.
std::vector<EventType>
defaultHiddenEventTypes();

QStringList
defaultHiddenTimelineEventTypeKeys();

QString
localTimelineEventTypeKey(EventType type);

std::optional<EventType>
localTimelineEventTypeFromKey(const QString &key);

EventType
matrixTimelineEventType(const QString &itemKind, const QString &matrixEventType);
}
