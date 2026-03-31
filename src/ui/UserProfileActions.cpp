// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFileDialog>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QPointer>
#include <QStandardPaths>

#include <utility>

#include "UserProfile.h"
#include "chat/ChatPage.h"
#include "encryption/VerificationManager.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "timeline/TimelineViewManager.h"
#include "utils/QtWorkerTask.h"

namespace {
void
notifyVerificationStateRefresh(const QString &userId)
{
    if (auto *verificationManager = VerificationManager::instance()) {
        emit verificationManager->verificationStateChanged(userId);
    }
}

template<typename WorkFnT, typename UiFnT>
void
runUserProfileRuntimeTask(UserProfile *profile, WorkFnT work, UiFnT ui)
{
    komai::qt_worker_task::runQueued(profile, std::move(work), std::move(ui));
}
}

void
UserProfile::banUser(const QString &reason)
{
    ChatPage::instance()->banUser(roomid_, this->userid_, reason);
}

void
UserProfile::kickUser(const QString &reason)
{
    ChatPage::instance()->kickUser(roomid_, this->userid_, reason);
}

void
UserProfile::startChat(bool encryption)
{
    ChatPage::instance()->startChat(this->userid_, encryption);
}

void
UserProfile::startChat()
{
    ChatPage::instance()->startChat(this->userid_, std::nullopt);
}

void
UserProfile::changeUsername(const QString &username)
{
    if (!isSelf()) {
        emit displayError(tr("Only your own profile can be changed here."));
        return;
    }

    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        emit displayError(tr("Matrix backend runtime is not available."));
        return;
    }

    const auto nextName      = isGlobalUserProfile() ? username : username.trimmed();
    const auto roomId        = roomid_;
    const auto globalProfile = isGlobalUserProfile();

    runUserProfileRuntimeTask(
      this,
      [handleId, roomId, nextName, globalProfile]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = globalProfile ? komai::MatrixBackendRuntimeService::setOwnDisplayName(
                                            context, handleId, nextName, &error)
                                        : komai::MatrixBackendRuntimeService::setOwnRoomDisplayName(
                                            context, handleId, roomId, nextName, &error);
          return std::make_pair(ok, error);
      },
      [globalProfile](UserProfile *profile, const std::pair<bool, QString> &result) {
          const auto &[ok, error] = result;
          if (!ok) {
              emit profile->displayError(
                error.isEmpty() ? (globalProfile ? tr("Failed to update display name.")
                                                 : tr("Failed to update room display name."))
                                : error);
              return;
          }

          profile->getGlobalProfileData();
      });
}

void
UserProfile::changeDeviceName(const QString &deviceID, const QString &deviceName)
{
    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        emit displayError(tr("Matrix backend runtime is not available."));
        return;
    }

    const auto trimmedDeviceId   = deviceID.trimmed();
    const auto trimmedDeviceName = deviceName.trimmed();
    if (trimmedDeviceId.isEmpty()) {
        emit displayError(tr("Device id cannot be empty."));
        return;
    }
    if (trimmedDeviceName.isEmpty()) {
        emit displayError(tr("Device name cannot be empty."));
        return;
    }

    runUserProfileRuntimeTask(
      this,
      [handleId, trimmedDeviceId, trimmedDeviceName]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::renameDevice(
            context, handleId, trimmedDeviceId, trimmedDeviceName, &error);
          return std::make_pair(ok, error);
      },
      [trimmedDeviceId](UserProfile *profile, const std::pair<bool, QString> &result) {
          const auto &[ok, error] = result;
          if (!ok) {
              emit profile->displayError(
                error.isEmpty()
                  ? tr("Failed to rename device \"%1\".").arg(trimmedDeviceId)
                  : tr("Failed to rename device \"%1\": %2").arg(trimmedDeviceId, error));
              return;
          }

          profile->refreshDevices();
      });
}

void
UserProfile::verify(QString device)
{
    auto *verificationManager = VerificationManager::instance();
    if (!verificationManager) {
        emit displayError(tr("The verification manager is not available."));
        return;
    }

    const QPointer<UserProfile> guard(this);
    const auto onFailure = [guard](const QString &error) {
        if (!guard)
            return;

        emit guard->displayError(error.isEmpty() ? guard->tr("Failed to start device verification.")
                                                 : error);
    };

    if (device.trimmed().isEmpty()) {
        verificationManager->verifyUser(userid_, onFailure);
    } else {
        verificationManager->verifyDevice(userid_, device, onFailure);
    }
}

void
UserProfile::unverify(const QString &device)
{
    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        emit displayError(tr("Matrix backend runtime is not available."));
        return;
    }

    const auto trimmedDeviceId = device.trimmed();
    if (trimmedDeviceId.isEmpty()) {
        emit displayError(tr("Device id cannot be empty."));
        return;
    }

    const auto userId = userid_;
    runUserProfileRuntimeTask(
      this,
      [handleId, userId, trimmedDeviceId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::unverifyDevice(
            context, handleId, userId, trimmedDeviceId, &error);
          return std::make_pair(ok, error);
      },
      [trimmedDeviceId](UserProfile *profile, const std::pair<bool, QString> &result) {
          const auto &[ok, error] = result;
          if (!ok) {
              emit profile->displayError(
                error.isEmpty()
                  ? tr("Failed to clear verification for device \"%1\".").arg(trimmedDeviceId)
                  : tr("Failed to clear verification for device \"%1\": %2")
                      .arg(trimmedDeviceId, error));
              return;
          }

          profile->refreshDevices();
          notifyVerificationStateRefresh(profile->userid_);
      });
}

