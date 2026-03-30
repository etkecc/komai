// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDesktopServices>
#include <QInputDialog>
#include <QLineEdit>
#include <QMetaObject>
#include <QUrl>

#include <thread>

#include "UserProfile.h"
#include "chat/ChatPage.h"
#include "encryption/VerificationManager.h"
#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"
#include "utils/Utils.h"

namespace {
crypto::Trust
userTrustFromRuntime(const QString &trust, bool isSelf)
{
    if (trust == QLatin1String("Verified"))
        return crypto::Trust::Verified;
    if (!isSelf && trust == QLatin1String("TOFU"))
        return crypto::Trust::TOFU;
    return crypto::Trust::Unverified;
}

verification::Status
deviceVerificationStatusFromRuntime(const QString &state)
{
    if (state == QLatin1String("self"))
        return verification::SELF;
    if (state == QLatin1String("verified"))
        return verification::VERIFIED;
    if (state == QLatin1String("blocked"))
        return verification::BLOCKED;
    return verification::UNVERIFIED;
}

void
notifyVerificationStateRefresh(const QString &userId)
{
    if (auto *verificationManager = VerificationManager::instance()) {
        emit verificationManager->verificationStateChanged(userId);
    }
}
}

UserProfile::UserProfile(const QString &roomid,
                         const QString &userid,
                         TimelineViewManager *manager_,
                         QString roomName,
                         QString roomAvatarUrl,
                         QObject *parent)
  : QObject(parent)
  , roomid_(roomid)
  , userid_(userid)
  , roomName_(std::move(roomName))
  , roomAvatarUrl_(komai::matrix::normalizeMxcUri(std::move(roomAvatarUrl)))
  , globalAvatarUrl{QLatin1String("")}
  , manager(manager_)
  , model(parent)
{
    connect(this,
            &UserProfile::globalUsernameRetrieved,
            this,
            &UserProfile::setGlobalUsername,
            Qt::QueuedConnection);
    connect(this,
            &UserProfile::verificationStatiChanged,
            this,
            &UserProfile::updateVerificationStatus,
            Qt::QueuedConnection);

    if (isGlobalUserProfile() && isSelf() && ChatPage::instance()) {
        connect(ChatPage::instance(),
                &ChatPage::setUserDisplayName,
                this,
                [this](const QString &name) { emit globalUsernameRetrieved(name); });
        connect(
          ChatPage::instance(), &ChatPage::setUserAvatar, this, [this](const QString &avatar) {
              globalAvatarUrl = komai::matrix::normalizeMxcUri(avatar);
              emit avatarUrlChanged();
              emit globalAvatarUrlChanged();
          });
    }

    if (auto *verificationManager = VerificationManager::instance()) {
        connect(verificationManager,
                &VerificationManager::verificationStateChanged,
                this,
                [this](const QString &userId) {
                    if (userId.trimmed() != userid_.trimmed())
                        return;

                    refreshDevices();
                });
    }

    if (auto *app = qGuiApp) {
        connect(
          app, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
              if (state != Qt::ApplicationActive || !refreshDevicesOnNextActivation_)
                  return;

              refreshDevicesOnNextActivation_ = false;
              refreshDevices();
          });
    }

    getGlobalProfileData();

    if (ChatPage::instance() && ChatPage::instance()->timelineManager()) {
        connect(ChatPage::instance()->timelineManager(),
                &TimelineViewManager::ignoredUsersChanged,
                this,
                [this](const QVector<QString> &) {
                    ignoredOverride_.reset();
                    emit ignoredChanged();
                });
    }

    if (matrixBackendHandleId() != 0 || !ChatPage::instance() ||
        !ChatPage::instance()->timelineManager()) {
        sharedRooms_ = new RoomInfoModel({}, this);
        updateVerificationStatus();
        return;
    }

    sharedRooms_ = new RoomInfoModel({}, this);
    updateVerificationStatus();
}

DeviceInfoModel *
UserProfile::deviceList()
{
    return &this->deviceList_;
}

RoomInfoModel *
UserProfile::sharedRooms()
{
    return this->sharedRooms_;
}

QString
UserProfile::userid()
{
    return this->userid_;
}

QString
UserProfile::displayName()
{
    if (!isGlobalUserProfile() && !roomDisplayName_.trimmed().isEmpty())
        return roomDisplayName_;

    if (!globalUsername.trimmed().isEmpty())
        return globalUsername;

    return userid_;
}

QString
UserProfile::avatarUrl()
{
    if (!isGlobalUserProfile() && !roomAvatarOverrideUrl_.trimmed().isEmpty())
        return roomAvatarOverrideUrl_;

    return globalAvatarUrl;
}

bool
UserProfile::isGlobalUserProfile() const
{
    return roomid_ == QLatin1String("");
}

crypto::Trust
UserProfile::getUserStatus()
{
    return isUserVerified;
}

bool
UserProfile::userVerificationEnabled() const
{
    return hasMasterKey;
}

bool
UserProfile::canStartVerification() const
{
    if (hasMasterKey)
        return true;

    for (const auto &device : deviceList_.deviceList_) {
        if (device.verification_status == verification::UNVERIFIED)
            return true;
    }

    return false;
}

