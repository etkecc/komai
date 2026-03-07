// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CommunitiesModel.h"

#include "RoomlistModel.h"
#include "cache/Cache.h"
#include "settings/ui/facade/UserSettingsPage.h"

CommunitiesModel::CommunitiesModel(QObject *parent)
  : QAbstractListModel(parent)
  , globalExcludedFilterIds_{UserSettings::instance()->globalExcludes()}
  , badgesHiddenFilterIds_{UserSettings::instance()->badgesHiddenFilters()}
{
    instance_ = this;

    cache::onRoomReadStatusChanged(
      this, [this](const std::map<QString, bool> &) { recomputeFilterBadges(); });

    connect(
      this, &CommunitiesModel::globalExcludesChanged, this, [this]() { recomputeFilterBadges(); });
}

QHash<int, QByteArray>
CommunitiesModel::roleNames() const
{
    return {
      {AvatarUrl, "avatarUrl"},
      {DisplayName, "displayName"},
      {Tooltip, "tooltip"},
      {Collapsed, "collapsed"},
      {Collapsible, "collapsible"},
      {Hidden, "hidden"},
      {Depth, "depth"},
      {Id, "id"},
      {UnreadMessages, "unreadMessages"},
      {HasLoudNotification, "hasLoudNotification"},
      {BadgesHidden, "badgesHidden"},
    };
}

void
CommunitiesModel::FlatTree::storeCollapsed()
{
    QStringList ids;
    ids.reserve(static_cast<int>(tree.size()));

    for (const auto &e : tree) {
        if (e.collapsed)
            ids.push_back(e.id);
    }

    UserSettings::instance()->setCollapsedSpaces(ids);
}
void
CommunitiesModel::FlatTree::restoreCollapsed()
{
    const QStringList ids = UserSettings::instance()->collapsedSpaces();

    for (auto &e : tree) {
        if (ids.contains(e.id))
            e.collapsed = true;
    }
}

void
CommunitiesModel::computeFilterBadges()
{
    for (auto &f : fixedFilters_) {
        f.unreadRoomCount = 0;
        f.hasHighlight    = false;
    }
    for (auto &space : spaceOrder_.tree) {
        space.unreadRoomCount = 0;
        space.hasHighlight    = false;
    }
    tagBadgeCache.clear();

    auto *filtered = FilteredRoomlistModel::instance();
    if (!filtered)
        return;

    // Build list of all community filter IDs and ask FilteredRoomlistModel
    // to compute badges using its own filterAcceptsRow logic — no duplication.
    QStringList communityIds;
    for (const auto &f : fixedFilters_)
        communityIds.append(f.id);
    for (const auto &s : spaceOrder_.tree)
        communityIds.append(QStringLiteral("space:") + s.id);
    for (const auto &t : std::as_const(tags_))
        communityIds.append(QStringLiteral("tag:") + t);

    const auto badges = filtered->computeFilterBadges(communityIds);

    // Distribute results to fixed filters.
    for (int i = 0; i < kFixedRowCount; i++) {
        if (auto it = badges.find(fixedFilters_[i].id); it != badges.end()) {
            fixedFilters_[i].unreadRoomCount = it->unreadCount;
            fixedFilters_[i].hasHighlight    = it->hasHighlight;
        }
    }

    // Distribute results to spaces.
    for (auto &space : spaceOrder_.tree) {
        if (auto it = badges.find(QStringLiteral("space:") + space.id); it != badges.end()) {
            space.unreadRoomCount = it->unreadCount;
            space.hasHighlight    = it->hasHighlight;
        }
    }

    // Distribute results to tags.
    for (const auto &t : std::as_const(tags_)) {
        if (auto it = badges.find(QStringLiteral("tag:") + t); it != badges.end()) {
            tagBadgeCache[t] = {it->unreadCount, it->hasHighlight};
        }
    }
}

void
CommunitiesModel::recomputeFilterBadges()
{
    computeFilterBadges();
    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1), {UnreadMessages, HasLoudNotification});
}

void
CommunitiesModel::clear()
{
    beginResetModel();
    tags_.clear();
    endResetModel();
    resetCurrentFilterId();

    emit tagsChanged();
}

