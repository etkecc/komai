// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CommunitiesModel.h"

#include <algorithm>
#include <set>

#include "DirectChatResolver.h"
#include "cache/Cache.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"

namespace {
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
    directMessages_.clear();
    tagBadgeCache.clear();
    for (auto &f : fixedFilters_) {
        f.unreadRoomCount = 0;
        f.hasHighlight    = false;
    }
    hasPeopleRooms_ = false;
    hasBotRooms_    = false;
    hasGroupRooms_  = false;

    const auto *window = MainWindow::instance();
    if (window && window->matrixBackendHandleId() != 0) {
        computeFilterBadges();
        endResetModel();

        emit tagsChanged();
        emit globalExcludesChanged();
        emit containsSubspacesChanged();

        setCurrentFilterId(UserSettings::instance()->currentFilterId());
        return;
    }

    {
        auto e = cache::getAccountData(mtx::events::EventType::Direct);
        if (e) {
            if (auto event =
                  std::get_if<mtx::events::AccountDataEvent<mtx::events::account_data::Direct>>(
                    &e.value())) {
                directMessages_.clear();
                for (const auto &[userId, roomIds] : event->content.user_to_rooms)
                    for (const auto &roomId : roomIds)
                        directMessages_.push_back(roomId);
            }
        }
    }

    std::set<std::string> ts;

    std::set<std::string> isSpace;
    std::map<std::string, std::set<std::string>> spaceChilds;
    std::map<std::string, std::set<std::string>> spaceParents;

    auto infos = cache::roomInfo();
    for (auto it = infos.begin(); it != infos.end(); ++it) {
        if (it.value().is_space) {
            spaces_[it.key()] = it.value();
            isSpace.insert(it.key().toStdString());
        } else {
            for (const auto &t : it.value().tags) {
                if (t.find("u.") == 0 || t.find("m." == 0)) {
                    ts.insert(t);
                }
            }
        }

        bool isBot    = DirectChatResolver::instance().isBotRoom(it.key());
        bool isDirect = DirectChatResolver::instance().isDirectChat(it.key());

        if (isBot)
            hasBotRooms_ = true;
        else if (isDirect)
            hasPeopleRooms_ = true;
        else if (!it.value().is_space)
            hasGroupRooms_ = true;
    }

    // NOTE(Nico): We build a forrest from the Directed Cyclic(!) Graph of spaces. To do that we
    // start with orphan spaces at the top. This leaves out some space circles, but there is no good
    // way to break that cycle imo anyway. Then we carefully walk a tree down from each root in our
    // forrest, carefully checking not to run in a circle and get lost forever.
    // TODO(Nico): Optimize this. We can do this with a lot fewer allocations and checks.
    for (const auto &space : isSpace) {
        spaceParents[space];
        for (const auto &p : cache::getParentRoomIds(space)) {
            spaceParents[space].insert(p);
            spaceChilds[p].insert(space);
        }
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
    for (const auto &roomId : sync.leftRoomIds) {
        if (spaces_.count(roomId))
            tagsUpdated = true;
        const auto filterId = QStringLiteral("space:%1").arg(roomId);
        if (globalExcludedFilterIds_.contains(filterId)) {
            globalExcludedFilterIds_.removeAll(filterId);
            UserSettings::instance()->setGlobalExcludes(globalExcludedFilterIds_);
            tagsUpdated = true;
        }
    }
    if (sync.directChatsChanged) {
        directMessages_.clear();
        const auto *window = MainWindow::instance();
        if (!(window && window->matrixBackendHandleId() != 0)) {
            auto event = cache::getAccountData(mtx::events::EventType::Direct);
            if (event) {
                if (auto direct =
                      std::get_if<mtx::events::AccountDataEvent<mtx::events::account_data::Direct>>(
                        &event.value())) {
                    for (const auto &[userId, roomIds] : direct->content.user_to_rooms)
                        for (const auto &roomId : roomIds)
                            directMessages_.push_back(roomId);
                }
            }
        }
        tagsUpdated = true;
    }

    if (tagsUpdated)
        initializeSidebar();
}
