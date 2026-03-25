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
#include "matrix/MatrixMediaUri.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

using namespace mtx::events;

namespace {
mtx::events::state::JoinRule
joinRuleFromKey(const QString &joinRule)
{
    if (joinRule == QLatin1String("public"))
        return mtx::events::state::JoinRule::Public;
    if (joinRule == QLatin1String("knock"))
        return mtx::events::state::JoinRule::Knock;
    if (joinRule == QLatin1String("restricted"))
        return mtx::events::state::JoinRule::Restricted;
    if (joinRule == QLatin1String("knock_restricted"))
        return mtx::events::state::JoinRule::KnockRestricted;
    if (joinRule == QLatin1String("private"))
        return mtx::events::state::JoinRule::Private;

    return mtx::events::state::JoinRule::Invite;
}

QString
joinRuleKey(mtx::events::state::JoinRule joinRule)
{
    using mtx::events::state::JoinRule;

    switch (joinRule) {
    case JoinRule::Public:
        return QStringLiteral("public");
    case JoinRule::Knock:
        return QStringLiteral("knock");
    case JoinRule::Restricted:
        return QStringLiteral("restricted");
    case JoinRule::KnockRestricted:
        return QStringLiteral("knock_restricted");
    case JoinRule::Private:
        return QStringLiteral("private");
    case JoinRule::Invite:
    default:
        return QStringLiteral("invite");
    }
}

mtx::events::state::Visibility
historyVisibilityFromKey(const QString &historyVisibility)
{
    if (historyVisibility == QLatin1String("world_readable"))
        return mtx::events::state::Visibility::WorldReadable;
    if (historyVisibility == QLatin1String("invited"))
        return mtx::events::state::Visibility::Invited;
    if (historyVisibility == QLatin1String("joined"))
        return mtx::events::state::Visibility::Joined;

    return mtx::events::state::Visibility::Shared;
}

QString
historyVisibilityKey(mtx::events::state::Visibility visibility)
{
    using mtx::events::state::Visibility;

    switch (visibility) {
    case Visibility::WorldReadable:
        return QStringLiteral("world_readable");
    case Visibility::Invited:
        return QStringLiteral("invited");
    case Visibility::Joined:
        return QStringLiteral("joined");
    case Visibility::Shared:
    default:
        return QStringLiteral("shared");
    }
}
} // namespace

RoomSettings::RoomSettings(QString roomid, QObject *parent)
  : QObject(parent)
  , roomid_{std::move(roomid)}
{
    connect(this, &RoomSettings::accessJoinRulesChanged, &RoomSettings::allowedRoomsChanged);

    if (loadMatrixRuntimeRoomSettings()) {
        this->allowedRoomsModel = new RoomSettingsAllowedRoomsModel(this);
        return;
    }

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
    if (matrixRoomSettings_)
        return !info_.name.empty();

    return !cache::getStateEvent<mtx::events::state::Name>(roomid_.toStdString())
              .value_or(mtx::events::StateEvent<mtx::events::state::Name>{})
              .content.name.empty();
}

// Deliberately returns the raw cached name without DM-aware overrides.
// Room settings deal with the actual m.room.name state, not presentation names.
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

// Raw name for the settings text field — no DM override, so the user sees and
// edits the actual m.room.name value (or an empty field if none is set).
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
    return komai::matrix::normalizeMxcUri(QString::fromStdString(info_.avatar_url));
}

int
RoomSettings::memberCount() const
{
    return static_cast<int>(info_.member_count);
}

void
RoomSettings::retrieveRoomInfo()
{
    if (loadMatrixRuntimeRoomSettings())
        return;

    try {
        usesEncryption_ = cache::isRoomEncrypted(roomid_.toStdString());
        info_           = cache::singleRoomInfo(roomid_.toStdString());
    } catch (const std::exception &) {
        nhlog::db()->warn("failed to retrieve room info from cache: {}", roomid_.toStdString());
    }
}