bool
UserProfile::isSelf() const
{
    return this->userid_ == utils::localUser();
}

uint64_t
UserProfile::matrixBackendHandleId() const
{
    const auto *mainWindow = MainWindow::instance();
    return mainWindow ? mainWindow->matrixBackendHandleId() : 0;
}

void
UserProfile::signOutDevice(const QString &deviceID)
{
    const auto trimmedDeviceId = deviceID.trimmed();
    if (trimmedDeviceId.isEmpty()) {
        MainWindow::instance()->showNotification(tr("Device id cannot be empty."));
        return;
    }

    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        MainWindow::instance()->showNotification(
          tr("Device sign-out requires an active matrix-sdk backend runtime."));
        return;
    }

    const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
    QString error;
    const auto result = komai::MatrixBackendRuntimeService::startSignOutDevice(
      context, handleId, trimmedDeviceId, &error);
    if (!result) {
        MainWindow::instance()->showNotification(
          error.isEmpty() ? tr("Failed to sign out device \"%1\".").arg(trimmedDeviceId)
                          : tr("Failed to sign out device \"%1\": %2").arg(trimmedDeviceId, error));
        return;
    }

    if (result->completed) {
        MainWindow::instance()->showNotification(
          tr("Signed out device \"%1\".").arg(trimmedDeviceId));
        refreshDevices();
        notifyVerificationStateRefresh(userid_);
        return;
    }

    if (result->authType == QLatin1String("password")) {
        bool ok             = false;
        const auto password = QInputDialog::getText(nullptr,
                                                    tr("Sign Out Device"),
                                                    tr("Enter your account password to sign out "
                                                       "device \"%1\".")
                                                      .arg(trimmedDeviceId),
                                                    QLineEdit::Password,
                                                    QString(),
                                                    &ok);
        if (!ok)
            return;

        if (password.isEmpty()) {
            MainWindow::instance()->showNotification(
              tr("Password is required to sign out device \"%1\".").arg(trimmedDeviceId));
            return;
        }

        if (!komai::MatrixBackendRuntimeService::continueSignOutDeviceWithPassword(
              context, handleId, password, &error)) {
            MainWindow::instance()->showNotification(
              error.isEmpty()
                ? tr("Failed to sign out device \"%1\".").arg(trimmedDeviceId)
                : tr("Failed to sign out device \"%1\": %2").arg(trimmedDeviceId, error));
            return;
        }

        MainWindow::instance()->showNotification(
          tr("Signed out device \"%1\".").arg(trimmedDeviceId));
        refreshDevices();
        notifyVerificationStateRefresh(userid_);
        return;
    }

    if (result->authType == QLatin1String("oauth")) {
        const auto url = QUrl::fromUserInput(result->approvalUrl);
        if (!url.isValid() || !QDesktopServices::openUrl(url)) {
            MainWindow::instance()->showNotification(
              tr("Failed to open the browser for device sign-out."));
            return;
        }

        refreshDevicesOnNextActivation_ = true;
        MainWindow::instance()->showNotification(
          tr("Finish signing out device \"%1\" in your browser. The device list will refresh when "
             "you return.")
            .arg(trimmedDeviceId));
        return;
    }

    MainWindow::instance()->showNotification(
      tr("Device sign-out for \"%1\" requires an unsupported authentication flow.")
        .arg(trimmedDeviceId));
}

void
UserProfile::refreshDevices()
{
    updateVerificationStatus();
}

bool
UserProfile::ignored() const
{
    if (ignoredOverride_)
        return *ignoredOverride_;

    auto old = TimelineViewManager::instance()->getIgnoredUsers();
    return old.contains(userid_);
}

void
UserProfile::setIgnored(bool ignore)
{
    const auto handleId = matrixBackendHandleId();
    if (handleId != 0) {
        const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
        QString error;
        const bool ok =
          ignore
            ? komai::MatrixBackendRuntimeService::ignoreUser(context, handleId, userid_, &error)
            : komai::MatrixBackendRuntimeService::unignoreUser(context, handleId, userid_, &error);
        if (!ok) {
            MainWindow::instance()->showNotification(
              error.isEmpty()
                ? tr("Failed to update ignored-user state for \"%1\".").arg(userid_)
                : tr("Failed to update ignored-user state for \"%1\": %2").arg(userid_, error));
            emit ignoredChanged();
            return;
        }

        ignoredOverride_ = ignore;
        emit ignoredChanged();
        return;
    }

    MainWindow::instance()->showNotification(
      tr("Ignoring users requires an active matrix-sdk backend runtime."));
    emit ignoredChanged();
}

void
UserProfile::fetchDeviceList(const QString &userID)
{
    Q_UNUSED(userID);
    updateVerificationStatus();
}

