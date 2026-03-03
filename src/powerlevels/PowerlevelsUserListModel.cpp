// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "powerlevels/PowerlevelsEditModels.h"

#include <algorithm>
#include <set>

#include "cache/Cache.h"

PowerlevelsUserListModel::PowerlevelsUserListModel(const std::string &rid,
                                                   const mtx::events::state::PowerLevels &pl,
                                                   QObject *parent)
  : QAbstractListModel(parent)
  , room_id(rid)
  , powerLevels_(pl)
{
    std::set<mtx::events::state::power_level_t> seen_levels;
    for (const auto &[user, level] : powerLevels_.users) {
        if (!seen_levels.count(level)) {
            users.push_back(Entry{"", level});
            seen_levels.insert(level);
        }
        users.push_back(Entry{user, level});
    }

    for (const auto &[type, level] : powerLevels_.events) {
        (void)type;
        if (!seen_levels.count(level)) {
            users.push_back(Entry{"", level});
            seen_levels.insert(level);
        }
    }

    for (const auto &level : {
           powerLevels_.events_default,
           powerLevels_.state_default,
           powerLevels_.users_default,
           powerLevels_.ban,
           powerLevels_.kick,
           powerLevels_.invite,
           powerLevels_.redact,
         }) {
        if (!seen_levels.count(level)) {
            users.push_back(Entry{"", level});
            seen_levels.insert(level);
        }
    }

    users.push_back(Entry{"default", powerLevels_.users_default});

    std::sort(users.begin(), users.end(), [](const Entry &a, const Entry &b) {
        if (a.pl != b.pl)
            return a.pl > b.pl;
        else
            return a.mxid < b.mxid;
    });
}

std::map<std::string, mtx::events::state::power_level_t, std::less<>>
PowerlevelsUserListModel::toUsers() const
{
    std::map<std::string, mtx::events::state::power_level_t, std::less<>> m;
    for (const auto &[key, pl] : std::as_const(users))
        if (key.size() > 0 && key.at(0) == '@')
            m[key] = pl;
    return m;
}

mtx::events::state::power_level_t
PowerlevelsUserListModel::usersDefault() const
{
    for (const auto &[key, pl] : std::as_const(users))
        if (key == "default")
            return pl;
    return powerLevels_.users_default;
}

QHash<int, QByteArray>
PowerlevelsUserListModel::roleNames() const
{
    return {
      {Mxid, "mxid"},
      {DisplayName, "displayName"},
      {AvatarUrl, "avatarUrl"},
      {Powerlevel, "powerlevel"},
      {IsUser, "isUser"},
      {Moveable, "moveable"},
      {Removeable, "removeable"},
    };
}

QVariant
PowerlevelsUserListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= users.size())
        return {};

    const auto &user = users.at(index.row());

    switch (static_cast<Roles>(role)) {
    case Mxid:
        if ("default" == user.mxid)
            return QStringLiteral("*");
        return QString::fromStdString(user.mxid);
    case DisplayName:
        if (user.mxid == "default")
            return tr("Other users");
        return QString::fromStdString(cache::displayName(room_id, user.mxid));
    case AvatarUrl:
        return cache::avatarUrl(QString::fromStdString(room_id), QString::fromStdString(user.mxid));
    case Powerlevel:
        return static_cast<qlonglong>(user.pl);
    case IsUser:
        return !user.mxid.empty();
    case Moveable:
        return !user.mxid.empty();
    case Removeable:
        return !user.mxid.empty() && user.mxid.find('.') != std::string::npos;
    }

    return {};
}

bool
PowerlevelsUserListModel::remove(int row)
{
    if (row < 0 || row >= users.size() || users.at(row).mxid.empty())
        return false;

    beginRemoveRows(QModelIndex(), row, row);
    users.remove(row);
    endRemoveRows();

    return true;
}

void
PowerlevelsUserListModel::add(int row, QString user)
{
    if (row < 0 || row > users.size())
        return;

    const auto userStr = user.toStdString();
    for (int i = 0; i < users.size(); i++) {
        if (users[i].mxid == userStr) {
            if (i > row)
                move(i, row + 1);
            else
                move(i, row);
            return;
        }
    }

    beginInsertRows(QModelIndex(), row + 1, row + 1);
    users.insert(row + 1, Entry{user.toStdString(), users.at(row).pl});
    endInsertRows();
}

void
PowerlevelsUserListModel::addRole(int64_t role)
{
    for (int i = 0; i < users.size(); i++) {
        if (users[i].pl < role) {
            beginInsertRows(QModelIndex(), i, i);
            users.insert(i, Entry{"", role});
            endInsertRows();
            return;
        }
    }

    beginInsertRows(QModelIndex(), users.size(), users.size());
    users.push_back(Entry{"", role});
    endInsertRows();
}

bool
PowerlevelsUserListModel::move(int from, int to)
{
    if (from == to)
        return false;
    if (from < to)
        to += 1;

    beginMoveRows(QModelIndex(), from, from, QModelIndex(), to);
    auto ret = moveRow(QModelIndex(), from, QModelIndex(), to);
    endMoveRows();
    return ret;
}

bool
PowerlevelsUserListModel::moveRows(const QModelIndex &,
                                   int sourceRow,
                                   int count,
                                   const QModelIndex &,
                                   int destinationChild)
{
    if (sourceRow == destinationChild)
        return true;

    if (count != 1)
        return false;

    if (sourceRow < 0 || sourceRow >= users.size())
        return false;
    if (destinationChild < 0 || destinationChild > users.size())
        return false;

    if (users.at(sourceRow).mxid.empty())
        return false;

    auto pl         = users.at(destinationChild > 0 ? destinationChild - 1 : 0).pl;
    auto sourceItem = users.takeAt(sourceRow);
    sourceItem.pl   = pl;

    auto movedType = sourceItem.mxid;

    if (destinationChild < sourceRow)
        users.insert(destinationChild, std::move(sourceItem));
    else
        users.insert(destinationChild - 1, std::move(sourceItem));

    if (movedType == "default")
        emit defaultUserLevelChanged();

    return true;
}
