// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include <cstdint>
#include <utility>

#include <QClipboard>
#include <QGuiApplication>

#include "Permissions.h"
#include "TimelineModel.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {
enum NotificationImportance : short
{
    NoPreview          = -3,
    Preview            = -2,
    ImportanceDisabled = -1,
    AllEventsRead      = 0,
    Draft              = 1,
    NewMessage         = 2,
    NewMentions        = 3,
    Invite             = 4,
    SubSpace           = 5,
    CurrentSpace       = 6,
};
}

short int
FilteredRoomlistModel::calculateImportance(const QModelIndex &idx) const
{
    // Returns the degree of importance of the unread messages in the room.
    // If sorting by importance is disabled in settings, this only ever
    // returns ImportanceDisabled or Invite
    if (sourceModel()->data(idx, RoomlistModel::IsSpace).toBool()) {
        if (filterType == FilterBy::Space &&
            filterStr == sourceModel()->data(idx, RoomlistModel::RoomId).toString())
            return CurrentSpace;
        else
            return SubSpace;
    } else if (sourceModel()->data(idx, RoomlistModel::IsPreview).toBool()) {
        if (sourceModel()->data(idx, RoomlistModel::IsPreviewFetched).toBool())
            return Preview;
        else
            return NoPreview;
    } else if (sourceModel()->data(idx, RoomlistModel::IsInvite).toBool()) {
        return Invite;
    } else if (this->sidebarsRoomListSort ==
                 static_cast<int>(UserSettings::RoomSortOrder::Recent) ||
               this->sidebarsRoomListSort ==
                 static_cast<int>(UserSettings::RoomSortOrder::Alphabetical)) {
        return ImportanceDisabled;
    } else if (sourceModel()->data(idx, RoomlistModel::HasLoudNotification).toBool()) {
        return NewMentions;
    } else if (sourceModel()->data(idx, RoomlistModel::NotificationCount).toInt() > 0) {
        return NewMessage;
    } else if (sourceModel()->data(idx, RoomlistModel::HasDraft).toBool()) {
        return Draft;
    } else {
        return AllEventsRead;
    }
}

bool
FilteredRoomlistModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QModelIndex const left_idx  = sourceModel()->index(left.row(), 0, QModelIndex());
    QModelIndex const right_idx = sourceModel()->index(right.row(), 0, QModelIndex());

    // Sort by "importance" (i.e. invites before mentions before
    // notifs before new events before old events), then secondly
    // by recency.

    // Checking importance first
    const auto a_importance = calculateImportance(left_idx);
    const auto b_importance = calculateImportance(right_idx);
    if (a_importance != b_importance) {
        return a_importance > b_importance;
    }

    // Now sort by recency or room name
    // Zero if empty, otherwise the time that the event occured

    bool sortAlphabetically =
      (this->sidebarsRoomListSort ==
         static_cast<int>(UserSettings::RoomSortOrder::UnreadFirst_Alpha) ||
       this->sidebarsRoomListSort == static_cast<int>(UserSettings::RoomSortOrder::Alphabetical));

    if (sortAlphabetically) {
        QString a_order = sourceModel()->data(left_idx, RoomlistModel::RoomName).toString();
        QString b_order = sourceModel()->data(right_idx, RoomlistModel::RoomName).toString();

        auto comp = a_order.compare(b_order, Qt::CaseInsensitive);
        if (comp != 0)
            return comp < 0;
    } else {
        uint64_t a_order = sourceModel()->data(left_idx, RoomlistModel::Timestamp).toULongLong();
        uint64_t b_order = sourceModel()->data(right_idx, RoomlistModel::Timestamp).toULongLong();

        if (a_order != b_order)
            return a_order > b_order;
    }

    return left.row() < right.row();
}

