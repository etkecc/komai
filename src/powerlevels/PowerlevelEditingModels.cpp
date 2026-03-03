// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "powerlevels/PowerlevelsEditModels.h"

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"

PowerlevelEditingModels::PowerlevelEditingModels(QString room_id, QObject *parent)
  : QObject(parent)
  , powerLevels_(cache::getStateEvent<mtx::events::state::PowerLevels>(room_id.toStdString())
                   .value_or(mtx::events::StateEvent<mtx::events::state::PowerLevels>{})
                   .content)
  , types_(room_id.toStdString(), powerLevels_, this)
  , users_(room_id.toStdString(), powerLevels_, this)
  , spaces_(room_id.toStdString(), powerLevels_, this)
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
    return cache::singleRoomInfo(room_id_).is_space;
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

    http::client()->send_state_event(
      room_id_, powerLevels_, [](const mtx::responses::EventId &, mtx::http::RequestErr e) {
          if (e) {
              nhlog::net()->error("Failed to send PL event: {}", *e);
              ChatPage::instance()->showNotification(
                tr("Failed to update powerlevel: %1")
                  .arg(QString::fromStdString(e->matrix_error.error)));
          }
      });
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
