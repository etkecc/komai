// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFileDialog>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStandardPaths>

#include "UserProfile.h"
#include "chat/ChatPage.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "timeline/TimelineViewManager.h"

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
    if (!isGlobalUserProfile() || !isSelf()) {
        emit displayError(tr("Room-specific profile overrides are not migrated yet."));
        return;
    }

    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        emit displayError(tr("Matrix backend runtime is not available."));
        return;
    }

    QString error;
    if (!komai::MatrixBackendRuntimeService::setOwnDisplayName(handleId, username, &error)) {
        emit displayError(error.isEmpty() ? tr("Failed to update display name.") : error);
        return;
    }

    getGlobalProfileData();
}

void
UserProfile::changeDeviceName(const QString &deviceID, const QString &deviceName)
{
    Q_UNUSED(deviceID);
    Q_UNUSED(deviceName);
    emit displayError(tr("Device management is not migrated to the matrix-sdk backend yet."));
}

void
UserProfile::verify(QString device)
{
    Q_UNUSED(device);
    emit displayError(tr("Device verification is not migrated to the matrix-sdk backend yet."));
}

void
UserProfile::unverify(const QString &device)
{
    Q_UNUSED(device);
    emit displayError(tr("Device verification is not migrated to the matrix-sdk backend yet."));
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

    if (!isGlobalUserProfile() || !isSelf()) {
        isLoading_ = false;
        emit loadingChanged();
        emit displayError(tr("Room-specific profile overrides are not migrated yet."));
        return;
    }

    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        isLoading_ = false;
        emit loadingChanged();
        emit displayError(tr("Matrix backend runtime is not available."));
        return;
    }

    QString error;
    if (!komai::MatrixBackendRuntimeService::uploadOwnAvatar(
          handleId, fileName, mime.name(), &error)) {
        isLoading_ = false;
        emit loadingChanged();
        emit displayError(error.isEmpty() ? tr("Failed to upload avatar.") : error);
        return;
    }

    isLoading_ = false;
    emit loadingChanged();
    getGlobalProfileData();
}

void
UserProfile::removeAvatar()
{
    if (!isGlobalUserProfile() || !isSelf()) {
        emit displayError(tr("Room-specific profile overrides are not migrated yet."));
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

    QString error;
    if (!komai::MatrixBackendRuntimeService::removeOwnAvatar(handleId, &error)) {
        isLoading_ = false;
        emit loadingChanged();
        emit displayError(error.isEmpty() ? tr("Failed to remove avatar.") : error);
        return;
    }

    isLoading_ = false;
    emit loadingChanged();
    getGlobalProfileData();
}

void
UserProfile::openGlobalProfile()
{
    emit manager->openGlobalUserProfile(userid_);
}
