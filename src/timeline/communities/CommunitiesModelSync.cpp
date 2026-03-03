// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CommunitiesModel.h"

#include <mtx/responses/common.hpp>
#include <set>

#include "cache/Cache.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "utils/Utils.h"

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

    void flatten(CommunitiesModel::FlatTree &to, int i = 0) const
    {
        for (const auto &[child, subtree] : children) {
            to.tree.push_back({QString::fromStdString(child), i, {}, false});
            subtree.flatten(to, i + 1);
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
    tagNotificationCache.clear();
    globalUnreads.notification_count = {};
    dmUnreads.notification_count     = {};

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

        for (const auto &t : it->tags) {
            auto tagId = QString::fromStdString(t);
            auto &tNs  = tagNotificationCache[tagId];
            tNs.notification_count += it->notification_count;
            tNs.highlight_count += it->highlight_count;
        }

        auto &e              = roomNotificationCache[it.key()];
        e.highlight_count    = it->highlight_count;
        e.notification_count = it->notification_count;
        globalUnreads.notification_count += it->notification_count;
        globalUnreads.highlight_count += it->highlight_count;

        if (std::find(begin(directMessages_), end(directMessages_), it.key().toStdString()) !=
            end(directMessages_)) {
            dmUnreads.notification_count += it->notification_count;
            dmUnreads.highlight_count += it->highlight_count;
        }
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
    spacetree.flatten(spaceOrder_);

    for (const auto &t : ts)
        tags_.push_back(QString::fromStdString(t));

    spaceOrder_.restoreCollapsed();

    for (auto &space : spaceOrder_.tree) {
        for (const auto &c : cache::getChildRoomIds(space.id.toStdString())) {
            const auto &counts = roomNotificationCache[QString::fromStdString(c)];
            space.notificationCounts.highlight_count += counts.highlight_count;
            space.notificationCounts.notification_count += counts.notification_count;
        }
    }

    endResetModel();

    emit tagsChanged();
    emit hiddenTagsChanged();
    emit containsSubspacesChanged();

    setCurrentTagId(UserSettings::instance()->currentTagId());
}

void
CommunitiesModel::sync(const mtx::responses::Sync &sync_)
{
    bool tagsUpdated  = false;
    const auto userid = utils::localUser().toStdString();

    for (const auto &[roomid, room] : sync_.rooms.join) {
        for (const auto &e : room.account_data.events)
            if (std::holds_alternative<
                  mtx::events::AccountDataEvent<mtx::events::account_data::Tags>>(e)) {
                tagsUpdated = true;
            }
        for (const auto &e : room.state.events) {
            if (std::holds_alternative<mtx::events::StateEvent<mtx::events::state::space::Child>>(
                  e) ||
                std::holds_alternative<mtx::events::StateEvent<mtx::events::state::space::Parent>>(
                  e))
                tagsUpdated = true;

            if (auto ev = std::get_if<mtx::events::StateEvent<mtx::events::state::Member>>(&e);
                ev && ev->state_key == userid)
                tagsUpdated = true;
        }
        for (const auto &e : room.timeline.events) {
            if (std::holds_alternative<mtx::events::StateEvent<mtx::events::state::space::Child>>(
                  e) ||
                std::holds_alternative<mtx::events::StateEvent<mtx::events::state::space::Parent>>(
                  e))
                tagsUpdated = true;

            if (auto ev = std::get_if<mtx::events::StateEvent<mtx::events::state::Member>>(&e);
                ev && ev->state_key == userid)
                tagsUpdated = true;
        }

        auto roomId            = QString::fromStdString(roomid);
        auto &oldUnreads       = roomNotificationCache[roomId];
        auto notificationCDiff = -static_cast<int64_t>(oldUnreads.notification_count) +
                                 static_cast<int64_t>(room.unread_notifications.notification_count);
        auto highlightCDiff = -static_cast<int64_t>(oldUnreads.highlight_count) +
                              static_cast<int64_t>(room.unread_notifications.highlight_count);

        auto applyDiff = [notificationCDiff,
                          highlightCDiff](mtx::responses::UnreadNotifications &n) {
            n.highlight_count    = static_cast<int64_t>(n.highlight_count) + highlightCDiff;
            n.notification_count = static_cast<int64_t>(n.notification_count) + notificationCDiff;
        };
        if (highlightCDiff || notificationCDiff) {
            // bool hidden = hiddenTagIds_.contains(roomId);
            applyDiff(globalUnreads);
            emit dataChanged(index(0),
                             index(0),
                             {
                               UnreadMessages,
                               HasLoudNotification,
                             });
            if (std::find(begin(directMessages_), end(directMessages_), roomid) !=
                end(directMessages_)) {
                applyDiff(dmUnreads);
                emit dataChanged(index(1),
                                 index(1),
                                 {
                                   UnreadMessages,
                                   HasLoudNotification,
                                 });
            }

            auto spaces = cache::getParentRoomIds(roomid);
            auto tags   = cache::singleRoomInfo(roomid).tags;

            for (const auto &t : tags) {
                auto tagId = QString::fromStdString(t);
                applyDiff(tagNotificationCache[tagId]);
                int idx = tags_.indexOf(tagId) + 2 + spaceOrder_.size();
                emit dataChanged(index(idx),
                                 index(idx),
                                 {
                                   UnreadMessages,
                                   HasLoudNotification,
                                 });
            }

            for (const auto &s : spaces) {
                auto spaceId = QString::fromStdString(s);

                for (int i = 0; i < spaceOrder_.size(); i++) {
                    if (spaceOrder_.tree[i].id != spaceId)
                        continue;

                    applyDiff(spaceOrder_.tree[i].notificationCounts);

                    int idx = i;
                    do {
                        emit dataChanged(index(idx + 2),
                                         index(idx + 2),
                                         {
                                           UnreadMessages,
                                           HasLoudNotification,
                                         });
                        idx = spaceOrder_.parent(idx);
                    } while (idx != -1);
                }
            }
        }

        roomNotificationCache[roomId] = room.unread_notifications;
    }
    for (const auto &[roomid, room] : sync_.rooms.leave) {
        (void)room;
        if (spaces_.count(QString::fromStdString(roomid)))
            tagsUpdated = true;
        if (hiddenTagIds_.contains(QString::fromStdString("space:" + roomid))) {
            hiddenTagIds_.removeAll(QString::fromStdString("space:" + roomid));
            UserSettings::instance()->setHiddenTags(hiddenTagIds_);
            tagsUpdated = true;
        }
    }
    for (const auto &e : sync_.account_data.events) {
        if (auto event =
              std::get_if<mtx::events::AccountDataEvent<mtx::events::account_data::Direct>>(&e)) {
            directMessages_.clear();
            for (const auto &[userId, roomIds] : event->content.user_to_rooms)
                for (const auto &roomId : roomIds)
                    directMessages_.push_back(roomId);
            tagsUpdated = true;
            break;
        }
    }

    if (tagsUpdated)
        initializeSidebar();
}