void
UserProfile::blockDevice(const QString &device)
{
    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        emit displayError(tr("Matrix backend runtime is not available."));
        return;
    }

    const auto trimmedDeviceId = device.trimmed();
    if (trimmedDeviceId.isEmpty()) {
        emit displayError(tr("Device id cannot be empty."));
        return;
    }

    const auto userId = userid_;
    runUserProfileRuntimeTask(
      this,
      [handleId, userId, trimmedDeviceId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::blockDevice(
            context, handleId, userId, trimmedDeviceId, &error);
          return std::make_pair(ok, error);
      },
      [trimmedDeviceId](UserProfile *profile, const std::pair<bool, QString> &result) {
          const auto &[ok, error] = result;
          if (!ok) {
              emit profile->displayError(
                error.isEmpty()
                  ? tr("Failed to block device \"%1\".").arg(trimmedDeviceId)
                  : tr("Failed to block device \"%1\": %2").arg(trimmedDeviceId, error));
              return;
          }

          profile->refreshDevices();
          notifyVerificationStateRefresh(profile->userid_);
      });
}

void
UserProfile::unblockDevice(const QString &device)
{
    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        emit displayError(tr("Matrix backend runtime is not available."));
        return;
    }

    const auto trimmedDeviceId = device.trimmed();
    if (trimmedDeviceId.isEmpty()) {
        emit displayError(tr("Device id cannot be empty."));
        return;
    }

    const auto userId = userid_;
    runUserProfileRuntimeTask(
      this,
      [handleId, userId, trimmedDeviceId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::unblockDevice(
            context, handleId, userId, trimmedDeviceId, &error);
          return std::make_pair(ok, error);
      },
      [trimmedDeviceId](UserProfile *profile, const std::pair<bool, QString> &result) {
          const auto &[ok, error] = result;
          if (!ok) {
              emit profile->displayError(
                error.isEmpty()
                  ? tr("Failed to unblock device \"%1\".").arg(trimmedDeviceId)
                  : tr("Failed to unblock device \"%1\": %2").arg(trimmedDeviceId, error));
              return;
          }

          profile->refreshDevices();
          notifyVerificationStateRefresh(profile->userid_);
      });
}

void
UserProfile::changeAvatar()
{
    const QString picturesFolder =
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString fileName = QFileDialog::getOpenFileName(
      nullptr, tr("Select an avatar"), picturesFolder, tr("All Files (*)"));

    if (fileName.isEmpty())
        return;

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(fileName, QMimeDatabase::MatchContent);

    const auto format = mime.name().split(QStringLiteral("/"))[0];

    QFile file{fileName, this};
    if (format != QLatin1String("image")) {
        emit displayError(tr("The selected file is not an image"));
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        emit displayError(tr("Error while reading file: %1").arg(file.errorString()));
        return;
    }

    isLoading_ = true;
    emit loadingChanged();

    if (!isSelf()) {
        isLoading_ = false;
        emit loadingChanged();
        emit displayError(tr("Only your own avatar can be changed here."));
        return;
    }

    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        isLoading_ = false;
        emit loadingChanged();
        emit displayError(tr("Matrix backend runtime is not available."));
        return;
    }

    const auto roomId        = roomid_;
    const auto globalProfile = isGlobalUserProfile();

    runUserProfileRuntimeTask(
      this,
      [handleId, roomId, fileName, mimeName = mime.name(), globalProfile]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = globalProfile ? komai::MatrixBackendRuntimeService::uploadOwnAvatar(
                                            context, handleId, fileName, mimeName, &error)
                                        : komai::MatrixBackendRuntimeService::uploadOwnRoomAvatar(
                                            context, handleId, roomId, fileName, mimeName, &error);
          return std::make_pair(ok, error);
      },
      [globalProfile](UserProfile *profile, const std::pair<bool, QString> &result) {
          const auto &[ok, error] = result;
          profile->isLoading_     = false;
          emit profile->loadingChanged();

          if (!ok) {
              emit profile->displayError(error.isEmpty()
                                           ? (globalProfile ? tr("Failed to upload avatar.")
                                                            : tr("Failed to upload room avatar."))
                                           : error);
              return;
          }

          profile->getGlobalProfileData();
      });
}

void
UserProfile::removeAvatar()
{
    if (!isSelf()) {
        emit displayError(tr("Only your own avatar can be changed here."));
        return;
    }

    isLoading_ = true;
    emit loadingChanged();

    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        isLoading_ = false;
        emit loadingChanged();
        emit displayError(tr("Matrix backend runtime is not available."));
        return;
    }

    const auto roomId        = roomid_;
    const auto globalProfile = isGlobalUserProfile();

    runUserProfileRuntimeTask(
      this,
      [handleId, roomId, globalProfile]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok =
            globalProfile
              ? komai::MatrixBackendRuntimeService::removeOwnAvatar(context, handleId, &error)
              : komai::MatrixBackendRuntimeService::removeOwnRoomAvatar(
                  context, handleId, roomId, &error);
          return std::make_pair(ok, error);
      },
      [globalProfile](UserProfile *profile, const std::pair<bool, QString> &result) {
          const auto &[ok, error] = result;
          profile->isLoading_     = false;
          emit profile->loadingChanged();

          if (!ok) {
              emit profile->displayError(error.isEmpty()
                                           ? (globalProfile ? tr("Failed to remove avatar.")
                                                            : tr("Failed to remove room avatar."))
                                           : error);
              return;
          }

          profile->getGlobalProfileData();
      });
}

void
UserProfile::openGlobalProfile()
{
    emit manager->openGlobalUserProfile(userid_);
}