FilteredRoomlistModel::FilteredRoomlistModel(RoomlistModel *model, QObject *parent)
  : QSortFilterProxyModel(parent)
  , roomlistmodel(model)
{
    instance_ = this;

    this->sidebarsRoomListSort = static_cast<int>(UserSettings::instance()->sidebarsRoomListSort());
    setSourceModel(model);
    setDynamicSortFilter(true);

    QObject::connect(UserSettings::instance().get(),
                     &UserSettings::sidebarsRoomListSortChanged,
                     this,
                     [this](UserSettings::RoomSortOrder order) {
                         this->sidebarsRoomListSort = static_cast<int>(order);
                         invalidate();
                     });

    connect(roomlistmodel,
            &RoomlistModel::currentRoomChanged,
            this,
            &FilteredRoomlistModel::currentRoomChanged);

    // Eagerly populate hidden tags/spaces from settings so that badge
    // computation during the first initializeSidebar() already sees them.
    updateHiddenTagsAndSpaces();

    sort(0);
}

FilteredRoomlistModel *
FilteredRoomlistModel::create(QQmlEngine *qmlEngine, QJSEngine *)
{
    // The instance has to exist before it is used. We cannot replace it.
    Q_ASSERT(instance_);

    // The engine has to have the same thread affinity as the singleton.
    Q_ASSERT(qmlEngine->thread() == instance_->thread());

    // There can only be one engine accessing the singleton.
    static QJSEngine *s_engine = nullptr;
    if (s_engine)
        Q_ASSERT(qmlEngine == s_engine);
    else
        s_engine = qmlEngine;

    QJSEngine::setObjectOwnership(instance_, QJSEngine::CppOwnership);
    return instance_;
}

TimelineModel *
FilteredRoomlistModel::getRoomById(const QString &id) const
{
    auto r = roomlistmodel->getRoomByIdWithReason(id, "qml.Rooms.getRoomById").data();
    QQmlEngine::setObjectOwnership(r, QQmlEngine::CppOwnership);
    return r;
}

void
FilteredRoomlistModel::updateHiddenTagsAndSpaces()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    beginFilterChange();
#endif

    hiddenTags.clear();
    hiddenSpaces.clear();
    hidePeople = false;
    hideBots   = false;
    hideGroups = false;

    auto hidden = UserSettings::instance()->hiddenTags();
    for (const auto &t : std::as_const(hidden)) {
        if (t.startsWith(u"tag:"))
            hiddenTags.push_back(t.mid(4));
        else if (t.startsWith(u"space:"))
            hiddenSpaces.push_back(t.mid(6));
        else if (t == QLatin1String("people"))
            hidePeople = true;
        else if (t == QLatin1String("bot"))
            hideBots = true;
        else if (t == QLatin1String("group"))
            hideGroups = true;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange();
#else
    invalidateFilter();
#endif
}

QModelIndex
FilteredRoomlistModel::sourceRowIndex(int sourceRow) const
{
    return sourceModel()->index(sourceRow, 0);
}

bool
FilteredRoomlistModel::isPreviewRow(int sourceRow) const
{
    return sourceModel()->data(sourceRowIndex(sourceRow), RoomlistModel::IsPreview).toBool();
}

bool
FilteredRoomlistModel::isSpaceRow(int sourceRow) const
{
    return sourceModel()->data(sourceRowIndex(sourceRow), RoomlistModel::IsSpace).toBool();
}

bool
FilteredRoomlistModel::isDirectRow(int sourceRow) const
{
    return sourceModel()->data(sourceRowIndex(sourceRow), RoomlistModel::IsDirect).toBool();
}

bool
FilteredRoomlistModel::isBotRow(int sourceRow) const
{
    return sourceModel()->data(sourceRowIndex(sourceRow), RoomlistModel::IsBotRoom).toBool();
}

QStringList
FilteredRoomlistModel::rowTags(int sourceRow) const
{
    return sourceModel()->data(sourceRowIndex(sourceRow), RoomlistModel::Tags).toStringList();
}

QStringList
FilteredRoomlistModel::rowParentSpaces(int sourceRow) const
{
    return sourceModel()
      ->data(sourceRowIndex(sourceRow), RoomlistModel::ParentSpaces)
      .toStringList();
}

