// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CommunitiesModel.h"

#include <algorithm>
#include <set>

#include "RoomlistModel.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {
RoomlistModel *
activeRoomlistModel()
{
    auto *filteredRooms = FilteredRoomlistModel::instance();
    return filteredRooms ? qobject_cast<RoomlistModel *>(filteredRooms->sourceModel()) : nullptr;
}

struct temptree
{
    std::map<std::string, temptree> children;

    void insert(const std::vector<std::string> &parents, const std::string &child)
    {
        temptree *t = this;
        for (const auto &e : parents)
            t = &t->children[e];
        t->children[child];
    }

    void flatten(CommunitiesModel::FlatTree &to,
                 const std::map<QString, RoomInfo> &spaces,
                 int depth = 0) const
    {
        std::vector<std::pair<std::string, const temptree *>> sorted;
        sorted.reserve(children.size());
        for (const auto &[child, subtree] : children)
            sorted.emplace_back(child, &subtree);

        std::sort(sorted.begin(), sorted.end(), [&spaces](const auto &a, const auto &b) {
            auto aId = QString::fromStdString(a.first);
            auto bId = QString::fromStdString(b.first);
            auto aIt = spaces.find(aId);
            auto bIt = spaces.find(bId);

            QString aName = (aIt != spaces.end() && !aIt->second.name.empty())
                              ? QString::fromStdString(aIt->second.name)
                              : aId;
            QString bName = (bIt != spaces.end() && !bIt->second.name.empty())
                              ? QString::fromStdString(bIt->second.name)
                              : bId;

            int cmp = aName.compare(bName, Qt::CaseInsensitive);
            if (cmp != 0)
                return cmp < 0;
            return a.first < b.first;
        });

        for (const auto &[child, subtree] : sorted) {
            to.tree.push_back({QString::fromStdString(child), depth, 0, false, false});
            subtree->flatten(to, spaces, depth + 1);
        }
    }
};

void
addChildren(temptree &t,
            std::vector<std::string> path,
            std::string child,
            const std::map<std::string, std::set<std::string>> &children)
{
    if (std::find(path.begin(), path.end(), child) != path.end())
        return;

    path.push_back(child);

    if (children.count(child)) {
        for (const auto &c : children.at(child)) {
            t.insert(path, c);
            addChildren(t, path, c, children);
        }
    }
}
} // namespace

