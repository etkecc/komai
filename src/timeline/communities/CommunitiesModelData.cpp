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

    if (index.row() >= 0 && index.row() < kFixedRowCount) {
        const auto &f = fixedFilters_[index.row()];
        switch (role) {
        case CommunitiesModel::Roles::AvatarUrl:
            return f.icon;
        case CommunitiesModel::Roles::DisplayName:
            return fixedFilterDisplayName(index.row());
        case CommunitiesModel::Roles::Tooltip:
            return fixedFilterTooltip(index.row());
        case CommunitiesModel::Roles::Collapsed:
        case CommunitiesModel::Roles::Collapsible:
            return false;
        case CommunitiesModel::Roles::Hidden:
            return f.id.isEmpty() ? false : hiddenTagIds_.contains(f.id);
        case CommunitiesModel::Roles::Parent:
            return QString();
        case CommunitiesModel::Roles::Depth:
            return 0;
        case CommunitiesModel::Roles::Id:
            return f.id;
        case CommunitiesModel::Roles::UnreadMessages:
            return f.unreadRoomCount;
        case CommunitiesModel::Roles::HasLoudNotification:
            return f.hasHighlight;
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
                count += spaceOrder_.tree[i].unreadRoomCount;
            return count;
        }
        case CommunitiesModel::Roles::HasLoudNotification: {
            auto end = spaceOrder_.lastChild(index.row() - kFixedRowCount);
            for (int i = index.row() - kFixedRowCount; i <= end; i++)
                if (spaceOrder_.tree[i].hasHighlight)
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
            if (auto it = tagBadgeCache.find(tag); it != tagBadgeCache.end())
                return it->second.unreadRoomCount;
            else
                return 0;
        case CommunitiesModel::Roles::HasLoudNotification:
            if (auto it = tagBadgeCache.find(tag); it != tagBadgeCache.end())
                return it->second.hasHighlight;
            else
                return false;
        }
    }
    return QVariant();
}

QString
CommunitiesModel::fixedFilterDisplayName(int row) const
{
    switch (row) {
    case kRowAllRooms:
        return tr("All rooms");
    case kRowPeople:
        return tr("People");
    case kRowBots:
        return tr("Bots");
    case kRowGroups:
        return tr("Groups");
    default:
        return {};
    }
}

QString
CommunitiesModel::fixedFilterTooltip(int row) const
{
    switch (row) {
    case kRowAllRooms:
        return tr("Shows all rooms without filtering.");
    case kRowPeople:
        return tr("Show direct chats with people, excluding bots.");
    case kRowBots:
        return tr("Show direct chats with bots.");
    case kRowGroups:
        return tr("Show group rooms (non-direct chats).");
    default:
        return {};
    }
}

FilteredCommunitiesModel::FilteredCommunitiesModel(CommunitiesModel *model, QObject *parent)
  : QSortFilterProxyModel(parent)
{
    setSourceModel(model);
    setDynamicSortFilter(true);
    sort(0);

    auto settings = UserSettings::instance();
    if (settings) {
        for (auto sig : {
               &UserSettings::sidebarsCommunitiesFilterFavouritesChanged,
               &UserSettings::sidebarsCommunitiesFilterPeopleChanged,
               &UserSettings::sidebarsCommunitiesFilterLowPriorityChanged,
               &UserSettings::sidebarsCommunitiesFilterBotsChanged,
               &UserSettings::sidebarsCommunitiesFilterGroupsChanged,
               &UserSettings::sidebarsCommunitiesFilterServerNoticesChanged,
             })
            connect(settings.get(), sig, this, &FilteredCommunitiesModel::invalidateFilter);
    }
}

namespace {
enum Categories
{
    World,
    Favourites,
    People,
    Bots,
    Groups,
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
    else if (tagId == QLatin1String("people"))
        return People;
    else if (tagId == QLatin1String("bot"))
        return Bots;
    else if (tagId == QLatin1String("group"))
        return Groups;
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

    // Check community filter settings for well-known filter rows.
    // Each filter is hidden when its setting is off OR when no rooms match.
    auto settings = UserSettings::instance();
    if (settings) {
        const auto tagId = m->data(m->index(sourceRow), CommunitiesModel::Roles::Id).toString();
        if (tagId == QLatin1String("people") &&
            (!settings->sidebarsCommunitiesFilterPeople() || !m->hasPeopleRooms_))
            return false;
        if (tagId == QLatin1String("bot") &&
            (!settings->sidebarsCommunitiesFilterBots() || !m->hasBotRooms_))
            return false;
        if (tagId == QLatin1String("group") &&
            (!settings->sidebarsCommunitiesFilterGroups() || !m->hasGroupRooms_))
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
