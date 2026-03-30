// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomSettings.h"

#include <cassert>
#include <limits>
#include <set>
#include <string_view>

#include "utils/QtWorkerTask.h"

namespace {
constexpr auto WorldReadableKey = "world_readable";
constexpr auto SharedKey        = "shared";
constexpr auto InvitedKey       = "invited";
constexpr auto JoinedKey        = "joined";
} // namespace

RoomSettings::Visibility
RoomSettings::historyVisibility() const
{
    if (historyVisibilityKey_ == QLatin1String(WorldReadableKey))
        return WorldReadable;
    if (historyVisibilityKey_ == QLatin1String(JoinedKey))
        return Joined;
    if (historyVisibilityKey_ == QLatin1String(InvitedKey))
        return Invited;
    return Shared;
}

bool
RoomSettings::privateAccess() const
{
    return joinRule_ != QLatin1String("public");
}

bool
RoomSettings::guestAccess() const
{
    return guestAccess_;
}

bool
RoomSettings::knockingEnabled() const
{
    return joinRule_ == QLatin1String("knock") || joinRule_ == QLatin1String("knock_restricted");
}

bool
RoomSettings::restrictedEnabled() const
{
    return joinRule_ == QLatin1String("restricted") ||
           joinRule_ == QLatin1String("knock_restricted");
}

QStringList
RoomSettings::allowedRooms() const
{
    assert(allowedRoomIds_.size() < std::numeric_limits<int>::max());
    QStringList rooms;
    rooms.reserve(static_cast<int>(allowedRoomIds_.size()));
    for (const auto &roomId : allowedRoomIds_)
        rooms.push_back(roomId);
    return rooms;
}

void
RoomSettings::setAllowedRooms(QStringList rooms)
{
    allowedRoomIds_ = QVector<QString>(rooms.begin(), rooms.end());
}

bool
RoomSettings::canChangeJoinRules() const
{
    return matrixRoomSettings_ && matrixRoomSettings_->canChangeJoinRules;
}

bool
RoomSettings::canChangeHistoryVisibility() const
{
    return matrixRoomSettings_ && matrixRoomSettings_->canChangeHistoryVisibility;
}

bool
RoomSettings::supportsKnocking() const
{
    const static std::set<std::string_view> unsupported{
      "",
      "1",
      "2",
      "3",
      "4",
      "5",
      "6",
    };
    return !unsupported.count(info_.version);
}

bool
RoomSettings::supportsRestricted() const
{
    const static std::set<std::string_view> unsupported{
      "",
      "1",
      "2",
      "3",
      "4",
      "5",
      "6",
      "7",
    };
    return !unsupported.count(info_.version);
}

bool
RoomSettings::supportsKnockRestricted() const
{
    const static std::set<std::string_view> unsupported{
      "",
      "1",
      "2",
      "3",
      "4",
      "5",
      "6",
      "7",
      "8",
      "9",
    };
    return !unsupported.count(info_.version);
}

void
RoomSettings::changeAccessRules(bool private_,
                                bool guestsAllowed,
                                bool knockingAllowed,
                                bool restrictedAllowed)
{
    QString joinRule = QStringLiteral("invite");
    if (!private_) {
        joinRule = QStringLiteral("public");
    } else if (knockingAllowed && restrictedAllowed && supportsKnockRestricted()) {
        joinRule = QStringLiteral("knock_restricted");
    } else if (knockingAllowed && supportsKnocking()) {
        joinRule = QStringLiteral("knock");
    } else if (restrictedAllowed && supportsRestricted()) {
        joinRule = QStringLiteral("restricted");
    }

    updateAccessRules(joinRule, guestsAllowed, allowedRoomIds_);
}

void
RoomSettings::changeHistoryVisibility(Visibility value)
{
    if (isLoading_)
        return;

    QString historyVisibility = QLatin1String(SharedKey);

    switch (value) {
    case WorldReadable:
        historyVisibility = QLatin1String(WorldReadableKey);
        break;
    case Joined:
        historyVisibility = QLatin1String(JoinedKey);
        break;
    case Invited:
        historyVisibility = QLatin1String(InvitedKey);
        break;
    case Shared:
        historyVisibility = QLatin1String(SharedKey);
        break;
    default:
        return;
    }

    const auto handleId = matrixBackendHandleId();
    isLoading_          = true;
    emit loadingChanged();

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId = roomid_, historyVisibility]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::setRoomHistoryVisibility(
            context, handleId, roomId, historyVisibility, &error);
          return std::make_pair(ok, error);
      },
      [historyVisibility](RoomSettings *settings, const std::pair<bool, QString> &result) {
          settings->isLoading_ = false;
          emit settings->loadingChanged();

          const auto &[ok, error] = result;
          if (!ok) {
              emit settings->displayError(
                error.isEmpty() ? tr("Failed to update history visibility.") : error);
              return;
          }

          settings->historyVisibilityKey_ = historyVisibility;
          if (settings->matrixRoomSettings_)
              settings->matrixRoomSettings_->historyVisibility = historyVisibility;
          emit settings->historyVisibilityChanged();
      });
}

void
RoomSettings::updateAccessRules(const QString &joinRule,
                                bool guestAccess,
                                const QVector<QString> &allowedRoomIds)
{
    if (isLoading_)
        return;

    isLoading_            = true;
    allowedRoomsModified_ = false;
    emit loadingChanged();
    emit allowedRoomsModifiedChanged();

    const auto handleId = matrixBackendHandleId();
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId = roomid_, joinRule, guestAccess, allowedRoomIds]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::setRoomAccessRules(
            context, handleId, roomId, joinRule, guestAccess, allowedRoomIds, &error);
          return std::make_pair(ok, error);
      },
      [joinRule, guestAccess, allowedRoomIds](RoomSettings *settings,
                                              const std::pair<bool, QString> &result) {
          settings->isLoading_ = false;
          emit settings->loadingChanged();

          const auto &[ok, error] = result;
          if (!ok) {
              emit settings->displayError(
                error.isEmpty() ? tr("Failed to update room access rules.") : error);
              return;
          }

          settings->joinRule_       = joinRule;
          settings->guestAccess_    = guestAccess;
          settings->allowedRoomIds_ = allowedRoomIds;
          if (settings->matrixRoomSettings_) {
              settings->matrixRoomSettings_->joinRule       = joinRule;
              settings->matrixRoomSettings_->guestAccess    = guestAccess;
              settings->matrixRoomSettings_->allowedRoomIds = allowedRoomIds;
          }

          emit settings->accessJoinRulesChanged();
      });
}

void
RoomSettings::applyAllowedFromModel()
{
    this->setAllowedRooms(this->allowedRoomsModel->allowedRoomIds);
    this->allowedRoomsModified_ = true;
    emit allowedRoomsModifiedChanged();
}