CommunitiesModel::CommunitiesModel(QObject *parent)
  : QAbstractListModel(parent)
  , globalExcludedFilterIds_{UserSettings::instance()->globalExcludes()}
  , unreadIndicatorsHiddenFilterIds_{UserSettings::instance()->unreadIndicatorsHiddenFilters()}
  , hiddenSpaceIds_{UserSettings::instance()->hiddenSpaces()}
{
    instance_ = this;

    connect(
      this, &CommunitiesModel::globalExcludesChanged, this, [this]() { recomputeFilterBadges(); });
    // Hidden status now affects unread/highlight rollups onto ancestor space rows
    // (hidden subspaces no longer contribute), so rebuild badges on toggle.
    connect(
      this, &CommunitiesModel::hiddenSpacesChanged, this, [this]() { recomputeFilterBadges(); });
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
      {UnreadIndicatorsHidden, "unreadIndicatorsHidden"},
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

bool
CommunitiesModel::hasRoomsForFixedFilter(const QString &filterId) const
{
    auto *filtered = FilteredRoomlistModel::instance();
    auto *model    = filtered ? filtered->sourceModel() : nullptr;
    if (!model)
        return false;

    const int rows = model->rowCount();
    for (int row = 0; row < rows; ++row) {
        const auto idx = model->index(row, 0, QModelIndex());

        if (model->data(idx, RoomlistModel::IsPreview).toBool() ||
            model->data(idx, RoomlistModel::IsSpace).toBool() ||
            model->data(idx, RoomlistModel::IsInvite).toBool()) {
            continue;
        }

        const bool isDirect = model->data(idx, RoomlistModel::IsDirect).toBool();
        const bool isBot    = model->data(idx, RoomlistModel::IsBotRoom).toBool();

        if (filterId == QLatin1String("people") && isDirect && !isBot)
            return true;
        if (filterId == QLatin1String("bot") && isBot)
            return true;
        if (filterId == QLatin1String("group") && !isDirect)
            return true;
    }

    return false;
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
CommunitiesModel::initializeSidebar()
{
    beginResetModel();
    tags_.clear();
    spaceOrder_.tree.clear();
    spaces_.clear();
    tagBadgeCache.clear();
    for (auto &f : fixedFilters_) {
        f.unreadRoomCount = 0;
        f.hasHighlight    = false;
    }
    hasPeopleRooms_ = false;
    hasBotRooms_    = false;
    hasGroupRooms_  = false;

    std::set<std::string> ts;
    std::set<std::string> isSpace;
    std::map<std::string, std::set<std::string>> spaceChilds;
    std::map<std::string, std::set<std::string>> spaceParents;
    auto *roomlistModel = activeRoomlistModel();
    if (roomlistModel) {
        const int rows = roomlistModel->rowCount();
        for (int row = 0; row < rows; ++row) {
            const auto idx    = roomlistModel->index(row, 0, QModelIndex());
            const auto roomId = roomlistModel->data(idx, RoomlistModel::RoomId).toString();
            if (roomId.isEmpty())
                continue;

            const bool isInvite    = roomlistModel->data(idx, RoomlistModel::IsInvite).toBool();
            const bool isBotRoom   = roomlistModel->data(idx, RoomlistModel::IsBotRoom).toBool();
            const bool isDirect    = roomlistModel->data(idx, RoomlistModel::IsDirect).toBool();
            const bool isSpaceRoom = roomlistModel->data(idx, RoomlistModel::IsSpace).toBool();

            if (isInvite)
                continue;

            if (isBotRoom)
                hasBotRooms_ = true;
            else if (isDirect)
                hasPeopleRooms_ = true;
            else if (!isSpaceRoom)
                hasGroupRooms_ = true;

            if (!isSpaceRoom) {
                const auto tags = roomlistModel->data(idx, RoomlistModel::Tags).toStringList();
                for (const auto &tag : tags) {
                    if (tag.startsWith(u"u.") || tag.startsWith(u"m."))
                        ts.insert(tag.toStdString());
                }
                continue;
            }

            RoomInfo info{};
            info.name = roomlistModel->data(idx, RoomlistModel::RoomName).toString().toStdString();
            info.avatar_url =
              roomlistModel->data(idx, RoomlistModel::AvatarUrl).toString().toStdString();
            info.is_space   = true;
            spaces_[roomId] = info;
            isSpace.insert(roomId.toStdString());
        }

        for (int row = 0; row < rows; ++row) {
            const auto idx    = roomlistModel->index(row, 0, QModelIndex());
            const auto roomId = roomlistModel->data(idx, RoomlistModel::RoomId).toString();
            if (roomId.isEmpty() || !roomlistModel->data(idx, RoomlistModel::IsSpace).toBool())
                continue;

            const auto childId = roomId.toStdString();
            spaceParents[childId];
            const auto parents =
              roomlistModel->data(idx, RoomlistModel::ParentSpaces).toStringList();
            for (const auto &parentId : parents) {
                const auto parentIdStd = parentId.toStdString();
                if (!isSpace.count(parentIdStd))
                    continue;

                spaceParents[childId].insert(parentIdStd);
                spaceChilds[parentIdStd].insert(childId);
            }
        }

        connect(roomlistModel,
                &QAbstractItemModel::dataChanged,
                this,
                &CommunitiesModel::handleRoomlistDataChanged,
                Qt::UniqueConnection);
        connect(roomlistModel,
                &QAbstractItemModel::modelReset,
                this,
                &CommunitiesModel::handleRoomlistModelReset,
                Qt::UniqueConnection);
        connect(roomlistModel,
                &QAbstractItemModel::rowsInserted,
                this,
                &CommunitiesModel::handleRoomlistRowsInserted,
                Qt::UniqueConnection);
        connect(roomlistModel,
                &QAbstractItemModel::rowsRemoved,
                this,
                &CommunitiesModel::handleRoomlistRowsRemoved,
                Qt::UniqueConnection);
    }

    temptree spacetree;
    std::vector<std::string> path;
    for (const auto &space : isSpace) {
        if (!spaceParents[space].empty())
            continue;

        spacetree.children[space] = {};
    }
    for (const auto &space : spacetree.children)
        addChildren(spacetree, path, space.first, spaceChilds);

    spacetree.flatten(spaceOrder_, spaces_);

    for (const auto &t : ts)
        tags_.push_back(QString::fromStdString(t));

    spaceOrder_.restoreCollapsed();

    computeFilterBadges();

    endResetModel();

    emit tagsChanged();
    emit globalExcludesChanged();
    emit containsSubspacesChanged();

    setCurrentFilterId(UserSettings::instance()->currentFilterId());
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
CommunitiesModel::toggleFilterUnreadIndicators(QString filterId)
{
    if (filterId.isEmpty())
        filterId = QStringLiteral("global");

    if (unreadIndicatorsHiddenFilterIds_.contains(filterId))
        unreadIndicatorsHiddenFilterIds_.removeOne(filterId);
    else
        unreadIndicatorsHiddenFilterIds_.push_back(filterId);
    UserSettings::instance()->setUnreadIndicatorsHiddenFilters(unreadIndicatorsHiddenFilterIds_);

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

    emit unreadIndicatorsHiddenFiltersChanged();
}

void
CommunitiesModel::handleRoomlistDataChanged(const QModelIndex &topLeft,
                                            const QModelIndex &bottomRight,
                                            const QList<int> &roles)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);

    if (roles.isEmpty() || roles.contains(RoomlistModel::HasUnreadMessages) ||
        roles.contains(RoomlistModel::HasLoudNotification) ||
        roles.contains(RoomlistModel::UnreadCount) || roles.contains(RoomlistModel::Tags) ||
        roles.contains(RoomlistModel::ParentSpaces) || roles.contains(RoomlistModel::IsDirect) ||
        roles.contains(RoomlistModel::IsBotRoom) || roles.contains(RoomlistModel::IsSpace)) {
        recomputeFilterBadges();
    }
}

void
CommunitiesModel::handleRoomlistModelReset()
{
    initializeSidebar();
}

void
CommunitiesModel::handleRoomlistRowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    initializeSidebar();
}

