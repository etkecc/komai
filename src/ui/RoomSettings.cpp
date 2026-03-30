// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomSettings.h"

#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"
#include "utils/Utils.h"

namespace {
struct RoomSettingsLoadResult
{
    std::optional<komai::MatrixRoomSettings> settings;
    QString error;
};

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
    this->allowedRoomsModel = new RoomSettingsAllowedRoomsModel(this);
    retrieveRoomInfo();
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
    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        nhlog::ui()->warn("Failed to refresh room settings via matrix-sdk runtime for '{}': {}",
                          roomid_.toStdString(),
                          tr("Matrix backend runtime is not available.").toStdString());
        return;
    }

    const auto requestId = ++roomSettingsLoadRequestId_;
    if (!isLoading_) {
        isLoading_ = true;
        emit loadingChanged();
    }

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId = roomid_]() {
          RoomSettingsLoadResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.settings    = komai::MatrixBackendRuntimeService::fetchRoomSettings(
            context, handleId, roomId, &result.error);
          return result;
      },
      [requestId](RoomSettings *settings, const RoomSettingsLoadResult &result) {
          if (requestId != settings->roomSettingsLoadRequestId_)
              return;

          settings->isLoading_ = false;
          emit settings->loadingChanged();

          if (!result.settings.has_value()) {
              if (!result.error.isEmpty()) {
                  nhlog::ui()->warn("Failed to refresh room settings via matrix-sdk runtime for "
                                    "'{}': {}",
                                    settings->roomid_.toStdString(),
                                    result.error.toStdString());
              }
              return;
          }

          settings->applyMatrixRoomSettings(*result.settings);
      });
}

void
RoomSettings::applyMatrixRoomSettings(const komai::MatrixRoomSettings &settings)
{
    const auto previousName                       = info_.name;
    const auto previousTopic                      = info_.topic;
    const auto previousAvatarUrl                  = info_.avatar_url;
    const auto previousVersion                    = info_.version;
    const auto previousMemberCount                = info_.member_count;
    const auto previousNotifications              = notifications_;
    const auto previousUsesEncryption             = usesEncryption_;
    const auto previousGuestAccess                = guestAccess_;
    const auto previousHistoryVisibilityKey       = historyVisibilityKey_;
    const auto previousJoinRule                   = joinRule_;
    const auto previousAllowedRoomIds             = allowedRoomIds_;
    const auto previousCanChangeAvatar            = canChangeAvatar();
    const auto previousCanChangeJoinRules         = canChangeJoinRules();
    const auto previousCanChangeName              = canChangeName();
    const auto previousCanChangeTopic             = canChangeTopic();
    const auto previousCanChangeHistoryVisibility = canChangeHistoryVisibility();
    const auto previousSupportsKnocking           = supportsKnocking();
    const auto previousSupportsRestricted         = supportsRestricted();
    const auto previousSupportsKnockRestricted    = supportsKnockRestricted();

    matrixRoomSettings_ = settings;
    info_.name          = settings.roomName.toStdString();
    info_.topic         = settings.roomTopic.toStdString();
    info_.avatar_url    = settings.roomAvatarUrl.toStdString();
    info_.version       = settings.roomVersion.toStdString();
    info_.member_count  = static_cast<size_t>(settings.memberCount);

    notifications_        = settings.notifications;
    usesEncryption_       = settings.isEncrypted;
    guestAccess_          = settings.guestAccess;
    historyVisibilityKey_ = normalizedHistoryVisibilityKey(settings.historyVisibility);
    joinRule_             = settings.joinRule.trimmed();
    allowedRoomIds_       = settings.allowedRoomIds;
    parentSpaceRoomIds_   = settings.parentSpaceRoomIds;

    if (allowedRoomsModel)
        allowedRoomsModel->refreshFromSettings();

    if (previousName != info_.name)
        emit roomNameChanged();
    if (previousTopic != info_.topic)
        emit roomTopicChanged();
    if (previousAvatarUrl != info_.avatar_url)
        emit avatarUrlChanged();
    if (previousVersion != info_.version || previousMemberCount != info_.member_count)
        emit roomDetailsChanged();
    if (previousNotifications != notifications_)
        emit notificationsChanged();
    if (previousUsesEncryption != usesEncryption_)
        emit encryptionChanged();
    if (previousHistoryVisibilityKey != historyVisibilityKey_)
        emit historyVisibilityChanged();
    if (previousGuestAccess != guestAccess_ || previousJoinRule != joinRule_ ||
        previousAllowedRoomIds != allowedRoomIds_) {
        emit accessJoinRulesChanged();
    }
    if (previousCanChangeAvatar != canChangeAvatar() ||
        previousCanChangeJoinRules != canChangeJoinRules() ||
        previousCanChangeName != canChangeName() || previousCanChangeTopic != canChangeTopic() ||
        previousCanChangeHistoryVisibility != canChangeHistoryVisibility() ||
        previousSupportsKnocking != supportsKnocking() ||
        previousSupportsRestricted != supportsRestricted() ||
        previousSupportsKnockRestricted != supportsKnockRestricted()) {
        emit permissionsChanged();
    }
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
    if (usesEncryption_ || isLoading_)
        return;

    const auto handleId = matrixBackendHandleId();

    isLoading_ = true;
    emit loadingChanged();

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId = roomid_]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::enableRoomEncryption(
            context, handleId, roomId, &error);
          return std::make_pair(ok, error);
      },
      [](RoomSettings *settings, const std::pair<bool, QString> &result) {
          settings->isLoading_ = false;
          emit settings->loadingChanged();

          const auto &[ok, error] = result;
          if (!ok) {
              emit settings->displayError(error.isEmpty() ? tr("Failed to enable encryption.")
                                                          : error);
              settings->usesEncryption_ = false;
              emit settings->encryptionChanged();
              return;
          }

          settings->usesEncryption_ = true;
          if (settings->matrixRoomSettings_)
              settings->matrixRoomSettings_->isEncrypted = true;
          emit settings->encryptionChanged();
      });
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
    const auto previousNotifications = notifications_;
    const auto requestId             = ++notificationsRequestId_;
    notifications_                   = currentIndex;
    emit notificationsChanged();

    const auto handleId = matrixBackendHandleId();
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId = roomid_, currentIndex]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::setRoomNotificationMode(
            context, handleId, roomId, currentIndex, &error);
          return std::make_pair(ok, error);
      },
      [requestId, previousNotifications, currentIndex](RoomSettings *settings,
                                                       const std::pair<bool, QString> &result) {
          if (requestId != settings->notificationsRequestId_)
              return;

          const auto &[ok, error] = result;
          if (!ok) {
              settings->notifications_ = previousNotifications;
              emit settings->notificationsChanged();
              emit settings->displayError(error.isEmpty() ? tr("Failed to update notifications.")
                                                          : error);
              return;
          }

          settings->notifications_ = currentIndex;
          if (settings->matrixRoomSettings_)
              settings->matrixRoomSettings_->notifications = currentIndex;
          emit settings->notificationsChanged();
      });
}

