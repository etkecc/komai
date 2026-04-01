// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Permissions.h"

#include <QCoreApplication>
#include <QPointer>

#include <limits>
#include <thread>

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "timeline/TimelineEventTypes.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

namespace {

QString
matrixEventTypeKey(qml_mtx_events::EventType eventType)
{
    using EventType = qml_mtx_events::EventType;

    switch (eventType) {
    case EventType::KeyRequest:
        return QStringLiteral("m.room_key_request");
    case EventType::Reaction:
        return QStringLiteral("m.reaction");
    case EventType::Aliases:
        return QStringLiteral("m.room.aliases");
    case EventType::Avatar:
        return QStringLiteral("m.room.avatar");
    case EventType::CallInvite:
        return QStringLiteral("m.call.invite");
    case EventType::CallAnswer:
        return QStringLiteral("m.call.answer");
    case EventType::CallHangUp:
        return QStringLiteral("m.call.hangup");
    case EventType::CallCandidates:
        return QStringLiteral("m.call.candidates");
    case EventType::CallSelectAnswer:
        return QStringLiteral("m.call.select_answer");
    case EventType::CallReject:
        return QStringLiteral("m.call.reject");
    case EventType::CallNegotiate:
        return QStringLiteral("m.call.negotiate");
    case EventType::CanonicalAlias:
        return QStringLiteral("m.room.canonical_alias");
    case EventType::RoomCreate:
        return QStringLiteral("m.room.create");
    case EventType::Encrypted:
        return QStringLiteral("m.room.encrypted");
    case EventType::Encryption:
        return QStringLiteral("m.room.encryption");
    case EventType::RoomGuestAccess:
        return QStringLiteral("m.room.guest_access");
    case EventType::RoomHistoryVisibility:
        return QStringLiteral("m.room.history_visibility");
    case EventType::RoomJoinRules:
        return QStringLiteral("m.room.join_rules");
    case EventType::Member:
        return QStringLiteral("m.room.member");
    case EventType::Name:
        return QStringLiteral("m.room.name");
    case EventType::PowerLevels:
        return QStringLiteral("m.room.power_levels");
    case EventType::Tombstone:
        return QStringLiteral("m.room.tombstone");
    case EventType::ServerAcl:
        return QStringLiteral("m.room.server_acl");
    case EventType::Topic:
        return QStringLiteral("m.room.topic");
    case EventType::Redaction:
        return QStringLiteral("m.room.redaction");
    case EventType::PinnedEvents:
        return QStringLiteral("m.room.pinned_events");
    case EventType::Sticker:
        return QStringLiteral("m.sticker");
    case EventType::Tag:
        return QStringLiteral("m.tag");
    case EventType::Widget:
        return QStringLiteral("m.widget");
    case EventType::AudioMessage:
    case EventType::ElementEffectMessage:
    case EventType::EmoteMessage:
    case EventType::FileMessage:
    case EventType::ImageMessage:
    case EventType::LocationMessage:
    case EventType::NoticeMessage:
    case EventType::TextMessage:
    case EventType::UnknownMessage:
    case EventType::VideoMessage:
        return QStringLiteral("m.room.message");
    case EventType::KeyVerificationRequest:
        return QStringLiteral("m.key.verification.request");
    case EventType::KeyVerificationStart:
        return QStringLiteral("m.key.verification.start");
    case EventType::KeyVerificationMac:
        return QStringLiteral("m.key.verification.mac");
    case EventType::KeyVerificationAccept:
        return QStringLiteral("m.key.verification.accept");
    case EventType::KeyVerificationCancel:
        return QStringLiteral("m.key.verification.cancel");
    case EventType::KeyVerificationKey:
        return QStringLiteral("m.key.verification.key");
    case EventType::KeyVerificationDone:
        return QStringLiteral("m.key.verification.done");
    case EventType::KeyVerificationReady:
        return QStringLiteral("m.key.verification.ready");
    case EventType::ImagePackInRoom:
        return QStringLiteral("im.ponies.room_emotes");
    case EventType::ImagePackInAccountData:
        return QStringLiteral("im.ponies.user_emotes");
    case EventType::ImagePackRooms:
        return QStringLiteral("im.ponies.emote_rooms");
    case EventType::PolicyRuleUser:
        return QStringLiteral("m.policy.rule.user");
    case EventType::PolicyRuleRoom:
        return QStringLiteral("m.policy.rule.room");
    case EventType::PolicyRuleServer:
        return QStringLiteral("m.policy.rule.server");
    case EventType::SpaceParent:
        return QStringLiteral("m.space.parent");
    case EventType::SpaceChild:
        return QStringLiteral("m.space.child");
    case EventType::Unsupported:
    case EventType::Redacted:
    case EventType::UnknownEvent:
    default:
        return {};
    }
}

bool
isStateEventType(qml_mtx_events::EventType eventType)
{
    using EventType = qml_mtx_events::EventType;

    switch (eventType) {
    case EventType::Aliases:
    case EventType::Avatar:
    case EventType::CanonicalAlias:
    case EventType::RoomCreate:
    case EventType::Encryption:
    case EventType::RoomGuestAccess:
    case EventType::RoomHistoryVisibility:
    case EventType::RoomJoinRules:
    case EventType::Member:
    case EventType::Name:
    case EventType::PowerLevels:
    case EventType::Tombstone:
    case EventType::ServerAcl:
    case EventType::Topic:
    case EventType::PinnedEvents:
    case EventType::Tag:
    case EventType::Widget:
    case EventType::ImagePackInRoom:
    case EventType::PolicyRuleUser:
    case EventType::PolicyRuleRoom:
    case EventType::PolicyRuleServer:
    case EventType::SpaceParent:
    case EventType::SpaceChild:
        return true;
    default:
        return false;
    }
}

qlonglong
eventLevelForKey(const komai::MatrixRoomPowerLevels &powerLevels, QStringView key, bool stateEvent)
{
    if (!key.isEmpty()) {
        for (const auto &entry : powerLevels.events) {
            if (entry.key == key)
                return entry.level;
        }
    }

    return stateEvent ? powerLevels.stateDefault : powerLevels.eventsDefault;
}

} // namespace

