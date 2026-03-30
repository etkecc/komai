// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "powerlevels/PowerlevelsEditModels.h"

#include <tuple>

#include "chat/ChatPage.h"
#include "utils/Utils.h"

static bool
samePl(const komai::MatrixRoomPowerLevels &a, const komai::MatrixRoomPowerLevels &b)
{
    return std::tie(a.events,
                    a.usersDefault,
                    a.users,
                    a.stateDefault,
                    a.eventsDefault,
                    a.ban,
                    a.kick,
                    a.invite,
                    a.redact) == std::tie(b.events,
                                          b.usersDefault,
                                          b.users,
                                          b.stateDefault,
                                          b.eventsDefault,
                                          b.ban,
                                          b.kick,
                                          b.invite,
                                          b.redact);
}

PowerlevelsSpacesListModel::PowerlevelsSpacesListModel(const QString &roomId,
                                                       const komai::MatrixRoomPowerLevels &powerLevels,
                                                       QObject *parent)
  : QAbstractListModel(parent)
  , room_id(roomId)
  , oldPowerLevels_(powerLevels)
{
    beginResetModel();
    spaces.push_back(Entry{room_id, oldPowerLevels_, true});
    endResetModel();
    updateToDefaults();
}

void
PowerlevelsSpacesListModel::commit()
{
    int selectedSpaceCount = 0;
    for (const auto &space : std::as_const(spaces)) {
        if (space.apply)
            ++selectedSpaceCount;
    }

    Q_UNUSED(selectedSpaceCount)
    ChatPage::instance()->showNotification(
      tr("Applying power levels to child spaces has not been migrated to the matrix-sdk "
         "backend yet."));
}

void
PowerlevelsSpacesListModel::updateToDefaults()
{
    for (int i = 1; i < spaces.size(); i++) {
        spaces[i].apply =
          applyToChildren_ && data(index(i), Roles::IsEditable).toBool() &&
          !data(index(i), Roles::IsAlreadyUpToDate).toBool() &&
          (overwriteDiverged_ || !data(index(i), Roles::IsDifferentFromBase).toBool());
    }

    if (spaces.size() > 1)
        emit dataChanged(index(1), index(spaces.size() - 1), {Roles::ApplyPermissions});
}

bool
PowerlevelsSpacesListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Roles::ApplyPermissions || index.row() < 0 || index.row() >= spaces.size())
        return false;

    spaces[index.row()].apply = value.toBool();
    return true;
}

QVariant
PowerlevelsSpacesListModel::data(QModelIndex const &index, int role) const
{
    const auto row = index.row();
    if (row >= spaces.size() || row < 0)
        return {};

    if (role == Roles::DisplayName || role == Roles::AvatarUrl || role == Roles::IsSpace) {
        if (role == Roles::DisplayName)
            return spaces.at(row).roomid;
        if (role == Roles::AvatarUrl)
            return {};
        return row == 0;
    }

    const auto entry = spaces.at(row);
    switch (role) {
    case Roles::IsEditable:
        return komai::matrix::effectiveUserPowerLevel(entry.pl, utils::localUser()) >=
               entry.pl.stateDefault;
    case Roles::IsDifferentFromBase:
        return !samePl(entry.pl, oldPowerLevels_);
    case Roles::IsAlreadyUpToDate:
        return samePl(entry.pl, newPowerlevels_);
    case Roles::ApplyPermissions:
        return entry.apply;
    default:
        return {};
    }
}

QHash<int, QByteArray>
PowerlevelsSpacesListModel::roleNames() const
{
    return {
      {DisplayName, "displayName"},
      {AvatarUrl, "avatarUrl"},
      {IsEditable, "isEditable"},
      {IsDifferentFromBase, "isDifferentFromBase"},
      {IsAlreadyUpToDate, "isAlreadyUpToDate"},
      {ApplyPermissions, "applyPermissions"},
    };
}
