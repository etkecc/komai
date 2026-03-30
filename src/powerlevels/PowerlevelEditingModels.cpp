// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "powerlevels/PowerlevelsEditModels.h"

#include "chat/ChatPage.h"
#include "logging/Logging.h"

PowerlevelEditingModels::PowerlevelEditingModels(QString room_id, QObject *parent)
  : QObject(parent)
  , types_(room_id.toStdString(), powerLevels_, create_, this)
  , users_(room_id.toStdString(), powerLevels_, create_, this)
  , spaces_(room_id.toStdString(), powerLevels_, create_, this)
  , room_id_(room_id.toStdString())
{
    connect(&types_,
            &PowerlevelsTypeListModel::adminLevelChanged,
            this,
            &PowerlevelEditingModels::adminLevelChanged);
    connect(&types_,
            &PowerlevelsTypeListModel::moderatorLevelChanged,
            this,
            &PowerlevelEditingModels::moderatorLevelChanged);
    connect(&users_,
            &PowerlevelsUserListModel::defaultUserLevelChanged,
            this,
            &PowerlevelEditingModels::defaultUserLevelChanged);
}

bool
PowerlevelEditingModels::isSpace() const
{
    return false;
}

mtx::events::state::PowerLevels
PowerlevelEditingModels::calculateNewPowerlevel() const
{
    auto newPl           = powerLevels_;
    newPl.events         = types_.toEvents();
    newPl.kick           = types_.kick();
    newPl.invite         = types_.invite();
    newPl.ban            = types_.ban();
    newPl.redact         = types_.redact();
    newPl.events_default = types_.eventsDefault();
    newPl.state_default  = types_.stateDefault();
    newPl.users          = users_.toUsers();
    newPl.users_default  = users_.usersDefault();
    return newPl;
}

void
PowerlevelEditingModels::commit()
{
    powerLevels_ = calculateNewPowerlevel();
    nhlog::ui()->warn("Ignoring power level update for '{}'; this flow is not migrated to "
                      "matrix-sdk yet",
                      room_id_);
    ChatPage::instance()->showNotification(
      tr("Power level editing has not been migrated to the matrix-sdk backend yet."));
}

void
PowerlevelEditingModels::updateSpacesModel()
{
    powerLevels_            = calculateNewPowerlevel();
    spaces_.newPowerlevels_ = powerLevels_;
}

void
PowerlevelEditingModels::addRole(int pl)
{
    for (const auto &e : std::as_const(types_.types))
        if (pl == int(e.pl))
            return;

    types_.addRole(pl);
    users_.addRole(pl);
}

#include "moc_PowerlevelsEditModels.cpp"
