// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomSettings.h"

#include <mtx/events/event_type.hpp>
#include <mtx/responses/common.hpp>

#include "cache/Cache.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "utils/Utils.h"

using namespace mtx::events;

RoomSettings::RoomSettings(QString roomid, QObject *parent)
  : QObject(parent)
  , roomid_{std::move(roomid)}
{
    connect(this, &RoomSettings::accessJoinRulesChanged, &RoomSettings::allowedRoomsChanged);
    retrieveRoomInfo();

    // get room setting notifications
    http::client()->get_pushrules(
      "global",
      "override",
      roomid_.toStdString(),
      [this](const mtx::pushrules::PushRule &rule, mtx::http::RequestErr err) {
          if (err) {
              if (err->status_code == 404)
                  http::client()->get_pushrules(
                    "global",
                    "room",
                    roomid_.toStdString(),
                    [this](const mtx::pushrules::PushRule &rule, mtx::http::RequestErr err) {
                        if (err) {
                            notifications_ = 2; // all messages
                            emit notificationsChanged();
                            return;
                        }

                        if (rule.enabled) {
                            notifications_ = 1; // mentions only
                            emit notificationsChanged();
                        }
                    });
              return;
          }

          if (rule.enabled) {
              notifications_ = 0; // muted
              emit notificationsChanged();
          } else {
              notifications_ = 2; // all messages
              emit notificationsChanged();
          }
      });

    // access rules
    this->accessRules_ = cache::getStateEvent<mtx::events::state::JoinRules>(roomid_.toStdString())
                           .value_or(mtx::events::StateEvent<mtx::events::state::JoinRules>{})
                           .content;
    using mtx::events::state::AccessState;
    guestRules_ = info_.guest_access ? AccessState::CanJoin : AccessState::Forbidden;
    emit accessJoinRulesChanged();

    if (auto ev =
          cache::getStateEvent<mtx::events::state::HistoryVisibility>(roomid_.toStdString())) {
        this->historyVisibility_ = ev->content.history_visibility;
    }

    this->allowedRoomsModel = new RoomSettingsAllowedRoomsModel(this);
}

bool
RoomSettings::isRoomNameSet() const
{
    return !cache::getStateEvent<mtx::events::state::Name>(roomid_.toStdString())
              .value_or(mtx::events::StateEvent<mtx::events::state::Name>{})
              .content.name.empty();
}

QString
RoomSettings::roomName() const
{
    return utils::replaceEmoji(QString::fromStdString(info_.name).toHtmlEscaped());
}

QString
RoomSettings::roomTopic() const
{
    return utils::replaceEmoji(
      utils::linkifyMessage(QString::fromStdString(info_.topic)
                              .toHtmlEscaped()
                              .replace(QLatin1String("\n"), QLatin1String("<br>"))));
}

QString
RoomSettings::plainRoomName() const
{
    return QString::fromStdString(info_.name);
}

QString
RoomSettings::plainRoomTopic() const
{
    return QString::fromStdString(info_.topic);
}

QString
RoomSettings::roomId() const
{
    return roomid_;
}

QString
RoomSettings::roomVersion() const
{
    return QString::fromStdString(info_.version);
}

bool
RoomSettings::isLoading() const
{
    return isLoading_;
}

QString
RoomSettings::roomAvatarUrl()
{
    return QString::fromStdString(info_.avatar_url);
}

int
RoomSettings::memberCount() const
{
    return static_cast<int>(info_.member_count);
}

void
RoomSettings::retrieveRoomInfo()
{
    try {
        usesEncryption_ = cache::isRoomEncrypted(roomid_.toStdString());
        info_           = cache::singleRoomInfo(roomid_.toStdString());
    } catch (const std::exception &) {
        nhlog::db()->warn("failed to retrieve room info from cache: {}", roomid_.toStdString());
    }
}

int
RoomSettings::notifications()
{
    return notifications_;
}

void
RoomSettings::enableEncryption()
{
    if (usesEncryption_)
        return;

    const auto room_id = roomid_.toStdString();
    http::client()->enable_encryption(
      room_id, [room_id, this](const mtx::responses::EventId &, mtx::http::RequestErr err) {
          if (err) {
              int status_code = static_cast<int>(err->status_code);
              nhlog::net()->warn("failed to enable encryption in room ({}): {} {}",
                                 room_id,
                                 err->matrix_error.error,
                                 status_code);
              emit displayError(tr("Failed to enable encryption: %1")
                                  .arg(QString::fromStdString(err->matrix_error.error)));
              usesEncryption_ = false;
              emit encryptionChanged();
              return;
          }

          nhlog::net()->info("enabled encryption on room ({})", room_id);
      });

    usesEncryption_ = true;
    emit encryptionChanged();
}