void
RoomSettings::changeName(const QString &name)
{
    // Check if the values are changed from the originals.
    auto newName = name.trimmed().toStdString();

    if (newName == info_.name) {
        return;
    }

    const auto newNameValue = QString::fromStdString(newName);
    const auto requestId    = ++roomNameRequestId_;
    const auto handleId     = matrixBackendHandleId();
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId = roomid_, newNameValue]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::setRoomName(
            context, handleId, roomId, newNameValue, &error);
          return std::make_pair(ok, error);
      },
      [requestId, newName, newNameValue](RoomSettings *settings,
                                         const std::pair<bool, QString> &result) {
          if (requestId != settings->roomNameRequestId_)
              return;

          const auto &[ok, error] = result;
          if (!ok) {
              emit settings->displayError(error.isEmpty() ? tr("Failed to update room name.")
                                                          : error);
              return;
          }

          settings->info_.name = newName;
          if (settings->matrixRoomSettings_)
              settings->matrixRoomSettings_->roomName = newNameValue;
          emit settings->roomNameChanged();
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

    const auto newTopicValue = QString::fromStdString(newTopic);
    const auto requestId     = ++roomTopicRequestId_;
    const auto handleId      = matrixBackendHandleId();
    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId = roomid_, newTopicValue]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::setRoomTopic(
            context, handleId, roomId, newTopicValue, &error);
          return std::make_pair(ok, error);
      },
      [requestId, newTopic, newTopicValue](RoomSettings *settings,
                                           const std::pair<bool, QString> &result) {
          if (requestId != settings->roomTopicRequestId_)
              return;

          const auto &[ok, error] = result;
          if (!ok) {
              emit settings->displayError(error.isEmpty() ? tr("Failed to update room topic.")
                                                          : error);
              return;
          }

          settings->info_.topic = newTopic;
          if (settings->matrixRoomSettings_)
              settings->matrixRoomSettings_->roomTopic = newTopicValue;
          emit settings->roomTopicChanged();
      });
}

QStringList
RoomSettings::parentSpaceRoomIds() const
{
    return QStringList(parentSpaceRoomIds_.begin(), parentSpaceRoomIds_.end());
}

#include "moc_RoomSettings.cpp"