Permissions::Permissions(QString roomId, QObject *parent)
  : AbstractPermissions(parent)
  , roomId_(std::move(roomId))
{
    invalidate();
}

void
Permissions::invalidate()
{
    nhlog::ui()->warn("Using conservative default room permissions for '{}' until matrix-sdk "
                      "power-level fetch is migrated",
                      roomId_.toStdString());
    powerLevels_ = {};
}

bool
Permissions::canInvite()
{
    return false;
}

bool
Permissions::canBan()
{
    return false;
}

bool
Permissions::canKick()
{
    return false;
}

bool
Permissions::canRedact()
{
    return false;
}

bool
Permissions::canChange(int eventType)
{
    Q_UNUSED(eventType);
    return false;
}

bool
Permissions::canSend(int eventType)
{
    Q_UNUSED(eventType);
    return true;
}

int
Permissions::defaultLevel()
{
    return static_cast<int>(powerLevels_.usersDefault);
}

int
Permissions::redactLevel()
{
    return static_cast<int>(powerLevels_.redact);
}

int
Permissions::changeLevel(int eventType)
{
    Q_UNUSED(eventType);
    return static_cast<int>(powerLevels_.stateDefault);
}

int
Permissions::sendLevel(int eventType)
{
    Q_UNUSED(eventType);
    return static_cast<int>(powerLevels_.eventsDefault);
}

bool
Permissions::canPingRoom()
{
    return true;
}

MatrixRoomPermissions::MatrixRoomPermissions(QObject *parent)
  : AbstractPermissions(parent)
{
}

void
MatrixRoomPermissions::setRoomId(QString roomId)
{
    roomId = roomId.trimmed();
    if (roomId_ == roomId)
        return;

    roomId_ = std::move(roomId);
    ++requestToken_;
    clearLoadedState();

    emit roomIdChanged();

    if (!roomId_.isEmpty())
        refreshAsync();
}

