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
#include "TimelineEventTypes.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"

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
    } else if (this->navigationRoomListSort ==
                 static_cast<int>(UserSettings::RoomSortOrder::Recent) ||
               this->navigationRoomListSort ==
                 static_cast<int>(UserSettings::RoomSortOrder::Alphabetical)) {
        return ImportanceDisabled;
    } else if (sourceModel()->data(idx, RoomlistModel::HasLoudNotification).toBool()) {
        return NewMentions;
    } else if (sourceModel()->data(idx, RoomlistModel::UnreadCount).toInt() > 0) {
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
      (this->navigationRoomListSort ==
         static_cast<int>(UserSettings::RoomSortOrder::UnreadFirst_Alpha) ||
       this->navigationRoomListSort == static_cast<int>(UserSettings::RoomSortOrder::Alphabetical));

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

    this->navigationRoomListSort =
      static_cast<int>(UserSettings::instance()->navigationRoomListSort());
    setSourceModel(model);
    setDynamicSortFilter(true);

    QObject::connect(UserSettings::instance().get(),
                     &UserSettings::navigationRoomListSortChanged,
                     this,
                     [this](UserSettings::RoomSortOrder order) {
                         this->navigationRoomListSort = static_cast<int>(order);
                         invalidate();
                     });
    QObject::connect(UserSettings::instance().get(),
                     &UserSettings::globalExcludesChanged,
                     this,
                     &FilteredRoomlistModel::updateGlobalExcludes);

    connect(roomlistmodel,
            &RoomlistModel::currentRoomIdChanged,
            this,
            &FilteredRoomlistModel::currentRoomIdChanged);
    connect(roomlistmodel,
            &RoomlistModel::currentRoomPreviewChanged,
            this,
            &FilteredRoomlistModel::currentRoomPreviewChanged);
    connect(roomlistmodel,
            &RoomlistModel::suppressedUpdatesChanged,
            this,
            &FilteredRoomlistModel::hasSuppressedUpdatesChanged);

    // Eagerly populate hidden tags/spaces from settings so that badge
    // computation during the first initializeSidebar() already sees them.
    updateGlobalExcludes();

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

void
FilteredRoomlistModel::setInteractionSuppressed(bool suppressed)
{
    if (interactionSuppressed_ == suppressed)
        return;

    interactionSuppressed_ = suppressed;

    if (interactionSuppressed_) {
        nhlog::ui()->info("Pausing room list live updates while the user is interacting with it");
        setDynamicSortFilter(false);
        roomlistmodel->setInteractionSuppressed(true);
        return;
    }

    nhlog::ui()->info("Resuming room list live updates after room-list interaction");

    // Re-enable dynamic sort/filter BEFORE applying deferred changes so the proxy
    // auto-sorts incrementally as the source model emits signals.  This avoids a
    // blanket invalidate() + sort(0) which emits layoutChanged and resets the
    // QML ListView scroll position.
    setDynamicSortFilter(true);
    roomlistmodel->setInteractionSuppressed(false);
}

void
FilteredRoomlistModel::updateGlobalExcludes()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    beginFilterChange();
#endif

    globalExcludedTags.clear();
    globalExcludedSpaces.clear();
    excludePeople = false;
    excludeBots   = false;
    excludeGroups = false;

    auto excluded = UserSettings::instance()->globalExcludes();
    for (const auto &t : std::as_const(excluded)) {
        if (t.startsWith(u"tag:"))
            globalExcludedTags.push_back(t.mid(4));
        else if (t.startsWith(u"space:"))
            globalExcludedSpaces.push_back(t.mid(6));
        else if (t == QLatin1String("people"))
            excludePeople = true;
        else if (t == QLatin1String("bot"))
            excludeBots = true;
        else if (t == QLatin1String("group"))
            excludeGroups = true;
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
FilteredRoomlistModel::excludedByTags(int sourceRow, const QString &requiredTag) const
{
    if (globalExcludedTags.empty())
        return false;

    const auto tags = rowTags(sourceRow);
    for (const auto &tag : tags) {
        if (!requiredTag.isEmpty() && tag == requiredTag)
            continue;
        if (globalExcludedTags.contains(tag))
            return true;
    }

    return false;
}

bool
FilteredRoomlistModel::excludedBySpaces(int sourceRow, const QString &requiredSpace) const
{
    if (globalExcludedSpaces.empty())
        return false;

    const auto parents = rowParentSpaces(sourceRow);
    for (const auto &space : parents) {
        if (!requiredSpace.isEmpty() && space == requiredSpace)
            continue;
        if (globalExcludedSpaces.contains(space))
            return true;
    }

    return false;
}

bool
FilteredRoomlistModel::excludedByPeople(int sourceRow) const
{
    return excludePeople && isDirectRow(sourceRow) && !isBotRow(sourceRow);
}

bool
FilteredRoomlistModel::excludedByBots(int sourceRow) const
{
    return excludeBots && isBotRow(sourceRow);
}

bool
FilteredRoomlistModel::excludedByGroups(int sourceRow) const
{
    return excludeGroups && !isDirectRow(sourceRow);
}

bool
FilteredRoomlistModel::acceptsForFilter(int sourceRow, FilterBy type, const QString &str) const
{
    if (type == FilterBy::Nothing) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;
        if (excludedByTags(sourceRow) || excludedBySpaces(sourceRow) ||
            excludedByPeople(sourceRow) || excludedByBots(sourceRow) || excludedByGroups(sourceRow))
            return false;
        return true;
    } else if (type == FilterBy::People) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;
        if (excludedByTags(sourceRow) || excludedBySpaces(sourceRow))
            return false;
        return isDirectRow(sourceRow) && !isBotRow(sourceRow);
    } else if (type == FilterBy::Bots) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;
        if (excludedByTags(sourceRow) || excludedBySpaces(sourceRow))
            return false;
        return isBotRow(sourceRow);
    } else if (type == FilterBy::Groups) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;
        if (excludedByTags(sourceRow) || excludedBySpaces(sourceRow) || excludedByBots(sourceRow))
            return false;
        return !isDirectRow(sourceRow);
    } else if (type == FilterBy::Tag) {
        if (isPreviewRow(sourceRow) || isSpaceRow(sourceRow))
            return false;

        const auto tags = rowTags(sourceRow);
        if (!tags.contains(str))
            return false;
        if (excludedByTags(sourceRow, str) || excludedBySpaces(sourceRow) ||
            excludedByPeople(sourceRow) || excludedByBots(sourceRow) || excludedByGroups(sourceRow))
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

        if (excludedByTags(sourceRow) || excludedBySpaces(sourceRow, str) ||
            excludedByPeople(sourceRow) || excludedByBots(sourceRow) || excludedByGroups(sourceRow))
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

        const auto attentionState = RoomlistModel::attentionStateForRow(sourceModel(), idx);
        if (!attentionState.hasAnyAttention())
            continue;

        for (const auto &f : filters) {
            if (acceptsForFilter(row, f.type, f.str)) {
                // For the m.lowpriority tag filter itself, count all unread rooms.
                // For other filters, suppress low-priority rooms without highlights.
                bool isLowPriorityFilter =
                  (f.type == FilterBy::Tag && f.str == QStringLiteral("m.lowpriority"));
                bool countActive = attentionState.countAsActive(isLowPriorityFilter);

                auto &badge = result[f.id];
                if (countActive)
                    badge.unreadCount++;
                if (attentionState.hasHighlight)
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
            if (globalExcludedSpaces.contains(roomId))
                continue;

            const auto attentionState = RoomlistModel::attentionStateForRow(sourceModel(), idx);
            if (!attentionState.hasAnyAttention())
                continue;

            auto &badge = result[QString()];
            if (attentionState.countAsActive(false))
                badge.unreadCount++;
            if (attentionState.hasHighlight)
                badge.hasHighlight = true;
        }
    }

    return result;
}