bool
FilteredRoomlistModel::hiddenByTags(int sourceRow, const QString &requiredTag) const
{
    if (hiddenTags.empty())
        return false;

    const auto tags = rowTags(sourceRow);
    for (const auto &tag : tags) {
        if (!requiredTag.isEmpty() && tag == requiredTag)
            continue;
        if (hiddenTags.contains(tag))
            return true;
    }

    return false;
}

bool
FilteredRoomlistModel::hiddenBySpaces(int sourceRow, const QString &requiredSpace) const
{
    if (hiddenSpaces.empty())
        return false;

    const auto parents = rowParentSpaces(sourceRow);
    for (const auto &space : parents) {
        if (!requiredSpace.isEmpty() && space == requiredSpace)
            continue;
        if (hiddenSpaces.contains(space))
            return true;
    }

    return false;
}

bool
FilteredRoomlistModel::hiddenByPeople(int sourceRow) const
{
    return hidePeople && isDirectRow(sourceRow) && !isBotRow(sourceRow);
}

bool
FilteredRoomlistModel::hiddenByBots(int sourceRow) const
{
    return hideBots && isBotRow(sourceRow);
}

bool
FilteredRoomlistModel::hiddenByGroups(int sourceRow) const
{
    return hideGroups && !isDirectRow(sourceRow);
}

bool
FilteredRoomlistModel::acceptsForFilter(int sourceRow, FilterBy type, const QString &str) const
{
    if (type == FilterBy::Nothing) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;
        if (hiddenByTags(sourceRow) || hiddenBySpaces(sourceRow) || hiddenByPeople(sourceRow) ||
            hiddenByBots(sourceRow) || hiddenByGroups(sourceRow))
            return false;
        return true;
    } else if (type == FilterBy::People) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;
        if (hiddenByTags(sourceRow) || hiddenBySpaces(sourceRow))
            return false;
        return isDirectRow(sourceRow) && !isBotRow(sourceRow);
    } else if (type == FilterBy::Bots) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;
        if (hiddenByTags(sourceRow) || hiddenBySpaces(sourceRow))
            return false;
        return isBotRow(sourceRow);
    } else if (type == FilterBy::Groups) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;
        if (hiddenByTags(sourceRow) || hiddenBySpaces(sourceRow) || hiddenByBots(sourceRow))
            return false;
        return !isDirectRow(sourceRow);
    } else if (type == FilterBy::Tag) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;

        const auto tags = rowTags(sourceRow);
        if (!tags.contains(str))
            return false;
        if (hiddenByTags(sourceRow, str) || hiddenBySpaces(sourceRow) ||
            hiddenByPeople(sourceRow) || hiddenByBots(sourceRow) || hiddenByGroups(sourceRow))
            return false;
        return true;
    } else if (type == FilterBy::Space) {
        const auto idx = sourceRowIndex(sourceRow);

        // The space room itself is rendered as a separate header above the
        // room list — don't include it as a regular list entry.
        if (str == sourceModel()->data(idx, RoomlistModel::RoomId).toString())
            return false;

        const auto parents = rowParentSpaces(sourceRow);
        if (!parents.contains(str))
            return false;

        if (hiddenByTags(sourceRow) || hiddenBySpaces(sourceRow, str) ||
            hiddenByPeople(sourceRow) || hiddenByBots(sourceRow) || hiddenByGroups(sourceRow))
            return false;

        if (isSpaceRow(sourceRow) && !parents.contains(str))
            return false;

        // If it is a preview but it can't be fetched, it is probably an inaccessible private room.
        // Hide it if the user isn't an admin.
        if (isPreviewRow(sourceRow) &&
            !sourceModel()->data(idx, RoomlistModel::IsPreviewFetched).toBool() &&
            !Permissions(str).canChange(qml_mtx_events::SpaceChild)) {
            return false;
        }

        return true;
    } else {
        return true;
    }
}

bool
FilteredRoomlistModel::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    return acceptsForFilter(sourceRow, filterType, filterStr);
}

