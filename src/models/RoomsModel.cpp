// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/RoomsModel.h"

#include <algorithm>
#include <chrono>
#include <unordered_set>

#include <QUrl>

#include "cache/Cache.h"
#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "models/CompletionModelRoles.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/DirectChatResolver.h"
#include "utils/Utils.h"

namespace {
// Forward-mode filtering: rooms without own activity within this window are
// deprioritised in the default (empty-search) suggestion list.
constexpr int kForwardOwnActivityDays      = 30;
constexpr int kForwardOwnActivityMaxEvents = 5000;

//! Scan the locally-cached timeline of @p room_id backwards looking for a
//! message sent by @p localUser within the last @p days days.
//! Scans at most @p maxEvents events to avoid unbounded work.
//! Returns the number of events actually scanned; sets @p found to true if a
//! matching own message was found.
int
scanForOwnRecentMessage(const std::string &room_id,
                        const std::string &localUser,
                        int days,
                        int maxEvents,
                        bool &found)
{
    found = false;

    const auto range = cache::getTimelineRange(room_id);
    if (!range.has_value())
        return 0;

    using namespace std::chrono;
    const auto cutoff =
      static_cast<uint64_t>(duration_cast<milliseconds>(
                              (system_clock::now() - std::chrono::days(days)).time_since_epoch())
                              .count());

    int scanned = 0;
    for (uint64_t idx = range->last;; --idx) {
        const auto eventId = cache::getTimelineEventId(room_id, idx);
        if (!eventId.has_value())
            break;

        ++scanned;
        const auto event = cache::getEvent(room_id, *eventId);
        if (event.has_value()) {
            // Stop scanning once we reach events older than the cutoff.
            const auto ts = mtx::accessors::origin_server_ts_ms(*event);
            if (ts > 0 && static_cast<uint64_t>(ts) < cutoff)
                break;

            if (mtx::accessors::is_message(*event) && mtx::accessors::sender(*event) == localUser) {
                found = true;
                return scanned;
            }
        }

        if (idx == range->first || scanned >= maxEvents)
            break;
    }

    return scanned;
}
} // namespace

RoomsModel::RoomsModel(bool showOnlyRoomWithAliases, bool forwardMode, QObject *parent)
  : QAbstractListModel(parent)
  , showOnlyRoomWithAliases_(showOnlyRoomWithAliases)
{
    rooms = cache::roomNamesAndAliases();

    if (showOnlyRoomWithAliases_)
        utils::erase_if(rooms, [](auto &r) { return r.alias.empty(); });

    std::ranges::sort(rooms,
                      [](auto &a, auto &b) { return a.recent_activity > b.recent_activity; });

    if (forwardMode) {
        const auto localUser = utils::localUser().toStdString();

        // Build a set of room IDs that have own recent activity.
        std::unordered_set<std::string> activeRooms;
        int totalEventsScanned = 0;
        for (const auto &room : rooms) {
            if (room.is_low_priority || room.is_space)
                continue;
            if (totalEventsScanned >= kForwardOwnActivityMaxEvents)
                break;
            const int budget = kForwardOwnActivityMaxEvents - totalEventsScanned;
            bool found       = false;
            totalEventsScanned +=
              scanForOwnRecentMessage(room.id, localUser, kForwardOwnActivityDays, budget, found);
            if (found)
                activeRooms.insert(room.id);
        }

        // Stable-partition: preferred rooms first, deprioritised rooms last.
        // Within each partition the existing recent_activity order is preserved.
        std::stable_partition(rooms.begin(), rooms.end(), [&](const RoomNameAlias &r) {
            if (r.is_low_priority)
                return false;
            if (r.is_space)
                return false;
            return activeRooms.contains(r.id);
        });
    }
}

QHash<int, QByteArray>
RoomsModel::roleNames() const
{
    return {
      {CompletionModel::CompletionRole, "completionRole"},
      {CompletionModel::SearchRole, "searchRole"},
      {CompletionModel::SearchRole2, "searchRole2"},
      {Roles::RoomAlias, "roomAlias"},
      {Roles::AvatarUrl, "avatarUrl"},
      {Roles::RoomID, "roomid"},
      {Roles::RawRoomID, "rawroomid"},
      {Roles::RoomName, "roomName"},
      {Roles::IsTombstoned, "isTombstoned"},
      {Roles::IsSpace, "isSpace"},
    };
}

QVariant
RoomsModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        switch (role) {
        case CompletionModel::CompletionRole: {
            auto alias = QString::fromStdString(rooms[index.row()].alias);
            if (UserSettings::instance()->composerInputMarkdownToHtmlEnabled()) {
                QString percentEncoding = QUrl::toPercentEncoding(alias);
                return QStringLiteral("[%1](https://matrix.to/#/%2)")
                  .arg(alias.replace("[", "\\[").replace("]", "\\]").toHtmlEscaped(),
                       percentEncoding);
            } else {
                return alias;
            }
        }
        case CompletionModel::SearchRole:
        case Qt::DisplayRole:
        case Roles::RoomAlias:
            return QString::fromStdString(rooms[index.row()].alias).toHtmlEscaped();
        case CompletionModel::SearchRole2:
        case Roles::RoomName: {
            // Use the DM-aware display name so the Quick Switcher and Forward
            // dialog show the same room name as the room list and header.
            auto roomId = QString::fromStdString(rooms[index.row()].id);
            auto dmName = DirectChatResolver::instance().dmRoomDisplayName(roomId);
            if (!dmName.isEmpty())
                return dmName.toHtmlEscaped();
            return QString::fromStdString(rooms[index.row()].name).toHtmlEscaped();
        }
        case CompletionModel::SearchRole3:
            return QString::fromStdString(rooms[index.row()].id);
        case Roles::AvatarUrl: {
            // The pre-computed avatar_url from roomNamesAndAliases() may be
            // empty for DM rooms. Fall back to cache::roomAvatarUrl() which
            // recomputes it fresh (with bot-aware partner detection).
            auto url = QString::fromStdString(rooms[index.row()].avatar_url);
            if (url.isEmpty())
                url = cache::roomAvatarUrl(rooms[index.row()].id);
            return url;
        }
        case Roles::RoomID:
            return QString::fromStdString(rooms[index.row()].id).toHtmlEscaped();
        case Roles::RawRoomID:
            return QString::fromStdString(rooms[index.row()].id);
        case Roles::IsTombstoned:
            return rooms[index.row()].is_tombstoned;
        case Roles::IsSpace:
            return rooms[index.row()].is_space;
        }
    }
    return {};
}
