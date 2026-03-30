// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "powerlevels/PowerlevelsEditModels.h"

#include <algorithm>
#include <set>

PowerlevelsUserListModel::PowerlevelsUserListModel(const komai::MatrixRoomPowerLevels &powerLevels,
                                                   QObject *parent)
  : QAbstractListModel(parent)
{
    setPowerLevels(powerLevels);
}

void
PowerlevelsUserListModel::setPowerLevels(const komai::MatrixRoomPowerLevels &powerLevels)
{
    beginResetModel();

    powerLevels_ = powerLevels;
    users.clear();

    std::set<qlonglong> seenLevels;
    for (const auto &entry : powerLevels_.users) {
        if (!seenLevels.count(entry.level)) {
            users.push_back(Entry{QString{}, entry.level});
            seenLevels.insert(entry.level);
        }
        users.push_back(Entry{entry.key, entry.level});
    }

    for (const auto &entry : powerLevels_.events) {
        if (!seenLevels.count(entry.level)) {
            users.push_back(Entry{QString{}, entry.level});
            seenLevels.insert(entry.level);
        }
    }

    for (const auto level : {powerLevels_.eventsDefault,
                             powerLevels_.stateDefault,
                             powerLevels_.usersDefault,
                             powerLevels_.ban,
                             powerLevels_.kick,
                             powerLevels_.invite,
                             powerLevels_.redact}) {
        if (!seenLevels.count(level)) {
            users.push_back(Entry{QString{}, level});
            seenLevels.insert(level);
        }
    }

    if (komai::matrix::powerLevelsCreatorsHaveInfinitePower(powerLevels_)) {
        users.push_back(Entry{QString{}, komai::powerlevels::CreatorPowerLevel});
        seenLevels.insert(komai::powerlevels::CreatorPowerLevel);

        for (const auto &creator : powerLevels_.creators)
            users.push_back(Entry{creator, komai::powerlevels::CreatorPowerLevel});
    }

    users.push_back(Entry{QStringLiteral("default"), powerLevels_.usersDefault});

    std::sort(users.begin(), users.end(), [](const Entry &a, const Entry &b) {
        if (a.pl != b.pl)
            return a.pl > b.pl;
        return a.mxid < b.mxid;
    });

    endResetModel();
}

QVector<komai::MatrixPowerLevelEntry>
PowerlevelsUserListModel::toUsers() const
{
    QVector<komai::MatrixPowerLevelEntry> result;
    for (const auto &entry : std::as_const(users)) {
        if (entry.mxid.startsWith('@') && entry.pl != komai::powerlevels::CreatorPowerLevel)
            result.push_back({.key = entry.mxid, .level = entry.pl});
    }
    return result;
}

qlonglong
PowerlevelsUserListModel::usersDefault() const
{
    for (const auto &entry : std::as_const(users)) {
        if (entry.mxid == "default")
            return entry.pl;
    }
    return powerLevels_.usersDefault;
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
        if (user.mxid == "default")
            return QStringLiteral("*");
        return user.mxid;
    case DisplayName:
        if (user.mxid == "default")
            return tr("Other users");
        return user.mxid;
    case AvatarUrl:
        return {};
    case Powerlevel:
        return user.pl;
    case IsUser:
        return !user.mxid.isEmpty();
    case Moveable:
        return !user.mxid.isEmpty() && user.pl != komai::powerlevels::CreatorPowerLevel;
    case Removeable:
        return !user.mxid.isEmpty() && user.mxid.contains(':');
    }

    return {};
}

bool
PowerlevelsUserListModel::remove(int row)
{
    if (row < 0 || row >= users.size() || users.at(row).mxid.isEmpty())
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

    for (int i = 0; i < users.size(); i++) {
        if (users[i].mxid == user) {
            if (i > row)
                move(i, row + 1);
            else
                move(i, row);
            return;
        }
    }

    beginInsertRows(QModelIndex(), row + 1, row + 1);
    users.insert(row + 1, Entry{user, users.at(row).pl});
    endInsertRows();
}

void
PowerlevelsUserListModel::addRole(int64_t role)
{
    for (int i = 0; i < users.size(); i++) {
        if (users[i].pl < role) {
            beginInsertRows(QModelIndex(), i, i);
            users.insert(i, Entry{QString{}, role});
            endInsertRows();
            return;
        }
    }

    beginInsertRows(QModelIndex(), users.size(), users.size());
    users.push_back(Entry{QString{}, role});
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
    const auto ret = moveRow(QModelIndex(), from, QModelIndex(), to);
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
    if (users.at(sourceRow).mxid.isEmpty())
        return false;
    if (users.at(sourceRow).pl == komai::powerlevels::CreatorPowerLevel)
        return false;
    if (destinationChild < users.size() &&
        users.at(destinationChild).pl == komai::powerlevels::CreatorPowerLevel)
        return false;

    const auto pl = users.at(destinationChild > 0 ? destinationChild - 1 : 0).pl;
    if (pl == komai::powerlevels::CreatorPowerLevel)
        return false;

    auto sourceItem = users.takeAt(sourceRow);
    sourceItem.pl   = pl;

    const auto movedType = sourceItem.mxid;

    if (destinationChild < sourceRow)
        users.insert(destinationChild, std::move(sourceItem));
    else
        users.insert(destinationChild - 1, std::move(sourceItem));

    if (movedType == "default")
        emit defaultUserLevelChanged();

    return true;
}