QHash<QString, FilteredRoomlistModel::FilterBadge>
FilteredRoomlistModel::computeFilterBadges(const QStringList &communityIds) const
{
    QHash<QString, FilterBadge> result;

    // Parse each community ID into FilterBy + filterStr, same as updateFilterTag().
    struct ParsedFilter
    {
        QString id;
        FilterBy type;
        QString str;
    };
    std::vector<ParsedFilter> filters;
    filters.reserve(communityIds.size());
    for (const auto &cid : communityIds) {
        ParsedFilter f;
        f.id = cid;
        if (cid.startsWith(QLatin1String("tag:"))) {
            f.type = FilterBy::Tag;
            f.str  = cid.mid(4);
        } else if (cid.startsWith(QLatin1String("space:"))) {
            f.type = FilterBy::Space;
            f.str  = cid.mid(6);
        } else if (cid == QLatin1String("people")) {
            f.type = FilterBy::People;
        } else if (cid == QLatin1String("bot")) {
            f.type = FilterBy::Bots;
        } else if (cid == QLatin1String("group")) {
            f.type = FilterBy::Groups;
        } else {
            f.type = FilterBy::Nothing;
        }
        filters.push_back(std::move(f));
        result[cid] = {};
    }

    const int rows = sourceModel()->rowCount();
    for (int row = 0; row < rows; row++) {
        const auto idx = sourceModel()->index(row, 0);

        // Space rooms are structural — don't count them toward any badge.
        if (sourceModel()->data(idx, RoomlistModel::IsSpace).toBool())
            continue;

        // Match the visual "emphasizeUnreadState" logic from the QML delegate:
        // low-priority rooms are only considered unread when they have a loud notification.
        bool hasUnread    = sourceModel()->data(idx, RoomlistModel::HasUnreadMessages).toBool();
        bool hasDraft     = sourceModel()->data(idx, RoomlistModel::HasDraft).toBool();
        bool hasHighlight = sourceModel()->data(idx, RoomlistModel::HasLoudNotification).toBool();

        bool isLowPriority = false;
        if (hasUnread && !hasHighlight) {
            const auto tags = sourceModel()->data(idx, RoomlistModel::Tags).toStringList();
            if (tags.contains(QStringLiteral("m.lowpriority")))
                isLowPriority = true;
        }

        bool active           = hasUnread || hasDraft;
        bool suppressedActive = (isLowPriority ? (hasDraft || hasHighlight) : active);
        if (!active && !hasHighlight)
            continue;

        for (const auto &f : filters) {
            if (acceptsForFilter(row, f.type, f.str)) {
                // For the m.lowpriority tag filter itself, count all unread rooms.
                // For other filters, suppress low-priority rooms without highlights.
                bool isLowPriorityFilter =
                  (f.type == FilterBy::Tag && f.str == QStringLiteral("m.lowpriority"));
                bool countActive = isLowPriorityFilter ? active : suppressedActive;

                auto &badge = result[f.id];
                if (countActive)
                    badge.unreadCount++;
                if (hasHighlight)
                    badge.hasHighlight = true;
            }
        }
    }

    // Space rooms are skipped above (structural, not regular rooms) but should
    // still count toward the "All rooms" badge — unless the space itself is excluded.
    if (result.contains(QString())) {
        for (int row = 0; row < rows; row++) {
            const auto idx = sourceModel()->index(row, 0);
            if (!sourceModel()->data(idx, RoomlistModel::IsSpace).toBool())
                continue;

            // Respect the per-space Exclude setting.
            auto roomId = sourceModel()->data(idx, RoomlistModel::RoomId).toString();
            if (hiddenSpaces.contains(roomId))
                continue;

            bool hasUnread = sourceModel()->data(idx, RoomlistModel::HasUnreadMessages).toBool();
            bool hasDraft  = sourceModel()->data(idx, RoomlistModel::HasDraft).toBool();
            bool hasHighlight =
              sourceModel()->data(idx, RoomlistModel::HasLoudNotification).toBool();
            bool active = hasUnread || hasDraft;
            if (!active && !hasHighlight)
                continue;

            auto &badge = result[QString()];
            if (active)
                badge.unreadCount++;
            if (hasHighlight)
                badge.hasHighlight = true;
        }
    }

    return result;
}

