// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "powerlevels/PowerlevelsEditModels.h"

#include <functional>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "utils/Utils.h"

static bool
samePl(const mtx::events::state::PowerLevels &a, const mtx::events::state::PowerLevels &b)
{
    return std::tie(a.events,
                    a.users_default,
                    a.users,
                    a.state_default,
                    a.users_default,
                    a.events_default,
                    a.ban,
                    a.kick,
                    a.invite,
                    a.notifications,
                    a.redact) == std::tie(b.events,
                                          b.users_default,
                                          b.users,
                                          b.state_default,
                                          b.users_default,
                                          b.events_default,
                                          b.ban,
                                          b.kick,
                                          b.invite,
                                          b.notifications,
                                          b.redact);
}

PowerlevelsSpacesListModel::PowerlevelsSpacesListModel(
  const std::string &room_id_,
  const mtx::events::state::PowerLevels &pl,
  const mtx::events::StateEvent<mtx::events::state::Create> &create,
  QObject *parent)
  : QAbstractListModel(parent)
  , room_id(std::move(room_id_))
  , oldPowerLevels_(std::move(pl))
{
    beginResetModel();

    spaces.push_back(Entry{room_id, oldPowerLevels_, create, true});

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

    nhlog::ui()->warn("Ignoring power level propagation for '{}' to {} child rooms; this flow "
                      "is not migrated to matrix-sdk yet",
                      room_id,
                      selectedSpaceCount);
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
    auto row = index.row();
    if (row >= spaces.size() && row < 0)
        return {};

    if (role == Roles::DisplayName || role == Roles::AvatarUrl || role == Roles::IsSpace) {
        if (role == Roles::DisplayName)
            return QString::fromStdString(spaces.at(row).roomid);
        else if (role == Roles::AvatarUrl)
            return {};
        else
            return row == 0;
    }

    auto entry = spaces.at(row);
    switch (role) {
    case Roles::IsEditable:
        return komai::matrix::effectiveUserPowerLevel(entry.pl,
                                                      entry.create,
                                                      utils::localUser().toStdString()) >=
               entry.pl.state_level("m.room.power_levels");
    case Roles::IsDifferentFromBase:
        return !samePl(entry.pl, oldPowerLevels_);
    case Roles::IsAlreadyUpToDate:
        return samePl(entry.pl, newPowerlevels_);
    case Roles::ApplyPermissions:
        return entry.apply;
    }
    return {};
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