bool
RoomSettings::canChangeName() const
{
    try {
        return cache::hasEnoughPowerLevel(
          {EventType::RoomName}, roomid_.toStdString(), utils::localUser().toStdString());
    } catch (const std::exception &e) {
        nhlog::db()->warn("database error: {}", e.what());
    }

    return false;
}

bool
RoomSettings::canChangeTopic() const
{
    try {
        return cache::hasEnoughPowerLevel(
          {EventType::RoomTopic}, roomid_.toStdString(), utils::localUser().toStdString());
    } catch (const std::exception &e) {
        nhlog::db()->warn("database error: {}", e.what());
    }

    return false;
}

bool
RoomSettings::canChangeAvatar() const
{
    try {
        return cache::hasEnoughPowerLevel(
          {EventType::RoomAvatar}, roomid_.toStdString(), utils::localUser().toStdString());
    } catch (const std::exception &e) {
        nhlog::db()->warn("database error: {}", e.what());
    }

    return false;
}

bool
RoomSettings::isEncryptionEnabled() const
{
    return usesEncryption_;
}

void
RoomSettings::changeNotifications(int currentIndex)
{
    notifications_ = currentIndex;

    std::string room_id = roomid_.toStdString();
    if (notifications_ == 0) {
        // mute room
        // delete old rule first, then add new rule
        mtx::pushrules::PushRule rule;
        rule.actions = {mtx::pushrules::actions::dont_notify{}};
        mtx::pushrules::PushCondition condition;
        condition.kind    = "event_match";
        condition.key     = "room_id";
        condition.pattern = room_id;
        rule.conditions   = {condition};

        http::client()->put_pushrules(
          "global", "override", room_id, rule, [room_id](mtx::http::RequestErr &err) {
              if (err)
                  nhlog::net()->error("failed to set pushrule for room {}: {} {}",
                                      room_id,
                                      static_cast<int>(err->status_code),
                                      err->matrix_error.error);
              http::client()->delete_pushrules(
                "global", "room", room_id, [room_id](mtx::http::RequestErr &) {});
          });
    } else if (notifications_ == 1) {
        // mentions only
        // delete old rule first, then add new rule
        mtx::pushrules::PushRule rule;
        rule.actions = {mtx::pushrules::actions::dont_notify{}};
        http::client()->put_pushrules(
          "global", "room", room_id, rule, [room_id](mtx::http::RequestErr &err) {
              if (err)
                  nhlog::net()->error("failed to set pushrule for room {}: {} {}",
                                      room_id,
                                      static_cast<int>(err->status_code),
                                      err->matrix_error.error);
              http::client()->delete_pushrules(
                "global", "override", room_id, [room_id](mtx::http::RequestErr &) {});
          });
    } else {
        // all messages
        http::client()->delete_pushrules(
          "global", "override", room_id, [room_id](mtx::http::RequestErr &) {
              http::client()->delete_pushrules(
                "global", "room", room_id, [room_id](mtx::http::RequestErr &) {});
          });
    }
}

void
RoomSettings::changeName(const QString &name)
{
    // Check if the values are changed from the originals.
    auto newName = name.trimmed().toStdString();

    if (newName == info_.name) {
        return;
    }

    using namespace mtx::events;
    auto proxy = std::make_shared<ThreadProxy>();
    connect(proxy.get(), &ThreadProxy::nameEventSent, this, [this](const QString &newRoomName) {
        this->info_.name = newRoomName.toStdString();
        emit roomNameChanged();
    });
    connect(proxy.get(), &ThreadProxy::error, this, &RoomSettings::displayError);

    state::Name body;
    body.name = newName;

    http::client()->send_state_event(
      roomid_.toStdString(),
      body,
      [proxy, newName](const mtx::responses::EventId &, mtx::http::RequestErr err) {
          if (err) {
              emit proxy->error(QString::fromStdString(err->matrix_error.error));
              return;
          }

          emit proxy->nameEventSent(QString::fromStdString(newName));
      });
}

void
RoomSettings::changeTopic(const QString &topic)
{
    // Check if the values are changed from the originals.
    auto newTopic = topic.trimmed().toStdString();

    if (newTopic == info_.topic) {
        return;
    }

    using namespace mtx::events;
    auto proxy = std::make_shared<ThreadProxy>();
    connect(proxy.get(), &ThreadProxy::topicEventSent, this, [this](const QString &newRoomTopic) {
        this->info_.topic = newRoomTopic.toStdString();
        emit roomTopicChanged();
    });
    connect(proxy.get(), &ThreadProxy::error, this, &RoomSettings::displayError);

    state::Topic body;
    body.topic = newTopic;

    http::client()->send_state_event(
      roomid_.toStdString(),
      body,
      [proxy, newTopic](const mtx::responses::EventId &, mtx::http::RequestErr err) {
          if (err) {
              emit proxy->error(QString::fromStdString(err->matrix_error.error));
              return;
          }

          emit proxy->topicEventSent(QString::fromStdString(newTopic));
      });
}

#include "moc_RoomSettings.cpp"