void
CommunitiesModel::handleRoomlistRowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    initializeSidebar();
}

bool
CommunitiesModel::areFilterUnreadIndicatorsHidden(const QString &filterId) const
{
    if (filterId.isEmpty())
        return unreadIndicatorsHiddenFilterIds_.contains(QStringLiteral("global"));
    return unreadIndicatorsHiddenFilterIds_.contains(filterId);
}

bool
CommunitiesModel::isGlobalExcluded(const QString &filterId) const
{
    return globalExcludedFilterIds_.contains(filterId);
}

bool
CommunitiesModel::isSpaceHidden(const QString &spaceId) const
{
    // Accept both bare room IDs and "space:"-prefixed IDs
    if (spaceId.startsWith(QLatin1String("space:")))
        return hiddenSpaceIds_.contains(spaceId.mid(6));
    return hiddenSpaceIds_.contains(spaceId);
}

bool
CommunitiesModel::isSpaceEffectivelyHidden(const QString &spaceId) const
{
    const auto bareId = spaceId.startsWith(QLatin1String("space:")) ? spaceId.mid(6) : spaceId;

    auto idx = spaceOrder_.indexOf(bareId);
    if (idx < 0)
        return hiddenSpaceIds_.contains(bareId);

    while (idx >= 0) {
        if (hiddenSpaceIds_.contains(spaceOrder_.tree[idx].id))
            return true;
        if (spaceOrder_.tree[idx].depth == 0)
            break;
        idx = spaceOrder_.parent(idx);
    }
    return false;
}

bool
CommunitiesModel::isSpaceEffectivelyExcludedFromAllRooms(const QString &spaceId,
                                                         const QString &stopAtBareId) const
{
    const auto bareId = spaceId.startsWith(QLatin1String("space:")) ? spaceId.mid(6) : spaceId;

    auto idx = spaceOrder_.indexOf(bareId);
    if (idx < 0)
        return globalExcludedFilterIds_.contains(QStringLiteral("space:") + bareId);

    while (idx >= 0) {
        if (!stopAtBareId.isEmpty() && spaceOrder_.tree[idx].id == stopAtBareId)
            return false;
        if (globalExcludedFilterIds_.contains(QStringLiteral("space:") + spaceOrder_.tree[idx].id))
            return true;
        if (spaceOrder_.tree[idx].depth == 0)
            break;
        idx = spaceOrder_.parent(idx);
    }
    return false;
}

void
CommunitiesModel::toggleSpaceHidden(const QString &spaceId)
{
    auto bareId = spaceId.startsWith(QLatin1String("space:")) ? spaceId.mid(6) : spaceId;

    if (hiddenSpaceIds_.contains(bareId))
        hiddenSpaceIds_.removeOne(bareId);
    else
        hiddenSpaceIds_.push_back(bareId);

    // Remove stale entries for spaces that no longer exist
    hiddenSpaceIds_.removeIf([this](const QString &id) { return !spaces_.contains(id); });

    UserSettings::instance()->setHiddenSpaces(hiddenSpaceIds_);

    // Notify the filtered proxy to re-evaluate visibility
    auto idx = spaceOrder_.indexOf(bareId);
    if (idx != -1) {
        auto lastChild = spaceOrder_.lastChild(idx);
        emit dataChanged(index(idx + kFixedRowCount), index(lastChild + kFixedRowCount), {Hidden});
    }

    emit hiddenSpacesChanged();
}

QVariantList
CommunitiesModel::spaceEntries() const
{
    QVariantList result;
    result.reserve(spaceOrder_.size());

    for (const auto &elem : spaceOrder_.tree) {
        if (elem.depth > 0)
            continue; // Only top-level spaces for settings; subspaces inherit parent visibility

        auto it = spaces_.find(elem.id);
        if (it == spaces_.end())
            continue;

        QVariantMap entry;
        entry[QStringLiteral("id")]        = QStringLiteral("space:") + elem.id;
        entry[QStringLiteral("name")]      = QString::fromStdString(it->second.name);
        entry[QStringLiteral("avatarUrl")] = QString::fromStdString(it->second.avatar_url);
        result.append(entry);
    }

    return result;
}

#include "moc_CommunitiesModel.cpp"
