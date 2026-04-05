// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "powerlevels/PowerlevelsEditModels.h"

#include <QCoreApplication>
#include <QPointer>
#include <thread>

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"

PowerlevelEditingModels::PowerlevelEditingModels(QString room_id, QObject *parent)
  : QObject(parent)
  , types_(powerLevels_, this)
  , users_(powerLevels_, this)
  , spaces_(room_id, powerLevels_, this)
  , roomId_(std::move(room_id))
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

    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0)
        return;

    QPointer<PowerlevelEditingModels> self(this);
    std::thread([self, handleId, roomId = roomId_]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const auto powerLevels = komai::MatrixBackendRuntimeService::fetchRoomPowerLevels(
          context, handleId, roomId, &error);

        QString childError;
        const auto childSpaces = komai::MatrixBackendRuntimeService::fetchRoomChildSpaces(
          context, handleId, roomId, &childError);

        auto *app = QCoreApplication::instance();
        if (!app)
            return;

        QMetaObject::invokeMethod(
          app,
          [self,
           roomId,
           error = std::move(error),
           powerLevels,
           childSpaces = std::move(childSpaces)]() mutable {
              if (!self)
                  return;

              if (!powerLevels) {
                  if (!error.isEmpty()) {
                      nhlog::ui()->warn("Failed to load matrix-sdk room power levels for '{}': {}",
                                        roomId.toStdString(),
                                        error.toStdString());
                      if (ChatPage::instance()) {
                          ChatPage::instance()->showNotification(PowerlevelEditingModels::tr(
                            "Failed to load room permissions from the matrix-sdk backend."));
                      }
                  }
                  return;
              }

              self->setPowerLevels(*powerLevels);

              if (childSpaces && !childSpaces->isEmpty()) {
                  QVector<PowerlevelsSpacesListModel::Entry> children;
                  children.reserve(childSpaces->size());
                  for (const auto &child : *childSpaces) {
                      children.push_back(PowerlevelsSpacesListModel::Entry{child.roomId,
                                                                           child.displayName,
                                                                           child.avatarUrl,
                                                                           child.powerLevels,
                                                                           false});
                  }
                  self->spaces_.addChildSpaces(std::move(children));
                  emit self->isSpaceChanged();
              }
          },
          Qt::QueuedConnection);
    }).detach();
}

bool
PowerlevelEditingModels::isSpace() const
{
    return spaces_.spaces.size() > 1;
}

komai::MatrixRoomPowerLevels
PowerlevelEditingModels::calculateNewPowerlevel() const
{
    auto newPl          = powerLevels_;
    newPl.events        = types_.toEvents();
    newPl.kick          = types_.kick();
    newPl.invite        = types_.invite();
    newPl.ban           = types_.ban();
    newPl.redact        = types_.redact();
    newPl.eventsDefault = types_.eventsDefault();
    newPl.stateDefault  = types_.stateDefault();
    newPl.users         = users_.toUsers();
    newPl.usersDefault  = users_.usersDefault();
    return newPl;
}

void
PowerlevelEditingModels::setPowerLevels(komai::MatrixRoomPowerLevels powerLevels)
{
    powerLevels_ = std::move(powerLevels);
    types_.setPowerLevels(powerLevels_);
    users_.setPowerLevels(powerLevels_);
    spaces_.oldPowerLevels_ = powerLevels_;
    spaces_.newPowerlevels_ = powerLevels_;
    if (!spaces_.spaces.isEmpty())
        spaces_.spaces[0].pl = powerLevels_;

    const bool loadedNow = !loaded_;
    loaded_              = true;

    emit adminLevelChanged();
    emit moderatorLevelChanged();
    emit defaultUserLevelChanged();
    if (loadedNow)
        emit loadedChanged();
}

void
PowerlevelEditingModels::commit()
{
    if (!loaded_ || committing_)
        return;

    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0)
        return;

    const auto updated = calculateNewPowerlevel();

    committing_ = true;
    emit committingChanged();

    QPointer<PowerlevelEditingModels> self(this);
    std::thread([self, handleId, roomId = roomId_, updated]() mutable {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const bool ok = komai::MatrixBackendRuntimeService::applyRoomPowerLevels(
          context, handleId, roomId, updated, &error);

        auto *app = QCoreApplication::instance();
        if (!app)
            return;

        QMetaObject::invokeMethod(
          app,
          [self, roomId, updated = std::move(updated), ok, error = std::move(error)]() mutable {
              if (!self)
                  return;

              self->committing_ = false;
              emit self->committingChanged();

              if (!ok) {
                  nhlog::ui()->warn("Failed to apply matrix-sdk room power levels for '{}': {}",
                                    roomId.toStdString(),
                                    error.toStdString());
                  if (ChatPage::instance()) {
                      ChatPage::instance()->showNotification(PowerlevelEditingModels::tr(
                        "Failed to save room permissions to the matrix-sdk backend."));
                  }
                  return;
              }

              self->setPowerLevels(std::move(updated));
          },
          Qt::QueuedConnection);
    }).detach();
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
    for (const auto &entry : std::as_const(types_.types)) {
        if (pl == entry.pl)
            return;
    }

    types_.addRole(pl);
    users_.addRole(pl);
}

#include "moc_PowerlevelsEditModels.cpp"