void
CommunitiesModel::setCurrentFilterId(const QString &filterId)
{
    if (currentFilterId_ == filterId)
        return;

    if (filterId.startsWith(QLatin1String("tag:"))) {
        auto tag = filterId.mid(4);
        for (const auto &t : std::as_const(tags_)) {
            if (t == tag) {
                this->currentFilterId_ = filterId;
                UserSettings::instance()->setCurrentFilterId(filterId);
                emit currentFilterIdChanged(currentFilterId_);
                return;
            }
        }
    } else if (filterId.startsWith(QLatin1String("space:"))) {
        auto tag = filterId.mid(6);
        for (const auto &t : spaceOrder_.tree) {
            if (t.id == tag) {
                this->currentFilterId_ = filterId;
                UserSettings::instance()->setCurrentFilterId(filterId);
                emit currentFilterIdChanged(currentFilterId_);
                return;
            }
        }
    } else if (filterId == QLatin1String("people") || filterId == QLatin1String("bot") ||
               filterId == QLatin1String("group")) {
        this->currentFilterId_ = filterId;
        UserSettings::instance()->setCurrentFilterId(filterId);
        emit currentFilterIdChanged(currentFilterId_);
        return;
    }

    this->currentFilterId_ = QLatin1String("");
    UserSettings::instance()->setCurrentFilterId(filterId);
    emit currentFilterIdChanged(currentFilterId_);
}

bool
CommunitiesModel::trySwitchToSpace(const QString &tag)
{
    for (const auto &t : spaceOrder_.tree) {
        if (t.id == tag) {
            this->currentFilterId_ = "space:" + tag;
            UserSettings::instance()->setCurrentFilterId(tag);
            emit currentFilterIdChanged(currentFilterId_);
            return true;
        }
    }

    return false;
}

void
CommunitiesModel::toggleGlobalExclude(QString filterId)
{
    if (globalExcludedFilterIds_.contains(filterId))
        globalExcludedFilterIds_.removeOne(filterId);
    else
        globalExcludedFilterIds_.push_back(filterId);

    // sanity check to remove stale spaces
    globalExcludedFilterIds_.removeIf([this](const QString &value) {
        return value.startsWith("space:") && !spaces_.contains(value.mid(6));
    });

    UserSettings::instance()->setGlobalExcludes(globalExcludedFilterIds_);

    if (filterId.startsWith(QLatin1String("tag:"))) {
        auto idx = tags_.indexOf(filterId.mid(4));
        if (idx != -1)
            emit dataChanged(index(idx + kFixedRowCount + spaceOrder_.size()),
                             index(idx + kFixedRowCount + spaceOrder_.size()),
                             {Hidden});
    } else if (filterId.startsWith(QLatin1String("space:"))) {
        auto idx = spaceOrder_.indexOf(filterId.mid(6));
        if (idx != -1)
            emit dataChanged(index(idx + kFixedRowCount), index(idx + kFixedRowCount), {Hidden});
    } else if (filterId == QLatin1String("people")) {
        emit dataChanged(index(kRowPeople), index(kRowPeople), {Hidden});
    } else if (filterId == QLatin1String("bot")) {
        emit dataChanged(index(kRowBots), index(kRowBots), {Hidden});
    } else if (filterId == QLatin1String("group")) {
        emit dataChanged(index(kRowGroups), index(kRowGroups), {Hidden});
    }

    emit globalExcludesChanged();
}

void
CommunitiesModel::toggleFilterBadges(QString filterId)
{
    if (filterId.isEmpty())
        filterId = QStringLiteral("global");

    if (badgesHiddenFilterIds_.contains(filterId))
        badgesHiddenFilterIds_.removeOne(filterId);
    else
        badgesHiddenFilterIds_.push_back(filterId);
    UserSettings::instance()->setBadgesHiddenFilters(badgesHiddenFilterIds_);

    if (filterId.startsWith(QLatin1String("tag:"))) {
        auto idx = tags_.indexOf(filterId.mid(4));
        if (idx != -1)
            emit dataChanged(index(idx + kFixedRowCount + spaceOrder_.size()),
                             index(idx + kFixedRowCount + spaceOrder_.size()));
    } else if (filterId.startsWith(QLatin1String("space:"))) {
        auto idx = spaceOrder_.indexOf(filterId.mid(6));
        if (idx != -1)
            emit dataChanged(index(idx + kFixedRowCount), index(idx + kFixedRowCount));
    } else if (filterId == QLatin1String("people")) {
        emit dataChanged(index(kRowPeople), index(kRowPeople));
    } else if (filterId == QLatin1String("bot")) {
        emit dataChanged(index(kRowBots), index(kRowBots));
    } else if (filterId == QLatin1String("group")) {
        emit dataChanged(index(kRowGroups), index(kRowGroups));
    } else if (filterId == QLatin1String("global")) {
        emit dataChanged(index(kRowAllRooms), index(kRowAllRooms));
    }

    emit badgesHiddenFiltersChanged();
}

bool
CommunitiesModel::areFilterBadgesHidden(const QString &filterId) const
{
    if (filterId.isEmpty())
        return badgesHiddenFilterIds_.contains(QStringLiteral("global"));
    return badgesHiddenFilterIds_.contains(filterId);
}

bool
CommunitiesModel::isGlobalExcluded(const QString &filterId) const
{
    return globalExcludedFilterIds_.contains(filterId);
}

#include "moc_CommunitiesModel.cpp"
