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
    for (const auto &space : spacetree.children) {
        addChildren(spacetree, path, space.first, spaceChilds);
    }

    // NOTE(Nico): This flattens the tree into a list, preserving the depth at each element.
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
CommunitiesModel::sync(const komai::SyncUpdate &sync)
{
    bool tagsUpdated = false;

    for (const auto &roomUpdate : sync.joinedRooms) {
        if (roomUpdate.tagsChanged || roomUpdate.spaceInfoChanged ||
            roomUpdate.ownMembershipChanged)
            tagsUpdated = true;
    }
    if (!sync.leftRoomIds.empty()) {
        for (const auto &roomId : sync.leftRoomIds) {
            const auto filterId = QStringLiteral("space:%1").arg(roomId);
            if (globalExcludedFilterIds_.contains(filterId)) {
                globalExcludedFilterIds_.removeAll(filterId);
                UserSettings::instance()->setGlobalExcludes(globalExcludedFilterIds_);
            }
        }
        tagsUpdated = true;
    }
    if (sync.directChatsChanged)
        tagsUpdated = true;

    if (tagsUpdated)
        initializeSidebar();
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
        roles.contains(RoomlistModel::NotificationCount) || roles.contains(RoomlistModel::Tags) ||
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