bool
RoomSettings::loadMatrixRuntimeRoomSettings(QString *errorOut)
{
    const auto handleId = matrixBackendHandleId();
    if (handleId == 0)
        return false;

    auto result =
      komai::MatrixBackendRuntimeService::fetchRoomSettings(handleId, roomid_, errorOut);
    if (!result.has_value())
        return false;

    matrixRoomSettings_ = *result;
    info_.name          = result->roomName.toStdString();
    info_.topic         = result->roomTopic.toStdString();
    info_.avatar_url    = result->roomAvatarUrl.toStdString();
    info_.version       = result->roomVersion.toStdString();
    info_.member_count  = static_cast<size_t>(result->memberCount);

    notifications_     = result->notifications;
    usesEncryption_    = result->isEncrypted;
    guestRules_        = result->guestAccess ? mtx::events::state::AccessState::CanJoin
                                             : mtx::events::state::AccessState::Forbidden;
    historyVisibility_ = historyVisibilityFromKey(result->historyVisibility);

    accessRules_           = {};
    accessRules_.join_rule = joinRuleFromKey(result->joinRule);
    accessRules_.allow.clear();
    for (const auto &roomId : result->allowedRoomIds) {
        accessRules_.allow.push_back(
          {mtx::events::state::JoinAllowanceType::RoomMembership, roomId.toStdString()});
    }

    return true;
}

uint64_t
RoomSettings::matrixBackendHandleId() const
{
    const auto *mainWindow = MainWindow::instance();
    return mainWindow ? mainWindow->matrixBackendHandleId() : 0;
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

    if (matrixRoomSettings_ && matrixBackendHandleId() != 0) {
        QString error;
        if (!komai::MatrixBackendRuntimeService::enableRoomEncryption(
              matrixBackendHandleId(), roomid_, &error)) {
            emit displayError(error.isEmpty() ? tr("Failed to enable encryption.") : error);
            usesEncryption_ = false;
            emit encryptionChanged();
            return;
        }

        usesEncryption_ = true;
        if (matrixRoomSettings_)
            matrixRoomSettings_->isEncrypted = true;
        emit encryptionChanged();
        return;
    }

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
    if (matrixRoomSettings_)
        return matrixRoomSettings_->canChangeName;

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
    if (matrixRoomSettings_)
        return matrixRoomSettings_->canChangeTopic;

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
    if (matrixRoomSettings_)
        return matrixRoomSettings_->canChangeAvatar;

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

    if (matrixRoomSettings_ && matrixBackendHandleId() != 0) {
        QString error;
        if (!komai::MatrixBackendRuntimeService::setRoomNotificationMode(
              matrixBackendHandleId(), roomid_, currentIndex, &error)) {
            emit displayError(error.isEmpty() ? tr("Failed to update notifications.") : error);
            return;
        }

        if (matrixRoomSettings_)
            matrixRoomSettings_->notifications = currentIndex;
        emit notificationsChanged();
        return;
    }

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

    if (matrixRoomSettings_ && matrixBackendHandleId() != 0) {
        QString error;
        if (!komai::MatrixBackendRuntimeService::setRoomName(
              matrixBackendHandleId(), roomid_, QString::fromStdString(newName), &error)) {
            emit displayError(error.isEmpty() ? tr("Failed to update room name.") : error);
            return;
        }

        this->info_.name = newName;
        if (matrixRoomSettings_)
            matrixRoomSettings_->roomName = QString::fromStdString(newName);
        emit roomNameChanged();
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

    if (matrixRoomSettings_ && matrixBackendHandleId() != 0) {
        QString error;
        if (!komai::MatrixBackendRuntimeService::setRoomTopic(
              matrixBackendHandleId(), roomid_, QString::fromStdString(newTopic), &error)) {
            emit displayError(error.isEmpty() ? tr("Failed to update room topic.") : error);
            return;
        }

        this->info_.topic = newTopic;
        if (matrixRoomSettings_)
            matrixRoomSettings_->roomTopic = QString::fromStdString(newTopic);
        emit roomTopicChanged();
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

QStringList
RoomSettings::parentSpaceRoomIds() const
{
    if (matrixRoomSettings_)
        return matrixRoomSettings_->parentSpaceRoomIds;

    QStringList parentSpaceRoomIds;
    for (const auto &roomId : cache::getParentRoomIds(roomid_.toStdString()))
        parentSpaceRoomIds.push_back(QString::fromStdString(roomId));
    return parentSpaceRoomIds;
}

#include "moc_RoomSettings.cpp"
