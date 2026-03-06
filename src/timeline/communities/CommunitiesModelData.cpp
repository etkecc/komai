// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CommunitiesModel.h"

#include <utility>

#include "settings/ui/facade/UserSettingsPage.h"

bool
CommunitiesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != CommunitiesModel::Collapsed)
        return false;
    else if (index.row() >= kFixedRowCount || index.row() - kFixedRowCount < spaceOrder_.size()) {
        spaceOrder_.tree.at(index.row() - kFixedRowCount).collapsed = value.toBool();

        const auto cindex = spaceOrder_.lastChild(index.row() - kFixedRowCount);
        emit dataChanged(index, this->index(cindex + kFixedRowCount), {Collapsed, Qt::DisplayRole});
        spaceOrder_.storeCollapsed();
        return true;
    } else
        return false;
}

QVariant
CommunitiesModel::data(const QModelIndex &index, int role) const
{
    if (role == CommunitiesModel::Roles::Muted) {
        if (index.row() == kRowAllRooms)
            return mutedTagIds_.contains(QStringLiteral("global"));
        else
            return mutedTagIds_.contains(data(index, CommunitiesModel::Roles::Id).toString());
    }

    if (index.row() == kRowAllRooms) {
        switch (role) {
        case CommunitiesModel::Roles::AvatarUrl:
            return QStringLiteral(":/icons/icons/ui/world.svg");
        case CommunitiesModel::Roles::DisplayName:
            return tr("All rooms");
        case CommunitiesModel::Roles::Tooltip:
            return tr("Shows all rooms without filtering.");
        case CommunitiesModel::Roles::Collapsed:
            return false;
        case CommunitiesModel::Roles::Collapsible:
            return false;
        case CommunitiesModel::Roles::Hidden:
            return false;
        case CommunitiesModel::Roles::Parent:
            return "";
        case CommunitiesModel::Roles::Depth:
            return 0;
        case CommunitiesModel::Roles::Id:
            return "";
        case CommunitiesModel::Roles::UnreadMessages:
            return (int)globalUnreads.notification_count;
        case CommunitiesModel::Roles::HasLoudNotification:
            return globalUnreads.highlight_count > 0;
        }
    } else if (index.row() == kRowDirectChats) {
        switch (role) {
        case CommunitiesModel::Roles::AvatarUrl:
            return QStringLiteral(":/icons/icons/ui/person.svg");
        case CommunitiesModel::Roles::DisplayName:
            return tr("Direct Chats");
        case CommunitiesModel::Roles::Tooltip:
            return tr("Show direct chats.");
        case CommunitiesModel::Roles::Collapsed:
            return false;
        case CommunitiesModel::Roles::Collapsible:
            return false;
        case CommunitiesModel::Roles::Hidden:
            return hiddenTagIds_.contains(QStringLiteral("dm"));
        case CommunitiesModel::Roles::Parent:
            return "";
        case CommunitiesModel::Roles::Depth:
            return 0;
        case CommunitiesModel::Roles::Id:
            return "dm";
        case CommunitiesModel::Roles::UnreadMessages:
            return (int)dmUnreads.notification_count;
        case CommunitiesModel::Roles::HasLoudNotification:
            return dmUnreads.highlight_count > 0;
        }
    } else if (index.row() == kRowBots) {
        switch (role) {
        case CommunitiesModel::Roles::AvatarUrl:
            return QStringLiteral(":/icons/icons/ui/robot-sparkle.svg");
        case CommunitiesModel::Roles::DisplayName:
            return tr("Bots");
        case CommunitiesModel::Roles::Tooltip:
            return tr("Show direct chats with bots.");
        case CommunitiesModel::Roles::Collapsed:
            return false;
        case CommunitiesModel::Roles::Collapsible:
            return false;
        case CommunitiesModel::Roles::Hidden:
            return hiddenTagIds_.contains(QStringLiteral("bot"));
        case CommunitiesModel::Roles::Parent:
            return "";
        case CommunitiesModel::Roles::Depth:
            return 0;
        case CommunitiesModel::Roles::Id:
            return "bot";
        case CommunitiesModel::Roles::UnreadMessages:
            return (int)botUnreads.notification_count;
        case CommunitiesModel::Roles::HasLoudNotification:
            return botUnreads.highlight_count > 0;
        }
    } else if (index.row() - kFixedRowCount < spaceOrder_.size()) {
        auto id = spaceOrder_.tree.at(index.row() - kFixedRowCount).id;
        switch (role) {
        case CommunitiesModel::Roles::AvatarUrl:
            return QString::fromStdString(spaces_.at(id).avatar_url);
        case CommunitiesModel::Roles::DisplayName:
        case CommunitiesModel::Roles::Tooltip:
            return QString::fromStdString(spaces_.at(id).name);
        case CommunitiesModel::Roles::Collapsed:
            return spaceOrder_.tree.at(index.row() - kFixedRowCount).collapsed;
        case CommunitiesModel::Roles::Collapsible: {
            auto idx = index.row() - kFixedRowCount;
            return idx != spaceOrder_.lastChild(idx);
        }
        case CommunitiesModel::Roles::Hidden:
            return hiddenTagIds_.contains("space:" + id);
        case CommunitiesModel::Roles::Parent: {
            if (auto p = spaceOrder_.parent(index.row() - kFixedRowCount); p >= 0)
                return spaceOrder_.tree[p].id;

            return "";
        }
        case CommunitiesModel::Roles::Depth:
            return spaceOrder_.tree.at(index.row() - kFixedRowCount).depth;
        case CommunitiesModel::Roles::Id:
            return "space:" + id;
        case CommunitiesModel::Roles::UnreadMessages: {
            int count = 0;
            auto end  = spaceOrder_.lastChild(index.row() - kFixedRowCount);
            for (int i = index.row() - kFixedRowCount; i <= end; i++)
                count +=
                  static_cast<int>(spaceOrder_.tree[i].notificationCounts.notification_count);
            return count;
        }
        case CommunitiesModel::Roles::HasLoudNotification: {
            auto end = spaceOrder_.lastChild(index.row() - kFixedRowCount);
            for (int i = index.row() - kFixedRowCount; i <= end; i++)
                if (spaceOrder_.tree[i].notificationCounts.highlight_count > 0)
                    return true;
            return false;
        }
        }
    } else if (index.row() - kFixedRowCount < tags_.size() + spaceOrder_.size()) {
        auto tag = tags_.at(index.row() - kFixedRowCount - spaceOrder_.size());
        if (tag == QLatin1String("m.favourite")) {
            switch (role) {
            case CommunitiesModel::Roles::AvatarUrl:
                return QStringLiteral(":/icons/icons/ui/star.svg");
            case CommunitiesModel::Roles::DisplayName:
                return tr("Favourites");
            case CommunitiesModel::Roles::Tooltip:
                return tr("Rooms you have favourited.");
            }
        } else if (tag == QLatin1String("m.lowpriority")) {
            switch (role) {
            case CommunitiesModel::Roles::AvatarUrl:
                return QStringLiteral(":/icons/icons/ui/lowprio.svg");
            case CommunitiesModel::Roles::DisplayName:
                return tr("Low Priority");
            case CommunitiesModel::Roles::Tooltip:
                return tr("Rooms with low priority.");
            }
        } else if (tag == QLatin1String("m.server_notice")) {
            switch (role) {
            case CommunitiesModel::Roles::AvatarUrl:
                return QStringLiteral(":/icons/icons/ui/tag.svg");
            case CommunitiesModel::Roles::DisplayName:
                return tr("Server Notices");
            case CommunitiesModel::Roles::Tooltip:
                return tr("Messages from your server or administrator.");
            }
        } else {
            switch (role) {
            case CommunitiesModel::Roles::AvatarUrl:
                return QStringLiteral(":/icons/icons/ui/tag.svg");
            case CommunitiesModel::Roles::DisplayName:
            case CommunitiesModel::Roles::Tooltip:
                return tag.mid(2);
            }
        }

        switch (role) {
        case CommunitiesModel::Roles::Hidden:
            return hiddenTagIds_.contains("tag:" + tag);
        case CommunitiesModel::Roles::Collapsed:
            return true;
        case CommunitiesModel::Roles::Collapsible:
            return false;
        case CommunitiesModel::Roles::Parent:
            return "";
        case CommunitiesModel::Roles::Depth:
            return 0;
        case CommunitiesModel::Roles::Id:
            return "tag:" + tag;
        case CommunitiesModel::Roles::UnreadMessages:
            if (auto it = tagNotificationCache.find(tag); it != tagNotificationCache.end())
                return (int)it->second.notification_count;
            else
                return 0;
        case CommunitiesModel::Roles::HasLoudNotification:
            if (auto it = tagNotificationCache.find(tag); it != tagNotificationCache.end())
                return it->second.highlight_count > 0;
            else
                return 0;
        }
    }
    return QVariant();
}

