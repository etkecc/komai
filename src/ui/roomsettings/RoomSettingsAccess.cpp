// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomSettings.h"

#include <cassert>
#include <limits>
#include <set>
#include <string_view>

#include <mtx/events/event_type.hpp>
#include <mtx/responses/common.hpp>

#include "cache/Cache.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "utils/Utils.h"

using namespace mtx::events;

RoomSettings::Visibility
RoomSettings::historyVisibility() const
{
    switch (this->historyVisibility_) {
    case mtx::events::state::Visibility::WorldReadable:
        return WorldReadable;
    case mtx::events::state::Visibility::Joined:
        return Joined;
    case mtx::events::state::Visibility::Invited:
        return Invited;
    case mtx::events::state::Visibility::Shared:
        return Shared;
    }
    return Shared;
}

bool
RoomSettings::privateAccess() const
{
    return accessRules_.join_rule != mtx::events::state::JoinRule::Public;
}

bool
RoomSettings::guestAccess() const
{
    return guestRules_ == mtx::events::state::AccessState::CanJoin;
}

bool
RoomSettings::knockingEnabled() const
{
    return accessRules_.join_rule == mtx::events::state::JoinRule::Knock ||
           accessRules_.join_rule == mtx::events::state::JoinRule::KnockRestricted;
}

bool
RoomSettings::restrictedEnabled() const
{
    return accessRules_.join_rule == mtx::events::state::JoinRule::Restricted ||
           accessRules_.join_rule == mtx::events::state::JoinRule::KnockRestricted;
}

QStringList
RoomSettings::allowedRooms() const
{
    QStringList rooms;
    assert(accessRules_.allow.size() < std::numeric_limits<int>::max());
    rooms.reserve(static_cast<int>(accessRules_.allow.size()));
    for (const auto &e : accessRules_.allow) {
        if (e.type == mtx::events::state::JoinAllowanceType::RoomMembership)
            rooms.push_back(QString::fromStdString(e.room_id));
    }
    return rooms;
}

void
RoomSettings::setAllowedRooms(QStringList rooms)
{
    accessRules_.allow.clear();
    for (const auto &e : rooms) {
        accessRules_.allow.push_back(
          {mtx::events::state::JoinAllowanceType::RoomMembership, e.toStdString()});
    }
}

bool
RoomSettings::canChangeJoinRules() const
{
    try {
        return cache::hasEnoughPowerLevel(
          {EventType::RoomJoinRules}, roomid_.toStdString(), utils::localUser().toStdString());
    } catch (const std::exception &e) {
        nhlog::db()->warn("database error: {}", e.what());
    }

    return false;
}

bool
RoomSettings::canChangeHistoryVisibility() const
{
    try {
        return cache::hasEnoughPowerLevel({EventType::RoomHistoryVisibility},
                                          roomid_.toStdString(),
                                          utils::localUser().toStdString());
    } catch (const std::exception &e) {
        nhlog::db()->warn("database error: {}", e.what());
    }

    return false;
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
    using namespace mtx::events::state;

    auto guest_access = [guestsAllowed]() -> state::GuestAccess {
        state::GuestAccess event;

        if (guestsAllowed)
            event.guest_access = state::AccessState::CanJoin;
        else
            event.guest_access = state::AccessState::Forbidden;

        return event;
    }();

    auto join_rule = [this, private_, knockingAllowed, restrictedAllowed]() -> state::JoinRules {
        state::JoinRules event = this->accessRules_;

        if (!private_) {
            event.join_rule = state::JoinRule::Public;
        } else if (knockingAllowed && restrictedAllowed && supportsKnockRestricted()) {
            event.join_rule = state::JoinRule::KnockRestricted;
        } else if (knockingAllowed && supportsKnocking()) {
            event.join_rule = state::JoinRule::Knock;
        } else if (restrictedAllowed && supportsRestricted()) {
            event.join_rule = state::JoinRule::Restricted;
        } else {
            event.join_rule = state::JoinRule::Invite;
        }

        return event;
    }();

    updateAccessRules(roomid_.toStdString(), join_rule, guest_access);
}

void
RoomSettings::changeHistoryVisibility(Visibility value)
{
    auto tempVis = mtx::events::state::Visibility::Shared;

    switch (value) {
    case WorldReadable:
        tempVis = mtx::events::state::Visibility::WorldReadable;
        break;
    case Joined:
        tempVis = mtx::events::state::Visibility::Joined;
        break;
    case Invited:
        tempVis = mtx::events::state::Visibility::Invited;
        break;
    case Shared:
        tempVis = mtx::events::state::Visibility::Shared;
        break;
    default:
        return;
    }

    auto proxy = std::make_shared<ThreadProxy>();
    connect(proxy.get(), &ThreadProxy::eventSent, this, [this, tempVis]() {
        this->historyVisibility_ = tempVis;
        emit historyVisibilityChanged();
    });
    connect(proxy.get(), &ThreadProxy::error, this, &RoomSettings::displayError);

    state::HistoryVisibility body;
    body.history_visibility = tempVis;

    http::client()->send_state_event(
      roomid_.toStdString(),
      body,
      [proxy](const mtx::responses::EventId &, mtx::http::RequestErr err) {
          if (err) {
              emit proxy->error(QString::fromStdString(err->matrix_error.error));
              return;
          }

          emit proxy->eventSent();
      });
}

void
RoomSettings::updateAccessRules(const std::string &room_id,
                                const mtx::events::state::JoinRules &join_rule,
                                const mtx::events::state::GuestAccess &guest_access)
{
    isLoading_            = true;
    allowedRoomsModified_ = false;
    emit loadingChanged();
    emit allowedRoomsModifiedChanged();

    http::client()->send_state_event(
      room_id,
      join_rule,
      [this, room_id, guest_access, join_rule](const mtx::responses::EventId &,
                                               mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->warn("failed to send m.room.join_rule: {} {}",
                                 static_cast<int>(err->status_code),
                                 err->matrix_error.error);
              emit displayError(QString::fromStdString(err->matrix_error.error));
              isLoading_ = false;
              emit loadingChanged();
              return;
          }

          http::client()->send_state_event(
            room_id,
            guest_access,
            [this, join_rule](const mtx::responses::EventId &, mtx::http::RequestErr err) {
                if (err) {
                    nhlog::net()->warn("failed to send m.room.guest_access: {} {}",
                                       static_cast<int>(err->status_code),
                                       err->matrix_error.error);
                    emit displayError(QString::fromStdString(err->matrix_error.error));
                }

                isLoading_ = false;
                emit loadingChanged();

                this->accessRules_ = join_rule;
                emit accessJoinRulesChanged();
            });
      });
}

void
RoomSettings::applyAllowedFromModel()
{
    this->setAllowedRooms(this->allowedRoomsModel->allowedRoomIds);
    this->allowedRoomsModified_ = true;
    emit allowedRoomsModifiedChanged();
}
