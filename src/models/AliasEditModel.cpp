// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/AliasEditModel.h"

#include <QCoreApplication>
#include <QPointer>

#include <set>
#include <thread>

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixRoomPowerLevels.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

namespace {

qlonglong
canonicalAliasEventLevel(const komai::MatrixRoomPowerLevels &powerLevels)
{
    for (const auto &entry : powerLevels.events) {
        if (entry.key == QLatin1String("m.room.canonical_alias"))
            return entry.level;
    }

    return powerLevels.stateDefault;
}

bool
canChangeCanonicalAlias(const komai::MatrixRoomPowerLevels &powerLevels)
{
    return komai::matrix::effectiveUserPowerLevel(powerLevels, utils::localUser().trimmed()) >=
           canonicalAliasEventLevel(powerLevels);
}

void
showAliasNotification(const QString &message)
{
    if (auto *chatPage = ChatPage::instance())
        chatPage->showNotification(message);
}

} // namespace

AliasEditingModel::AliasEditingModel(const std::string &rid, QObject *parent)
  : QAbstractListModel(parent)
  , room_id(rid)
{
    loadAsync();
}

void
AliasEditingModel::loadAsync()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0)
        return;

    if (!loading_) {
        loading_ = true;
        emit loadingChanged();
    }

    QPointer<AliasEditingModel> self(this);
    const auto roomId = QString::fromStdString(room_id);

    std::thread([self, handleId, roomId]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString aliasError;
        const auto runtimeAliases = komai::MatrixBackendRuntimeService::fetchRoomAliases(
          context, handleId, roomId, &aliasError);

        QString powerLevelsError;
        const auto powerLevels = komai::MatrixBackendRuntimeService::fetchRoomPowerLevels(
          context, handleId, roomId, &powerLevelsError);

        const bool canSendStateEvent =
          powerLevels.has_value() && canChangeCanonicalAlias(*powerLevels);

        auto *app = QCoreApplication::instance();
        if (!app)
            return;

        QMetaObject::invokeMethod(
          app,
          [self,
           roomId,
           runtimeAliases,
           canSendStateEvent,
           aliasError       = std::move(aliasError),
           powerLevelsError = std::move(powerLevelsError)]() mutable {
              if (!self)
                  return;

              if (self->loading_) {
                  self->loading_ = false;
                  emit self->loadingChanged();
              }

              if (self->canSendStateEvent != canSendStateEvent) {
                  self->canSendStateEvent = canSendStateEvent;
                  emit self->canAdvertizeChanged();
              }

              if (!runtimeAliases.has_value()) {
                  if (!aliasError.isEmpty()) {
                      komai::logging::ui()->warn(
                        "Failed to load matrix-sdk room aliases for '{}': {}",
                        roomId.toStdString(),
                        aliasError.toStdString());
                      showAliasNotification(AliasEditingModel::tr(
                        "Failed to load room aliases from the matrix-sdk backend."));
                  }

                  if (!powerLevelsError.isEmpty()) {
                      komai::logging::ui()->warn(
                        "Failed to load matrix-sdk room alias permissions for '{}': {}",
                        roomId.toStdString(),
                        powerLevelsError.toStdString());
                  }
                  return;
              }

              self->applyLoadedState(*runtimeAliases, canSendStateEvent);
          },
          Qt::QueuedConnection);
    }).detach();
}

void
AliasEditingModel::applyLoadedState(const komai::MatrixRoomAliases &loadedAliases,
                                    bool canSendCanonicalAliasStateEvent)
{
    beginResetModel();
    aliases.clear();
    aliasEvent = {};

    std::set<std::string> seenAliases;
    std::set<std::string> publishedAliases;
    for (const auto &alias : loadedAliases.publishedAliases)
        publishedAliases.insert(alias.toStdString());

    const auto canonicalAlias = loadedAliases.canonicalAlias.trimmed().toStdString();
    if (!canonicalAlias.empty()) {
        aliasEvent.alias = canonicalAlias;
        aliases.push_back(Entry{
          .alias      = canonicalAlias,
          .canonical  = true,
          .advertized = true,
          .published  = publishedAliases.count(canonicalAlias) > 0,
        });
        seenAliases.insert(canonicalAlias);
    }

    for (const auto &altAliasValue : loadedAliases.altAliases) {
        const auto altAlias = altAliasValue.trimmed().toStdString();
        if (altAlias.empty() || !seenAliases.insert(altAlias).second)
            continue;

        aliasEvent.alt_aliases.push_back(altAlias);
        aliases.push_back(Entry{
          .alias      = altAlias,
          .canonical  = false,
          .advertized = true,
          .published  = publishedAliases.count(altAlias) > 0,
        });
    }

    for (const auto &publishedAlias : publishedAliases) {
        if (!seenAliases.insert(publishedAlias).second)
            continue;

        aliases.push_back(Entry{
          .alias      = publishedAlias,
          .canonical  = false,
          .advertized = false,
          .published  = true,
        });
    }

    endResetModel();

    if (canSendStateEvent != canSendCanonicalAliasStateEvent) {
        canSendStateEvent = canSendCanonicalAliasStateEvent;
        emit canAdvertizeChanged();
    }
}