void
FilteredRoomlistModel::toggleTag(const QString &roomid, const QString &tag, bool on)
{
    if (on) {
        http::client()->put_tag(
          roomid.toStdString(), tag.toStdString(), {}, [tag](mtx::http::RequestErr err) {
              if (err) {
                  nhlog::ui()->error(
                    "Failed to add tag: {}, {}", tag.toStdString(), err->matrix_error.error);
              }
          });
    } else {
        http::client()->delete_tag(
          roomid.toStdString(), tag.toStdString(), [tag](mtx::http::RequestErr err) {
              if (err) {
                  nhlog::ui()->error(
                    "Failed to delete tag: {}, {}", tag.toStdString(), err->matrix_error.error);
              }
          });
    }
}

void
FilteredRoomlistModel::copyLink(QString roomid)
{
    auto link = QStringLiteral("%1?%2").arg(TimelineModel::getBareRoomLink(roomid),
                                            TimelineModel::getRoomVias(roomid));
    QGuiApplication::clipboard()->setText(link);
}

void
FilteredRoomlistModel::nextRoomWithActivity()
{
    int roomWithMention       = -1;
    int roomWithNotification  = -1;
    int roomWithUnreadMessage = -1;
    auto r                    = currentRoom();
    int currentRoomIdx        = r ? roomidToIndex(r->roomId()) : -1;
    // first look for mentions
    for (int i = 0; i < (int)roomlistmodel->roomids.size(); i++) {
        if (i == currentRoomIdx)
            continue;
        if (this->data(index(i, 0), RoomlistModel::HasLoudNotification).toBool()) {
            roomWithMention = i;
            break;
        }
        if (roomWithNotification == -1 &&
            this->data(index(i, 0), RoomlistModel::NotificationCount).toInt() > 0) {
            roomWithNotification = i;
            // don't break, we must continue looking for rooms with mentions
        }
        if (roomWithNotification == -1 && roomWithUnreadMessage == -1 &&
            this->data(index(i, 0), RoomlistModel::HasUnreadMessages).toBool()) {
            roomWithUnreadMessage = i;
            // don't break, we must continue looking for rooms with mentions
        }
    }
    QString targetRoomId = nullptr;
    if (roomWithMention != -1) {
        targetRoomId = this->data(index(roomWithMention, 0), RoomlistModel::RoomId).toString();
        nhlog::ui()->debug("choosing {} for mentions", targetRoomId.toStdString());
    } else if (roomWithNotification != -1) {
        targetRoomId = this->data(index(roomWithNotification, 0), RoomlistModel::RoomId).toString();
        nhlog::ui()->debug("choosing {} for notifications", targetRoomId.toStdString());
    } else if (roomWithUnreadMessage != -1) {
        targetRoomId =
          this->data(index(roomWithUnreadMessage, 0), RoomlistModel::RoomId).toString();
        nhlog::ui()->debug("choosing {} for unread messages", targetRoomId.toStdString());
    }
    if (targetRoomId != nullptr) {
        setCurrentRoom(targetRoomId);
    }
}

void
FilteredRoomlistModel::nextRoom()
{
    auto r = currentRoom();

    if (r) {
        int idx = roomidToIndex(r->roomId());
        idx++;
        if (idx < rowCount()) {
            setCurrentRoom(data(index(idx, 0), RoomlistModel::Roles::RoomId).toString());
        }
    }
}

void
FilteredRoomlistModel::previousRoom()
{
    auto r = currentRoom();

    if (r) {
        int idx = roomidToIndex(r->roomId());
        idx--;
        if (idx >= 0) {
            setCurrentRoom(data(index(idx, 0), RoomlistModel::Roles::RoomId).toString());
        }
    }
}