FilteredCommunitiesModel::FilteredCommunitiesModel(CommunitiesModel *model, QObject *parent)
  : QSortFilterProxyModel(parent)
{
    setSourceModel(model);
    setDynamicSortFilter(true);
    sort(0);

    auto settings = UserSettings::instance();
    if (settings) {
        connect(settings.get(),
                &UserSettings::sidebarsCommunitiesFilterDirectChatsChanged,
                this,
                &FilteredCommunitiesModel::invalidateFilter);
        connect(settings.get(),
                &UserSettings::sidebarsCommunitiesFilterFavouritesChanged,
                this,
                &FilteredCommunitiesModel::invalidateFilter);
        connect(settings.get(),
                &UserSettings::sidebarsCommunitiesFilterLowPriorityChanged,
                this,
                &FilteredCommunitiesModel::invalidateFilter);
        connect(settings.get(),
                &UserSettings::sidebarsCommunitiesFilterBotsChanged,
                this,
                &FilteredCommunitiesModel::invalidateFilter);
        connect(settings.get(),
                &UserSettings::sidebarsCommunitiesFilterServerNoticesChanged,
                this,
                &FilteredCommunitiesModel::invalidateFilter);
    }
}

namespace {
enum Categories
{
    World,
    Direct,
    Favourites,
    Bots,
    Server,
    LowPrio,
    Space,
    UserTag,
};

Categories
tagIdToCat(const QString &tagId)
{
    if (tagId.isEmpty())
        return World;
    else if (tagId == QLatin1String("dm"))
        return Direct;
    else if (tagId == QLatin1String("bot"))
        return Bots;
    else if (tagId == QLatin1String("tag:m.favourite"))
        return Favourites;
    else if (tagId == QLatin1String("tag:m.server_notice"))
        return Server;
    else if (tagId == QLatin1String("tag:m.lowpriority"))
        return LowPrio;
    else if (tagId.startsWith(QLatin1String("space:")))
        return Space;
    else
        return UserTag;
}
}