komai::MatrixRoomAliases
AliasEditingModel::desiredAliases() const
{
    komai::MatrixRoomAliases desired;
    desired.canonicalAlias = QString::fromStdString(aliasEvent.alias);

    for (const auto &alias : aliasEvent.alt_aliases)
        desired.altAliases.push_back(QString::fromStdString(alias));

    for (const auto &entry : aliases) {
        if (entry.published)
            desired.publishedAliases.push_back(QString::fromStdString(entry.alias));
    }

    return desired;
}

QHash<int, QByteArray>
AliasEditingModel::roleNames() const
{
    return {
      {Name, "name"},
      {IsPublished, "isPublished"},
      {IsCanonical, "isCanonical"},
      {IsAdvertized, "isAdvertized"},
    };
}

QVariant
AliasEditingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= aliases.size())
        return {};

    const auto &entry = aliases.at(index.row());

    switch (role) {
    case Name:
        return QString::fromStdString(entry.alias);
    case IsPublished:
        return entry.published;
    case IsCanonical:
        return entry.canonical;
    case IsAdvertized:
        return entry.advertized;
    }

    return {};
}

bool
AliasEditingModel::deleteAlias(int row)
{
    if (row < 0 || row >= aliases.size() || aliases.at(row).alias.empty())
        return false;

    const auto alias = aliases.at(row);

    beginRemoveRows(QModelIndex(), row, row);
    aliases.remove(row);
    endRemoveRows();

    if (aliasEvent.alias == alias.alias)
        aliasEvent.alias.clear();

    for (size_t i = 0; i < aliasEvent.alt_aliases.size(); i++) {
        if (aliasEvent.alt_aliases[i] == alias.alias) {
            aliasEvent.alt_aliases.erase(aliasEvent.alt_aliases.begin() + i);
            break;
        }
    }

    return true;
}

void
AliasEditingModel::addAlias(QString newAlias)
{
    const auto aliasStr = newAlias.trimmed().toStdString();
    if (aliasStr.empty())
        return;

    for (const auto &entry : std::as_const(aliases)) {
        if (entry.alias == aliasStr)
            return;
    }

    const bool becomesCanonical = canSendStateEvent && aliasEvent.alias.empty();

    beginInsertRows(QModelIndex(), aliases.length(), aliases.length());
    if (becomesCanonical)
        aliasEvent.alias = aliasStr;

    aliases.push_back(Entry{
      .alias      = aliasStr,
      .canonical  = becomesCanonical,
      .advertized = becomesCanonical,
      .published  = false,
    });
    endInsertRows();
}

void
AliasEditingModel::makeCanonical(int row)
{
    if (!canSendStateEvent || row < 0 || row >= aliases.size() || aliases.at(row).alias.empty())
        return;

    const auto moveAlias = aliases.at(row).alias;

    if (!aliasEvent.alias.empty()) {
        for (int i = 0; i < aliases.size(); i++) {
            if (aliases[i].canonical && aliases[i].alias != moveAlias) {
                aliases[i].canonical = false;
                aliasEvent.alt_aliases.push_back(aliasEvent.alias);
                emit dataChanged(index(i), index(i), {IsCanonical});
                break;
            }
        }
    }

    aliasEvent.alias = moveAlias;
    for (auto i = aliasEvent.alt_aliases.begin(); i != aliasEvent.alt_aliases.end(); ++i) {
        if (*i == moveAlias) {
            aliasEvent.alt_aliases.erase(i);
            break;
        }
    }
    aliases[row].canonical  = true;
    aliases[row].advertized = true;
    emit dataChanged(index(row), index(row), {IsCanonical, IsAdvertized});
}

