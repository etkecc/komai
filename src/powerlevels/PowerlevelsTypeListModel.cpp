// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "powerlevels/PowerlevelsEditModels.h"

#include <algorithm>
#include <set>

PowerlevelsTypeListModel::PowerlevelsTypeListModel(const komai::MatrixRoomPowerLevels &powerLevels,
                                                   QObject *parent)
  : QAbstractListModel(parent)
{
    setPowerLevels(powerLevels);
}

void
PowerlevelsTypeListModel::setPowerLevels(const komai::MatrixRoomPowerLevels &powerLevels)
{
    beginResetModel();

    powerLevels_ = powerLevels;
    types.clear();

    std::set<qlonglong> seenLevels;
    for (const auto &entry : powerLevels_.events) {
        if (!seenLevels.count(entry.level)) {
            types.push_back(Entry{QString{}, entry.level});
            seenLevels.insert(entry.level);
        }
        types.push_back(Entry{entry.key, entry.level});
    }

    for (const auto &entry : powerLevels_.users) {
        if (!seenLevels.count(entry.level)) {
            types.push_back(Entry{QString{}, entry.level});
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
            types.push_back(Entry{QString{}, level});
            seenLevels.insert(level);
        }
    }
    if (komai::matrix::powerLevelsCreatorsHaveInfinitePower(powerLevels_))
        seenLevels.insert(komai::powerlevels::CreatorPowerLevel);

    types.push_back(Entry{QStringLiteral("zdefault_states"), powerLevels_.stateDefault});
    types.push_back(Entry{QStringLiteral("zdefault_events"), powerLevels_.eventsDefault});
    types.push_back(Entry{QStringLiteral("ban"), powerLevels_.ban});
    types.push_back(Entry{QStringLiteral("kick"), powerLevels_.kick});
    types.push_back(Entry{QStringLiteral("invite"), powerLevels_.invite});
    types.push_back(Entry{QStringLiteral("redact"), powerLevels_.redact});

    std::sort(types.begin(), types.end(), [](const Entry &a, const Entry &b) {
        if (a.pl != b.pl)
            return a.pl > b.pl;
        if (a.type.isEmpty() != b.type.isEmpty())
            return a.type.isEmpty() > b.type.isEmpty();

        const bool aContainsDot = a.type.contains('.');
        const bool bContainsDot = b.type.contains('.');
        if (aContainsDot != bContainsDot)
            return aContainsDot > bContainsDot;
        return a.type < b.type;
    });

    endResetModel();
}

QVector<komai::MatrixPowerLevelEntry>
PowerlevelsTypeListModel::toEvents() const
{
    QVector<komai::MatrixPowerLevelEntry> result;
    for (const auto &entry : std::as_const(types)) {
        if (entry.type.contains('.'))
            result.push_back({.key = entry.type, .level = entry.pl});
    }
    return result;
}

qlonglong
PowerlevelsTypeListModel::kick() const
{
    for (const auto &entry : std::as_const(types)) {
        if (entry.type == "kick")
            return entry.pl;
    }
    return powerLevels_.usersDefault;
}

qlonglong
PowerlevelsTypeListModel::invite() const
{
    for (const auto &entry : std::as_const(types)) {
        if (entry.type == "invite")
            return entry.pl;
    }
    return powerLevels_.usersDefault;
}

qlonglong
PowerlevelsTypeListModel::ban() const
{
    for (const auto &entry : std::as_const(types)) {
        if (entry.type == "ban")
            return entry.pl;
    }
    return powerLevels_.usersDefault;
}

qlonglong
PowerlevelsTypeListModel::redact() const
{
    for (const auto &entry : std::as_const(types)) {
        if (entry.type == "redact")
            return entry.pl;
    }
    return powerLevels_.usersDefault;
}

qlonglong
PowerlevelsTypeListModel::eventsDefault() const
{
    for (const auto &entry : std::as_const(types)) {
        if (entry.type == "zdefault_events")
            return entry.pl;
    }
    return powerLevels_.usersDefault;
}

qlonglong
PowerlevelsTypeListModel::stateDefault() const
{
    for (const auto &entry : std::as_const(types)) {
        if (entry.type == "zdefault_states")
            return entry.pl;
    }
    return powerLevels_.usersDefault;
}

QHash<int, QByteArray>
PowerlevelsTypeListModel::roleNames() const
{
    return {
      {DisplayName, "displayName"},
      {Powerlevel, "powerlevel"},
      {IsType, "isType"},
      {Moveable, "moveable"},
      {Removeable, "removeable"},
    };
}