void
MatrixRoomPermissions::clearLoadedState()
{
    powerLevels_ = {};
    loaded_      = false;
    ++revision_;
    emit revisionChanged();
}

void
MatrixRoomPermissions::refreshAsync()
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || roomId_.isEmpty())
        return;

    const auto roomId       = roomId_;
    const auto requestToken = requestToken_;
    QPointer<MatrixRoomPermissions> self(this);

    std::thread([self, handleId, roomId, requestToken]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const auto powerLevels = komai::MatrixBackendRuntimeService::fetchRoomPowerLevels(
          context, handleId, roomId, &error);

        auto *app = QCoreApplication::instance();
        if (!app)
            return;

        QMetaObject::invokeMethod(
          app,
          [self, handleId, roomId, requestToken, powerLevels, error = std::move(error)]() mutable {
              if (!self)
                  return;

              auto *mainWindow = MainWindow::instance();
              if (!mainWindow || mainWindow->matrixBackendHandleId() != handleId)
                  return;

              if (self->requestToken_ != requestToken || self->roomId_ != roomId)
                  return;

              if (!powerLevels) {
                  if (!error.isEmpty()) {
                      nhlog::ui()->warn("Failed to fetch matrix-sdk room power levels for '{}': {}",
                                        roomId.toStdString(),
                                        error.toStdString());
                  }
                  return;
              }

              if (self->loaded_ && self->powerLevels_ == *powerLevels)
                  return;

              self->powerLevels_ = *powerLevels;
              self->loaded_      = true;
              ++self->revision_;
              emit self->revisionChanged();
          },
          Qt::QueuedConnection);
    }).detach();
}

qlonglong
MatrixRoomPermissions::currentUserPowerLevel() const
{
    return komai::matrix::effectiveUserPowerLevel(powerLevels_, utils::localUser().trimmed());
}

qlonglong
MatrixRoomPermissions::requiredEventLevel(int eventType) const
{
    const auto key = matrixEventTypeKey(static_cast<qml_mtx_events::EventType>(eventType));
    if (key.isEmpty())
        return std::numeric_limits<qlonglong>::max();

    return eventLevelForKey(
      powerLevels_, key, isStateEventType(static_cast<qml_mtx_events::EventType>(eventType)));
}

bool
MatrixRoomPermissions::canInvite()
{
    return loaded_ && currentUserPowerLevel() >= powerLevels_.invite;
}

bool
MatrixRoomPermissions::canBan()
{
    return loaded_ && currentUserPowerLevel() >= powerLevels_.ban;
}

bool
MatrixRoomPermissions::canKick()
{
    return loaded_ && currentUserPowerLevel() >= powerLevels_.kick;
}

bool
MatrixRoomPermissions::canRedact()
{
    return loaded_ && currentUserPowerLevel() >= powerLevels_.redact;
}

bool
MatrixRoomPermissions::canChange(int eventType)
{
    return loaded_ && currentUserPowerLevel() >= requiredEventLevel(eventType);
}

bool
MatrixRoomPermissions::canSend(int eventType)
{
    if (!loaded_)
        return true;

    return currentUserPowerLevel() >= requiredEventLevel(eventType);
}

int
MatrixRoomPermissions::defaultLevel()
{
    return static_cast<int>(powerLevels_.usersDefault);
}

int
MatrixRoomPermissions::redactLevel()
{
    return static_cast<int>(powerLevels_.redact);
}

int
MatrixRoomPermissions::changeLevel(int eventType)
{
    const auto level = requiredEventLevel(eventType);
    return level >= std::numeric_limits<int>::max() ? std::numeric_limits<int>::max()
                                                    : static_cast<int>(level);
}

int
MatrixRoomPermissions::sendLevel(int eventType)
{
    const auto level = requiredEventLevel(eventType);
    return level >= std::numeric_limits<int>::max() ? std::numeric_limits<int>::max()
                                                    : static_cast<int>(level);
}

#include "moc_Permissions.cpp"