void
AliasEditingModel::togglePublish(int row)
{
    if (row < 0 || row >= aliases.size() || aliases.at(row).alias.empty())
        return;

    aliases[row].published = !aliases[row].published;
    emit dataChanged(index(row), index(row), {IsPublished});
}

void
AliasEditingModel::toggleAdvertize(int row)
{
    if (!canSendStateEvent || row < 0 || row >= aliases.size() || aliases.at(row).alias.empty())
        return;

    auto &moveAlias = aliases[row];
    if (aliasEvent.alias == moveAlias.alias) {
        moveAlias.canonical  = false;
        moveAlias.advertized = false;
        aliasEvent.alias.clear();
        emit dataChanged(index(row), index(row), {IsAdvertized, IsCanonical});
    } else if (moveAlias.advertized) {
        for (auto i = aliasEvent.alt_aliases.begin(); i != aliasEvent.alt_aliases.end(); ++i) {
            if (*i == moveAlias.alias) {
                aliasEvent.alt_aliases.erase(i);
                moveAlias.advertized = false;
                emit dataChanged(index(row), index(row), {IsAdvertized});
                break;
            }
        }
    } else {
        aliasEvent.alt_aliases.push_back(moveAlias.alias);
        moveAlias.advertized = true;
        emit dataChanged(index(row), index(row), {IsAdvertized});
    }
}

void
AliasEditingModel::commit()
{
    if (committing_)
        return;

    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0)
        return;

    const auto desired = desiredAliases();
    const auto roomId  = QString::fromStdString(room_id);

    committing_ = true;
    emit committingChanged();

    QPointer<AliasEditingModel> self(this);
    std::thread([self, handleId, roomId, desired]() mutable {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const bool ok = komai::MatrixBackendRuntimeService::applyRoomAliases(
          context, handleId, roomId, desired, &error);

        QString refreshAliasesError;
        const auto refreshedAliases = ok ? komai::MatrixBackendRuntimeService::fetchRoomAliases(
                                             context, handleId, roomId, &refreshAliasesError)
                                         : std::optional<komai::MatrixRoomAliases>{};

        QString powerLevelsError;
        const auto powerLevels = ok ? komai::MatrixBackendRuntimeService::fetchRoomPowerLevels(
                                        context, handleId, roomId, &powerLevelsError)
                                    : std::optional<komai::MatrixRoomPowerLevels>{};
        const bool canSendStateEvent =
          powerLevels.has_value() && canChangeCanonicalAlias(*powerLevels);

        auto *app = QCoreApplication::instance();
        if (!app)
            return;

        QMetaObject::invokeMethod(
          app,
          [self,
           roomId,
           desired,
           ok,
           refreshedAliases,
           canSendStateEvent,
           error               = std::move(error),
           refreshAliasesError = std::move(refreshAliasesError),
           powerLevelsError    = std::move(powerLevelsError)]() mutable {
              if (!self)
                  return;

              self->committing_ = false;
              emit self->committingChanged();

              if (!ok) {
                  komai::logging::ui()->warn("Failed to save matrix-sdk room aliases for '{}': {}",
                                             roomId.toStdString(),
                                             error.toStdString());
                  showAliasNotification(AliasEditingModel::tr(
                    "Failed to save room aliases to the matrix-sdk backend."));
                  return;
              }

              if (refreshedAliases.has_value()) {
                  self->applyLoadedState(*refreshedAliases, canSendStateEvent);
                  return;
              }

              if (!refreshAliasesError.isEmpty()) {
                  komai::logging::ui()->warn(
                    "Failed to refresh matrix-sdk room aliases for '{}': {}",
                    roomId.toStdString(),
                    refreshAliasesError.toStdString());
              }
              if (!powerLevelsError.isEmpty()) {
                  komai::logging::ui()->warn(
                    "Failed to refresh matrix-sdk room alias permissions for '{}': {}",
                    roomId.toStdString(),
                    powerLevelsError.toStdString());
              }

              self->applyLoadedState(desired, canSendStateEvent);
          },
          Qt::QueuedConnection);
    }).detach();
}

#include "moc_AliasEditModel.cpp"