QVariant
PowerlevelsTypeListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= types.size())
        return {};

    const auto &type = types.at(index.row());

    switch (static_cast<Roles>(role)) {
    case DisplayName:
        if (type.type == "zdefault_events")
            return tr("Other events");
        else if (type.type == "zdefault_states")
            return tr("Other state events");
        else if (type.type == "kick")
            return tr("Remove other users");
        else if (type.type == "ban")
            return tr("Ban other users");
        else if (type.type == "invite")
            return tr("Invite other users");
        else if (type.type == "redact")
            return tr("Redact events sent by others");
        else if (type.type == "m.reaction")
            return tr("Reactions");
        else if (type.type == "m.room.aliases")
            return tr("Deprecated aliases events");
        else if (type.type == "m.room.avatar")
            return tr("Change the room avatar");
        else if (type.type == "m.room.canonical_alias")
            return tr("Change the room addresses");
        else if (type.type == "m.room.encrypted")
            return tr("Send encrypted messages");
        else if (type.type == "m.room.encryption")
            return tr("Enable encryption");
        else if (type.type == "m.room.guest_access")
            return tr("Change guest access");
        else if (type.type == "m.room.history_visibility")
            return tr("Change history visibility");
        else if (type.type == "m.room.join_rules")
            return tr("Change who can join");
        else if (type.type == "m.room.message")
            return tr("Send messages");
        else if (type.type == "m.room.name")
            return tr("Change the room name");
        else if (type.type == "m.room.power_levels")
            return tr("Change the room permissions");
        else if (type.type == "m.room.topic")
            return tr("Change the rooms topic");
        else if (type.type == "m.widget")
            return tr("Change the widgets");
        else if (type.type == "im.vector.modular.widgets")
            return tr("Change the widgets (experimental)");
        else if (type.type == "m.room.redaction")
            return tr("Redact own events");
        else if (type.type == "m.room.pinned_events")
            return tr("Change the pinned events");
        else if (type.type == "m.room.tombstone")
            return tr("Upgrade the room");
        else if (type.type == "m.sticker")
            return tr("Send stickers");
        else if (type.type == "m.policy.rule.user")
            return tr("Ban users using policy rules");
        else if (type.type == "m.policy.rule.room")
            return tr("Ban rooms using policy rules");
        else if (type.type == "m.policy.rule.server")
            return tr("Ban servers using policy rules");
        else if (type.type == "m.space.child")
            return tr("Edit child communities and rooms");
        else if (type.type == "m.space.parent")
            return tr("Change parent communities");
        else if (type.type == "m.call.invite")
            return tr("Start a call");
        else if (type.type == "m.call.candidates")
            return tr("Negotiate a call");
        else if (type.type == "m.call.answer")
            return tr("Answer a call");
        else if (type.type == "m.call.hangup")
            return tr("Hang up a call");
        else if (type.type == "m.call.reject")
            return tr("Reject a call");
        else if (type.type == "im.ponies.room_emotes")
            return tr("Change the room emotes");
        return type.type;
    case Powerlevel:
        return type.pl;
    case IsType:
        return !type.type.isEmpty();
    case Moveable:
        return !type.type.isEmpty();
    case Removeable:
        return !type.type.isEmpty() && type.type.contains('.');
    }

    return {};
}

bool
PowerlevelsTypeListModel::remove(int row)
{
    if (row < 0 || row >= types.size() || types.at(row).type.isEmpty())
        return false;

    beginRemoveRows(QModelIndex(), row, row);
    types.remove(row);
    endRemoveRows();

    return true;
}

void
PowerlevelsTypeListModel::add(int row, QString type)
{
    if (row < 0 || row > types.size())
        return;

    for (int i = 0; i < types.size(); i++) {
        if (types[i].type == type) {
            if (i > row)
                move(i, row + 1);
            else
                move(i, row);
            return;
        }
    }

    beginInsertRows(QModelIndex(), row + 1, row + 1);
    types.insert(row + 1, Entry{type, types.at(row).pl});
    endInsertRows();
}

void
PowerlevelsTypeListModel::addRole(int64_t role)
{
    for (int i = 0; i < types.size(); i++) {
        if (types[i].pl < role) {
            beginInsertRows(QModelIndex(), i, i);
            types.insert(i, Entry{QString{}, role});
            endInsertRows();
            return;
        }
    }

    beginInsertRows(QModelIndex(), types.size(), types.size());
    types.push_back(Entry{QString{}, role});
    endInsertRows();
}

bool
PowerlevelsTypeListModel::move(int from, int to)
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
PowerlevelsTypeListModel::moveRows(const QModelIndex &,
                                   int sourceRow,
                                   int count,
                                   const QModelIndex &,
                                   int destinationChild)
{
    if (sourceRow == destinationChild)
        return true;
    if (count != 1)
        return false;
    if (sourceRow < 0 || sourceRow >= types.size())
        return false;
    if (destinationChild < 0 || destinationChild > types.size())
        return false;
    if (types.at(sourceRow).type.isEmpty())
        return false;

    const auto pl = types.at(destinationChild > 0 ? destinationChild - 1 : 0).pl;
    auto sourceItem = types.takeAt(sourceRow);
    sourceItem.pl   = pl;

    const auto movedType = sourceItem.type;

    if (destinationChild < sourceRow)
        types.insert(destinationChild, std::move(sourceItem));
    else
        types.insert(destinationChild - 1, std::move(sourceItem));

    if (movedType == "m.room.power_levels")
        emit adminLevelChanged();
    else if (movedType == "redact")
        emit moderatorLevelChanged();

    return true;
}