bool
FilteredCommunitiesModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QModelIndex const left_idx  = sourceModel()->index(left.row(), 0, QModelIndex());
    QModelIndex const right_idx = sourceModel()->index(right.row(), 0, QModelIndex());

    Categories leftCat = tagIdToCat(sourceModel()->data(left_idx, CommunitiesModel::Id).toString());
    Categories rightCat =
      tagIdToCat(sourceModel()->data(right_idx, CommunitiesModel::Id).toString());

    if (leftCat != rightCat)
        return leftCat < rightCat;

    if (leftCat == Space) {
        return left.row() < right.row();
    }

    QString leftName  = sourceModel()->data(left_idx, CommunitiesModel::DisplayName).toString();
    QString rightName = sourceModel()->data(right_idx, CommunitiesModel::DisplayName).toString();

    return leftName.compare(rightName, Qt::CaseInsensitive) < 0;
}

bool
FilteredCommunitiesModel::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    CommunitiesModel *m = qobject_cast<CommunitiesModel *>(this->sourceModel());
    if (!m)
        return true;

    // Check community filter settings for well-known filter rows
    auto settings = UserSettings::instance();
    if (settings) {
        const auto tagId = m->data(m->index(sourceRow), CommunitiesModel::Roles::Id).toString();
        if (tagId == QLatin1String("dm") && !settings->sidebarsCommunitiesFilterDirectChats())
            return false;
        if (tagId == QLatin1String("bot") &&
            (!settings->sidebarsCommunitiesFilterBots() || !m->hasBotRooms_))
            return false;
        if (tagId == QLatin1String("tag:m.favourite") &&
            !settings->sidebarsCommunitiesFilterFavourites())
            return false;
        if (tagId == QLatin1String("tag:m.server_notice") &&
            !settings->sidebarsCommunitiesFilterServerNotices())
            return false;
        if (tagId == QLatin1String("tag:m.lowpriority") &&
            !settings->sidebarsCommunitiesFilterLowPriority())
            return false;
    }

    if (sourceRow < CommunitiesModel::kFixedRowCount ||
        sourceRow - CommunitiesModel::kFixedRowCount >= m->spaceOrder_.size())
        return true;

    auto idx = sourceRow - CommunitiesModel::kFixedRowCount;

    while (idx >= 0 && m->spaceOrder_.tree[idx].depth > 0) {
        idx = m->spaceOrder_.parent(idx);

        if (idx >= 0 && m->spaceOrder_.tree.at(idx).collapsed)
            return false;
    }

    return true;
}
