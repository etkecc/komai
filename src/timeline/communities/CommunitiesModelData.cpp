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
    else if (index.row() >= 2 || index.row() - 2 < spaceOrder_.size()) {
        spaceOrder_.tree.at(index.row() - 2).collapsed = value.toBool();

        const auto cindex = spaceOrder_.lastChild(index.row() - 2);
        emit dataChanged(index, this->index(cindex + 2), {Collapsed, Qt::DisplayRole});
        spaceOrder_.storeCollapsed();
        return true;
    } else
        return false;
}

QVariant
CommunitiesModel::data(const QModelIndex &index, int role) const
{
    if (role == CommunitiesModel::Roles::Muted) {
        if (index.row() == 0)
            return mutedTagIds_.contains(QStringLiteral("global"));
        else
            return mutedTagIds_.contains(data(index, CommunitiesModel::Roles::Id).toString());
    }

    if (index.row() == 0) {
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
    } else if (index.row() == 1) {
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
    } else if (index.row() - 2 < spaceOrder_.size()) {
        auto id = spaceOrder_.tree.at(index.row() - 2).id;
        switch (role) {
        case CommunitiesModel::Roles::AvatarUrl:
            return QString::fromStdString(spaces_.at(id).avatar_url);
        case CommunitiesModel::Roles::DisplayName:
        case CommunitiesModel::Roles::Tooltip:
            return QString::fromStdString(spaces_.at(id).name);
        case CommunitiesModel::Roles::Collapsed:
            return spaceOrder_.tree.at(index.row() - 2).collapsed;
        case CommunitiesModel::Roles::Collapsible: {
            auto idx = index.row() - 2;
            return idx != spaceOrder_.lastChild(idx);
        }
        case CommunitiesModel::Roles::Hidden:
            return hiddenTagIds_.contains("space:" + id);
        case CommunitiesModel::Roles::Parent: {
            if (auto p = spaceOrder_.parent(index.row() - 2); p >= 0)
                return spaceOrder_.tree[p].id;

            return "";
        }
        case CommunitiesModel::Roles::Depth:
            return spaceOrder_.tree.at(index.row() - 2).depth;
        case CommunitiesModel::Roles::Id:
            return "space:" + id;
        case CommunitiesModel::Roles::UnreadMessages: {
            int count = 0;
            auto end  = spaceOrder_.lastChild(index.row() - 2);
            for (int i = index.row() - 2; i <= end; i++)
                count +=
                  static_cast<int>(spaceOrder_.tree[i].notificationCounts.notification_count);
            return count;
        }
        case CommunitiesModel::Roles::HasLoudNotification: {
            auto end = spaceOrder_.lastChild(index.row() - 2);
            for (int i = index.row() - 2; i <= end; i++)
                if (spaceOrder_.tree[i].notificationCounts.highlight_count > 0)
                    return true;
            return false;
        }
        }
    } else if (index.row() - 2 < tags_.size() + spaceOrder_.size()) {
        auto tag = tags_.at(index.row() - 2 - spaceOrder_.size());
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
    }
}

namespace {
enum Categories
{
    World,
    Direct,
    Favourites,
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
        if (tagId == QLatin1String("tag:m.favourite") &&
            !settings->sidebarsCommunitiesFilterFavourites())
            return false;
        if (tagId == QLatin1String("tag:m.lowpriority") &&
            !settings->sidebarsCommunitiesFilterLowPriority())
            return false;
    }

    if (sourceRow < 2 || sourceRow - 2 >= m->spaceOrder_.size())
        return true;

    auto idx = sourceRow - 2;

    while (idx >= 0 && m->spaceOrder_.tree[idx].depth > 0) {
        idx = m->spaceOrder_.parent(idx);

        if (idx >= 0 && m->spaceOrder_.tree.at(idx).collapsed)
            return false;
    }

    return true;
}