void
UserProfile::updateVerificationStatus()
{
    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        this->hasMasterKey   = false;
        this->isUserVerified = crypto::Trust::Unverified;
        this->deviceList_.reset({});
        emit userStatusChanged();
        emit devicesChanged();
        return;
    }

    QPointer<UserProfile> guard(this);
    const auto userId     = userid_;
    const bool ownProfile = isSelf();

    std::thread([guard, handleId, userId, ownProfile]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        const auto result = komai::MatrixBackendRuntimeService::fetchUserVerificationState(
          context, handleId, userId, &error);

        if (!guard)
            return;

        if (!result) {
            nhlog::crypto()->warn("Failed to fetch matrix-sdk verification status for '{}': {}",
                                  userId.toStdString(),
                                  error.toStdString());
        }

        std::vector<DeviceInfo> devices;
        devices.reserve(result ? static_cast<size_t>(result->devices.size()) : 0);
        if (result) {
            for (const auto &device : result->devices) {
                devices.emplace_back(device.deviceId,
                                     device.displayName,
                                     deviceVerificationStatusFromRuntime(device.verificationState),
                                     device.lastIp,
                                     device.lastTs);
            }
        }

        const auto nextTrust =
          result ? userTrustFromRuntime(result->userTrust, ownProfile) : crypto::Trust::Unverified;
        const bool nextHasMasterKey = result && result->hasMasterKey;

        QMetaObject::invokeMethod(
          guard,
          [guard, devices = std::move(devices), nextTrust, nextHasMasterKey]() mutable {
              if (!guard)
                  return;

              guard->hasMasterKey   = nextHasMasterKey;
              guard->isUserVerified = nextTrust;
              guard->deviceList_.reset(devices);
              emit guard->userStatusChanged();
              emit guard->devicesChanged();
          },
          Qt::QueuedConnection);
    }).detach();
}

void
UserProfile::setGlobalUsername(const QString &globalUser)
{
    globalUsername = globalUser;
    emit displayNameChanged();
    emit globalDisplayNameChanged();
}

void
UserProfile::updateAvatarUrl()
{
    isLoading_ = false;
    emit loadingChanged();

    emit avatarUrlChanged();
}

bool
UserProfile::isLoading() const
{
    return isLoading_;
}

void
UserProfile::getGlobalProfileData()
{
    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        nhlog::net()->warn("failed to retrieve user profile info: matrix-sdk runtime is not "
                           "available");
        emit failedToFetchProfile();
        return;
    }

    QPointer<UserProfile> guard(this);
    const auto userId      = userid_;
    const bool ownProfile  = isSelf();
    const auto roomId      = roomid_;
    const bool roomProfile = !isGlobalUserProfile();

    std::thread([guard, handleId, userId, ownProfile, roomId, roomProfile]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString globalError;
        const auto ownResult =
          ownProfile
            ? komai::MatrixBackendRuntimeService::fetchOwnProfile(context, handleId, &globalError)
            : std::optional<komai::MatrixOwnProfile>{};
        const auto userResult = ownProfile ? std::optional<komai::MatrixUserProfile>{}
                                           : komai::MatrixBackendRuntimeService::fetchUserProfile(
                                               context, handleId, userId, &globalError);
        QString roomError;
        const auto roomResult = roomProfile
                                  ? komai::MatrixBackendRuntimeService::fetchRoomMemberProfile(
                                      context, handleId, roomId, userId, &roomError)
                                  : std::optional<komai::MatrixUserProfile>{};

        if (!guard)
            return;

        const bool globalOk       = ownProfile ? ownResult.has_value() : userResult.has_value();
        const QString displayName = ownProfile ? (ownResult ? ownResult->displayName : QString{})
                                               : (userResult ? userResult->displayName : QString{});
        const QString avatarUrl   = ownProfile ? (ownResult ? ownResult->avatarUrl : QString{})
                                               : (userResult ? userResult->avatarUrl : QString{});
        const QString roomDisplayName = roomResult ? roomResult->displayName : QString{};
        const QString roomAvatarUrl   = roomResult ? roomResult->avatarUrl : QString{};

        emit guard->globalUsernameRetrieved(displayName);
        QMetaObject::invokeMethod(
          guard,
          [guard,
           globalOk,
           avatarUrl,
           globalError,
           roomProfile,
           roomDisplayName,
           roomAvatarUrl,
           roomError]() {
              if (!guard)
                  return;

              if (!globalOk && !roomProfile) {
                  nhlog::net()->warn("failed to retrieve user profile info via matrix-sdk "
                                     "runtime: {}",
                                     globalError.toStdString());
                  emit guard->failedToFetchProfile();
                  return;
              }

              guard->globalAvatarUrl = komai::matrix::normalizeMxcUri(avatarUrl);
              if (roomProfile) {
                  if (!roomError.isEmpty()) {
                      nhlog::net()->debug(
                        "failed to retrieve room-member profile info via matrix-sdk runtime: {}",
                        roomError.toStdString());
                  }
                  guard->roomDisplayName_       = roomDisplayName;
                  guard->roomAvatarOverrideUrl_ = komai::matrix::normalizeMxcUri(roomAvatarUrl);
              }

              emit guard->displayNameChanged();
              emit guard->avatarUrlChanged();
              emit guard->globalAvatarUrlChanged();
          },
          Qt::QueuedConnection);
    }).detach();
}

#include "moc_UserProfile.cpp"
