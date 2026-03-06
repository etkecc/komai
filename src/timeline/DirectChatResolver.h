// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <set>

#include <QString>

//! Determines whether a room is a direct chat and who the other party is.
//!
//! Combines two data sources:
//!   1. The m.direct account data (authoritative — if present, the room is a DM).
//!   2. A member-count heuristic (rooms with <= 3 members, after bot elimination).
//!
//! Results are cached per room. Use reload() when m.direct changes and
//! invalidateForRoomId() when a room's membership changes.
class DirectChatResolver
{
public:
    static DirectChatResolver &instance();

    //! Returns true if the room is considered a direct chat.
    bool isDirectChat(const QString &roomId);

    //! Returns the other user's ID in a direct chat, or empty if not a DM.
    QString directChatPartner(const QString &roomId);

    //! Returns true if the room is a direct chat whose partner is a likely bot.
    bool isBotRoom(const QString &roomId);

    //! Re-reads m.direct account data, clears all cached results, and returns
    //! the set of room IDs whose m.direct mapping changed (symmetric difference
    //! of old vs new). Callers should emit dataChanged / signal updates for
    //! these rooms.
    std::set<QString> reload();

    //! Removes the cached result for a single room so the next query
    //! recomputes it (e.g. after a membership change).
    void invalidateForRoomId(const QString &roomId);

    //! For DM rooms without an explicit m.room.name, returns the partner's
    //! display name. Returns empty if the room is not a DM or has an explicit
    //! name. Presentation-layer callers can use this to override the cached
    //! room name from the store.
    QString dmRoomDisplayName(const QString &roomId);

private:
    DirectChatResolver() = default;

    //! Lazily loads m.direct account data into mdirectMap_ on first access.
    void ensureMDirectLoaded();

    //! Computes the DM partner for a room (not cached — called by the
    //! public query methods on cache miss).
    QString computePartner(const QString &roomId);

    //! Returns true if the user ID / display name suggest a bot or bridge
    //! service account (e.g. @telegrambot:server, "Telegram Bridge Bot").
    static bool isLikelyBotUser(const QString &userId, const QString &displayName);

    bool mdirectLoaded_ = false;
    std::map<QString, QString> mdirectMap_; // from m.direct: room_id → partner user_id

    // Per-room computed result. Empty QString = not direct.
    std::map<QString, QString> cache_;
};
