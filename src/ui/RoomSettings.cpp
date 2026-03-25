// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomSettings.h"

#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

namespace {
QString
normalizedHistoryVisibilityKey(const QString &historyVisibility)
{
    if (historyVisibility == QLatin1String("world_readable"))
        return QStringLiteral("world_readable");
    if (historyVisibility == QLatin1String("invited"))
        return QStringLiteral("invited");
    if (historyVisibility == QLatin1String("joined"))
        return QStringLiteral("joined");
    return QStringLiteral("shared");
}
} // namespace

RoomSettings::RoomSettings(QString roomid, QObject *parent)
  : QObject(parent)
  , roomid_{std::move(roomid)}
{
    connect(this, &RoomSettings::accessJoinRulesChanged, &RoomSettings::allowedRoomsChanged);

    QString error;
    if (!loadMatrixRuntimeRoomSettings(&error) && !error.isEmpty()) {
        nhlog::ui()->warn("Failed to load room settings via matrix-sdk runtime for '{}': {}",
                          roomid_.toStdString(),
                          error.toStdString());
    }

    this->allowedRoomsModel = new RoomSettingsAllowedRoomsModel(this);
}

bool
RoomSettings::isRoomNameSet() const
{
    return !info_.name.empty();
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
    QString error;
    if (!loadMatrixRuntimeRoomSettings(&error) && !error.isEmpty()) {
        nhlog::ui()->warn("Failed to refresh room settings via matrix-sdk runtime for '{}': {}",
                          roomid_.toStdString(),
                          error.toStdString());
    }
}

bool
RoomSettings::loadMatrixRuntimeRoomSettings(QString *errorOut)
{
    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        if (errorOut)
            *errorOut = tr("Matrix backend runtime is not available.");
        return false;
    }

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

    notifications_        = result->notifications;
    usesEncryption_       = result->isEncrypted;
    guestAccess_          = result->guestAccess;
    historyVisibilityKey_ = normalizedHistoryVisibilityKey(result->historyVisibility);
    joinRule_             = result->joinRule.trimmed();
    allowedRoomIds_       = result->allowedRoomIds;
    parentSpaceRoomIds_   = result->parentSpaceRoomIds;

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
}

bool
RoomSettings::canChangeName() const
{
    return matrixRoomSettings_ && matrixRoomSettings_->canChangeName;
}

bool
RoomSettings::canChangeTopic() const
{
    return matrixRoomSettings_ && matrixRoomSettings_->canChangeTopic;
}

bool
RoomSettings::canChangeAvatar() const
{
    return matrixRoomSettings_ && matrixRoomSettings_->canChangeAvatar;
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

    QString error;
    if (!komai::MatrixBackendRuntimeService::setRoomNotificationMode(
          matrixBackendHandleId(), roomid_, currentIndex, &error)) {
        emit displayError(error.isEmpty() ? tr("Failed to update notifications.") : error);
        return;
    }

    if (matrixRoomSettings_)
        matrixRoomSettings_->notifications = currentIndex;
    emit notificationsChanged();
}

void
RoomSettings::changeName(const QString &name)
{
    // Check if the values are changed from the originals.
    auto newName = name.trimmed().toStdString();

    if (newName == info_.name) {
        return;
    }

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
}

void
RoomSettings::changeTopic(const QString &topic)
{
    // Check if the values are changed from the originals.
    auto newTopic = topic.trimmed().toStdString();

    if (newTopic == info_.topic) {
        return;
    }

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
}

QStringList
RoomSettings::parentSpaceRoomIds() const
{
    return QStringList(parentSpaceRoomIds_.begin(), parentSpaceRoomIds_.end());
}

#include "moc_RoomSettings.cpp"