void
FilteredRoomlistModel::toggleTag(const QString &roomid, const QString &tag, bool on)
{
    const auto roomId = roomid.trimmed();
    const auto tagId  = tag.trimmed();
    if (roomId.isEmpty() || tagId.isEmpty())
        return;

    const auto preview = roomlistmodel->getRoomPreviewById(roomId);
    if (!preview.isMatrixSummary()) {
        nhlog::ui()->warn(
          "Refusing to toggle room tag '{}' for non-joined or unavailable room '{}'",
          tagId.toStdString(),
          roomId.toStdString());
        if (auto *mainWindow = MainWindow::instance()) {
            mainWindow->showNotification(
              tr("Room tags can only be changed for joined rooms available in this session."));
        }
        return;
    }

    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        nhlog::ui()->warn("Refusing to toggle matrix-sdk room tag '{}' for '{}' without an "
                          "active backend handle",
                          tagId.toStdString(),
                          roomId.toStdString());
        if (mainWindow) {
            mainWindow->showNotification(tr(
              "Room tags are temporarily unavailable because the Matrix session is not active."));
        }
        return;
    }

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, tagId, on]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::toggleRoomTag(
            context, handleId, roomId, tagId, on, &error);
          return std::make_pair(ok, error);
      },
      [roomId, tagId, on](FilteredRoomlistModel *, const std::pair<bool, QString> &result) {
          const auto &[ok, error] = result;
          if (ok)
              return;

          nhlog::ui()->warn("Failed to toggle matrix-sdk room tag '{}' for '{}': {}",
                            tagId.toStdString(),
                            roomId.toStdString(),
                            error.toStdString());
          if (auto *mainWindow = MainWindow::instance()) {
              mainWindow->showNotification(on ? tr("Failed to add room tag: %1").arg(error)
                                              : tr("Failed to remove room tag: %1").arg(error));
          }
      });
}

