// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CommunitiesModel.h"

#include "settings/ui/facade/UserSettingsPage.h"

CommunitiesModel::CommunitiesModel(QObject *parent)
  : QAbstractListModel(parent)
  , hiddenTagIds_{UserSettings::instance()->hiddenTags()}
  , mutedTagIds_{UserSettings::instance()->mutedTags()}
{
    instance_ = this;
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
      {Muted, "muted"},
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
    } else if (tagId == QLatin1String("dm") || tagId == QLatin1String("people") ||
               tagId == QLatin1String("bot") || tagId == QLatin1String("group")) {
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
    } else if (tagId == QLatin1String("dm")) {
        emit dataChanged(index(kRowDirectChats), index(kRowDirectChats), {Hidden});
    } else if (tagId == QLatin1String("bot")) {
        emit dataChanged(index(kRowBots), index(kRowBots), {Hidden});
    }

    emit hiddenTagsChanged();
}

void
CommunitiesModel::toggleTagMute(QString tagId)
{
    if (tagId.isEmpty())
        tagId = QStringLiteral("global");

    if (mutedTagIds_.contains(tagId))
        mutedTagIds_.removeOne(tagId);
    else
        mutedTagIds_.push_back(tagId);
    UserSettings::instance()->setMutedTags(mutedTagIds_);

    if (tagId.startsWith(QLatin1String("tag:"))) {
        auto idx = tags_.indexOf(tagId.mid(4));
        if (idx != -1)
            emit dataChanged(index(idx + kFixedRowCount + spaceOrder_.size()),
                             index(idx + kFixedRowCount + spaceOrder_.size()));
    } else if (tagId.startsWith(QLatin1String("space:"))) {
        auto idx = spaceOrder_.indexOf(tagId.mid(6));
        if (idx != -1)
            emit dataChanged(index(idx + kFixedRowCount), index(idx + kFixedRowCount));
    } else if (tagId == QLatin1String("dm")) {
        emit dataChanged(index(kRowDirectChats), index(kRowDirectChats));
    } else if (tagId == QLatin1String("bot")) {
        emit dataChanged(index(kRowBots), index(kRowBots));
    } else if (tagId == QLatin1String("global")) {
        emit dataChanged(index(kRowAllRooms), index(kRowAllRooms));
    }
}

#include "moc_CommunitiesModel.cpp"
