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
    hideDMs = false;

    auto hidden = UserSettings::instance()->hiddenTags();
    for (const auto &t : std::as_const(hidden)) {
        if (t.startsWith(u"tag:"))
            hiddenTags.push_back(t.mid(4));
        else if (t.startsWith(u"space:"))
            hiddenSpaces.push_back(t.mid(6));
        else if (t == QLatin1String("dm"))
            hideDMs = true;
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
FilteredRoomlistModel::hiddenByDms(int sourceRow) const
{
    return hideDMs && isDirectRow(sourceRow);
}

bool
FilteredRoomlistModel::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    if (filterType == FilterBy::Nothing) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;
        if (hiddenByTags(sourceRow) || hiddenBySpaces(sourceRow) || hiddenByDms(sourceRow))
            return false;
        return true;
    } else if (filterType == FilterBy::DirectChats) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;
        if (hiddenByTags(sourceRow) || hiddenBySpaces(sourceRow))
            return false;
        return isDirectRow(sourceRow);
    } else if (filterType == FilterBy::Tag) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;

        const auto tags = rowTags(sourceRow);
        if (!tags.contains(filterStr))
            return false;
        if (hiddenByTags(sourceRow, filterStr) || hiddenBySpaces(sourceRow) ||
            hiddenByDms(sourceRow))
            return false;
        return true;
    } else if (filterType == FilterBy::Space) {
        const auto idx = sourceRowIndex(sourceRow);
        if (filterStr == sourceModel()->data(idx, RoomlistModel::RoomId).toString())
            return true;

        const auto parents = rowParentSpaces(sourceRow);
        if (!parents.contains(filterStr))
            return false;

        if (hiddenByTags(sourceRow) || hiddenBySpaces(sourceRow, filterStr) ||
            hiddenByDms(sourceRow))
            return false;

        if (isSpaceRow(sourceRow) && !parents.contains(filterStr))
            return false;

        // If it is a preview but it can't be fetched, it is probably an inaccessible private room.
        // Hide it if the user isn't an admin.
        if (isPreviewRow(sourceRow) &&
            !sourceModel()->data(idx, RoomlistModel::IsPreviewFetched).toBool() &&
            !Permissions(filterStr).canChange(qml_mtx_events::SpaceChild)) {
            return false;
        }

        return true;
    } else {
        return true;
    }
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