void
FilteredRoomlistModel::copyLink(QString roomid)
{
    const auto link = QStringLiteral("https://matrix.to/#/%1").arg(roomid);
    QGuiApplication::clipboard()->setText(link);
}

void
FilteredRoomlistModel::nextRoomWithActivity()
{
    int roomWithMention       = -1;
    int roomWithNotification  = -1;
    int roomWithUnreadMessage = -1;
    const auto selectedRoomId = currentRoomId();
    int currentRoomIdx        = selectedRoomId.isEmpty() ? -1 : roomidToIndex(selectedRoomId);
    // first look for mentions
    for (int i = 0; i < (int)roomlistmodel->roomids.size(); i++) {
        if (i == currentRoomIdx)
            continue;
        if (this->data(index(i, 0), RoomlistModel::HasLoudNotification).toBool()) {
            roomWithMention = i;
            break;
        }
        if (roomWithNotification == -1 &&
            this->data(index(i, 0), RoomlistModel::UnreadCount).toInt() > 0) {
            roomWithNotification = i;
            // don't break, we must continue looking for rooms with mentions
        }
        if (roomWithNotification == -1 && roomWithUnreadMessage == -1 &&
            this->data(index(i, 0), RoomlistModel::HasUnreadMessages).toBool() &&
            !this->data(index(i, 0), RoomlistModel::Tags)
               .toStringList()
               .contains(QStringLiteral("m.lowpriority"))) {
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
    const auto selectedRoomId = this->currentRoomId();

    if (!selectedRoomId.isEmpty()) {
        int idx = roomidToIndex(selectedRoomId);
        idx++;
        if (idx < rowCount()) {
            setCurrentRoom(data(index(idx, 0), RoomlistModel::Roles::RoomId).toString());
        }
    }
}

void
FilteredRoomlistModel::previousRoom()
{
    const auto selectedRoomId = this->currentRoomId();

    if (!selectedRoomId.isEmpty()) {
        int idx = roomidToIndex(selectedRoomId);
        idx--;
        if (idx >= 0) {
            setCurrentRoom(data(index(idx, 0), RoomlistModel::Roles::RoomId).toString());
        }
    }
}
