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
  , hiddenTagIds_{UserSettings::instance()->hiddenTags()}
  , badgesHiddenTagIds_{UserSettings::instance()->badgesHiddenTags()}
{
    instance_ = this;

    cache::onRoomReadStatusChanged(
      this, [this](const std::map<QString, bool> &) { recomputeFilterBadges(); });

    connect(
      this, &CommunitiesModel::hiddenTagsChanged, this, [this]() { recomputeFilterBadges(); });
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
    QList<QStringList> elements;

    int depth = -1;

    QStringList current;
    elements.reserve(static_cast<int>(tree.size()));

    for (const auto &e : tree) {
        if (e.depth > depth) {
            current.push_back(e.id);
        } else if (e.depth == depth) {
            current.back() = e.id;
        } else {
            current.pop_back();
            current.back() = e.id;
        }

        if (e.collapsed)
            elements.push_back(current);
    }

    UserSettings::instance()->setCollapsedSpaces(elements);
}
void
CommunitiesModel::FlatTree::restoreCollapsed()
{
    QList<QStringList> elements = UserSettings::instance()->collapsedSpaces();

    int depth = -1;

    QStringList current;

    for (auto &e : tree) {
        if (e.depth > depth) {
            current.push_back(e.id);
        } else if (e.depth == depth) {
            current.back() = e.id;
        } else {
            current.pop_back();
            current.back() = e.id;
        }

        if (elements.contains(current))
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
    resetCurrentTagId();

    emit tagsChanged();
}

void
CommunitiesModel::setCurrentTagId(const QString &tagId)
{
    if (currentTagId_ == tagId)
        return;

    if (tagId.startsWith(QLatin1String("tag:"))) {
        auto tag = tagId.mid(4);
        for (const auto &t : std::as_const(tags_)) {
            if (t == tag) {
                this->currentTagId_ = tagId;
                UserSettings::instance()->setCurrentTagId(tagId);
                emit currentTagIdChanged(currentTagId_);
                return;
            }
        }
    } else if (tagId.startsWith(QLatin1String("space:"))) {
        auto tag = tagId.mid(6);
        for (const auto &t : spaceOrder_.tree) {
            if (t.id == tag) {
                this->currentTagId_ = tagId;
                UserSettings::instance()->setCurrentTagId(tagId);
                emit currentTagIdChanged(currentTagId_);
                return;
            }
        }
    } else if (tagId == QLatin1String("people") || tagId == QLatin1String("bot") ||
               tagId == QLatin1String("group")) {
        this->currentTagId_ = tagId;
        UserSettings::instance()->setCurrentTagId(tagId);
        emit currentTagIdChanged(currentTagId_);
        return;
    }

    this->currentTagId_ = QLatin1String("");
    UserSettings::instance()->setCurrentTagId(tagId);
    emit currentTagIdChanged(currentTagId_);
}

bool
CommunitiesModel::trySwitchToSpace(const QString &tag)
{
    for (const auto &t : spaceOrder_.tree) {
        if (t.id == tag) {
            this->currentTagId_ = "space:" + tag;
            UserSettings::instance()->setCurrentTagId(tag);
            emit currentTagIdChanged(currentTagId_);
            return true;
        }
    }

    return false;
}

void
CommunitiesModel::toggleTagId(QString tagId)
{
    if (hiddenTagIds_.contains(tagId))
        hiddenTagIds_.removeOne(tagId);
    else
        hiddenTagIds_.push_back(tagId);

    // sanity check to remove stale spaces
    hiddenTagIds_.removeIf([this](const QString &value) {
        return value.startsWith("space:") && !spaces_.contains(value.mid(6));
    });

    UserSettings::instance()->setHiddenTags(hiddenTagIds_);

    if (tagId.startsWith(QLatin1String("tag:"))) {
        auto idx = tags_.indexOf(tagId.mid(4));
        if (idx != -1)
            emit dataChanged(index(idx + kFixedRowCount + spaceOrder_.size()),
                             index(idx + kFixedRowCount + spaceOrder_.size()),
                             {Hidden});
    } else if (tagId.startsWith(QLatin1String("space:"))) {
        auto idx = spaceOrder_.indexOf(tagId.mid(6));
        if (idx != -1)
            emit dataChanged(index(idx + kFixedRowCount), index(idx + kFixedRowCount), {Hidden});
    } else if (tagId == QLatin1String("people")) {
        emit dataChanged(index(kRowPeople), index(kRowPeople), {Hidden});
    } else if (tagId == QLatin1String("bot")) {
        emit dataChanged(index(kRowBots), index(kRowBots), {Hidden});
    } else if (tagId == QLatin1String("group")) {
        emit dataChanged(index(kRowGroups), index(kRowGroups), {Hidden});
    }

    emit hiddenTagsChanged();
}

void
CommunitiesModel::toggleTagBadges(QString tagId)
{
    if (tagId.isEmpty())
        tagId = QStringLiteral("global");

    if (badgesHiddenTagIds_.contains(tagId))
        badgesHiddenTagIds_.removeOne(tagId);
    else
        badgesHiddenTagIds_.push_back(tagId);
    UserSettings::instance()->setBadgesHiddenTags(badgesHiddenTagIds_);

    if (tagId.startsWith(QLatin1String("tag:"))) {
        auto idx = tags_.indexOf(tagId.mid(4));
        if (idx != -1)
            emit dataChanged(index(idx + kFixedRowCount + spaceOrder_.size()),
                             index(idx + kFixedRowCount + spaceOrder_.size()));
    } else if (tagId.startsWith(QLatin1String("space:"))) {
        auto idx = spaceOrder_.indexOf(tagId.mid(6));
        if (idx != -1)
            emit dataChanged(index(idx + kFixedRowCount), index(idx + kFixedRowCount));
    } else if (tagId == QLatin1String("people")) {
        emit dataChanged(index(kRowPeople), index(kRowPeople));
    } else if (tagId == QLatin1String("bot")) {
        emit dataChanged(index(kRowBots), index(kRowBots));
    } else if (tagId == QLatin1String("group")) {
        emit dataChanged(index(kRowGroups), index(kRowGroups));
    } else if (tagId == QLatin1String("global")) {
        emit dataChanged(index(kRowAllRooms), index(kRowAllRooms));
    }

    emit badgesHiddenTagsChanged();
}

bool
CommunitiesModel::areTagBadgesHidden(const QString &tagId) const
{
    if (tagId.isEmpty())
        return badgesHiddenTagIds_.contains(QStringLiteral("global"));
    return badgesHiddenTagIds_.contains(tagId);
}

bool
CommunitiesModel::isTagHidden(const QString &tagId) const
{
    return hiddenTagIds_.contains(tagId);
}

#include "moc_CommunitiesModel.cpp"
