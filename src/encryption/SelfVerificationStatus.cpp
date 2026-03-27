// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SelfVerificationStatus.h"

#include <QTimer>

#include "VerificationManager.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"

namespace {
QString
notMigratedMessage()
{
    return SelfVerificationStatus::tr(
      "Encryption setup and self-verification are not migrated to the matrix-sdk backend yet.");
}

QString
missingRuntimeMessage()
{
    return SelfVerificationStatus::tr(
      "The Rust Matrix backend is not active, so encryption recovery is unavailable.");
}
}

SelfVerificationStatus::SelfVerificationStatus(QObject *o)
  : QObject(o)
{
    status_                     = AllVerified;
    hasSSSS_                    = false;
    canVerifyWithAnotherDevice_ = false;

    if (auto *chatPage = ChatPage::instance()) {
        connect(chatPage,
                &ChatPage::contentLoaded,
                this,
                &SelfVerificationStatus::refreshStateFromMatrixRuntime);
        connect(chatPage, &ChatPage::loggedOut, this, &SelfVerificationStatus::invalidate);
    }

    QTimer::singleShot(0, this, &SelfVerificationStatus::refreshStateFromMatrixRuntime);
}

void
SelfVerificationStatus::setupCrosssigning(bool useSSSS,
                                          const QString &password,
                                          bool encryptionBackupOnlineEnabled)
{
    Q_UNUSED(useSSSS);
    Q_UNUSED(password);
    Q_UNUSED(encryptionBackupOnlineEnabled);
    nhlog::crypto()->warn("Self-verification setup is not migrated to matrix-sdk yet");
    emit setupFailed(notMigratedMessage());
}

bool
SelfVerificationStatus::verifyMasterKey()
{
    refreshStateFromMatrixRuntime();

    if (!canVerifyWithAnotherDevice_) {
        emit setupFailed(tr("No other signed-in device is currently available for verification."));
        return false;
    }

    auto *verificationManager = VerificationManager::instance();
    if (!verificationManager) {
        emit setupFailed(tr("The verification manager is not available."));
        return false;
    }

    QString error;
    if (!verificationManager->verifySelf(&error)) {
        emit setupFailed(error.isEmpty() ? notMigratedMessage() : error);
        return false;
    }

    return true;
}

void
SelfVerificationStatus::verifyMasterKeyWithPassphrase()
{
    refreshStateFromMatrixRuntime();

    if (!hasSSSS_) {
        emit setupFailed(tr("This account does not currently expose an unlockable key backup."));
        return;
    }

    emit promptUnlockKeyBackup();
}

void
SelfVerificationStatus::submitUnlockKeyBackup(const QString &keyOrPassphrase)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        emit setupFailed(missingRuntimeMessage());
        return;
    }

    QString error;
    if (!komai::MatrixBackendRuntimeService::recoverEncryptionSecrets(
          handleId, keyOrPassphrase, &error)) {
        emit setupFailed(tr("Failed to unlock key backup: %1").arg(error));
        return;
    }

    nhlog::crypto()->info("Recovered encryption secrets through matrix-sdk recovery");
    refreshStateFromMatrixRuntime();
    emit unlockKeyBackupCompleted();
}

void
SelfVerificationStatus::cancelUnlockKeyBackup()
{
    nhlog::crypto()->debug("Cancelled matrix-sdk key-backup unlock prompt");
}

void
SelfVerificationStatus::verifyUnverifiedDevices()
{
    nhlog::crypto()->warn("Device verification is not migrated to matrix-sdk yet");
    emit setupFailed(notMigratedMessage());
}

void
SelfVerificationStatus::setupEncryptionBackup()
{
    nhlog::crypto()->warn("Encryption backup setup is not migrated to matrix-sdk yet");
    emit setupFailed(notMigratedMessage());
}

void
SelfVerificationStatus::resetEncryptionIdentity()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        emit setupFailed(missingRuntimeMessage());
        return;
    }

    QString error;
    const auto result =
      komai::MatrixBackendRuntimeService::startResetEncryptionIdentity(handleId, &error);
    if (!result) {
        emit setupFailed(tr("Failed to reset encryption identity: %1").arg(error));
        return;
    }

    if (result->completed) {
        nhlog::crypto()->info("Reset encryption identity through matrix-sdk without extra auth");
        refreshStateFromMatrixRuntime();
        emit resetEncryptionIdentityCompleted();
        return;
    }

    if (result->authType == QLatin1String("password")) {
        emit promptResetEncryptionIdentityPassword();
        return;
    }

    if (result->authType == QLatin1String("oauth")) {
        emit promptResetEncryptionIdentityApproval(result->approvalUrl);
        return;
    }

    emit setupFailed(tr("The encryption identity reset flow returned an unsupported "
                        "authentication type."));
}

void
SelfVerificationStatus::submitResetEncryptionIdentityPassword(const QString &password)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        emit setupFailed(missingRuntimeMessage());
        return;
    }

    QString error;
    if (!komai::MatrixBackendRuntimeService::continueResetEncryptionIdentityWithPassword(
          handleId, password, &error)) {
        emit setupFailed(tr("Failed to complete encryption identity reset: %1").arg(error));
        return;
    }

    nhlog::crypto()->info(
      "Completed matrix-sdk encryption identity reset using password authentication");
    refreshStateFromMatrixRuntime();
    emit resetEncryptionIdentityCompleted();
}

void
SelfVerificationStatus::continueResetEncryptionIdentityAfterApproval()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        emit setupFailed(missingRuntimeMessage());
        return;
    }

    QString error;
    if (!komai::MatrixBackendRuntimeService::continueResetEncryptionIdentityAfterApproval(handleId,
                                                                                          &error)) {
        emit setupFailed(tr("Failed to complete encryption identity reset: %1").arg(error));
        return;
    }

    nhlog::crypto()->info("Completed matrix-sdk encryption identity reset after browser approval");
    refreshStateFromMatrixRuntime();
    emit resetEncryptionIdentityCompleted();
}

void
SelfVerificationStatus::cancelResetEncryptionIdentity()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0)
        return;

    QString error;
    if (!komai::MatrixBackendRuntimeService::cancelResetEncryptionIdentity(handleId, &error)) {
        nhlog::crypto()->warn("Failed to cancel pending matrix-sdk encryption identity reset: {}",
                              error.toStdString());
    }
}

void
SelfVerificationStatus::promptCurrentVerificationAction()
{
    refreshStateFromMatrixRuntime();
    if (status_ != AllVerified)
        emit promptForStatus(status_);
}

void
SelfVerificationStatus::invalidate()
{
    applyRuntimeStatus(AllVerified, false, false);
}

void
SelfVerificationStatus::refreshStateFromMatrixRuntime()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        invalidate();
        return;
    }

    QString error;
    const auto recoveryStatus =
      komai::MatrixBackendRuntimeService::fetchRecoveryStatus(handleId, &error);
    if (!recoveryStatus) {
        nhlog::crypto()->warn("Failed to fetch matrix-sdk recovery status: {}",
                              error.toStdString());
        invalidate();
        return;
    }

    if (recoveryStatus->state == QLatin1String("enabled")) {
        applyRuntimeStatus(AllVerified, true, recoveryStatus->hasDevicesToVerifyAgainst);
    } else if (recoveryStatus->state == QLatin1String("incomplete")) {
        applyRuntimeStatus(UnverifiedMasterKey, true, recoveryStatus->hasDevicesToVerifyAgainst);
    } else if (recoveryStatus->state == QLatin1String("disabled")) {
        applyRuntimeStatus(NoMasterKey, false, recoveryStatus->hasDevicesToVerifyAgainst);
    } else {
        applyRuntimeStatus(AllVerified, false, recoveryStatus->hasDevicesToVerifyAgainst);
    }
}

void
SelfVerificationStatus::applyRuntimeStatus(Status status,
                                           bool hasSSSS,
                                           bool canVerifyWithAnotherDevice)
{
    if (status_ != status) {
        status_ = status;
        emit statusChanged();
    }

    if (hasSSSS_ != hasSSSS) {
        hasSSSS_ = hasSSSS;
        emit hasSSSSChanged();
    }

    if (canVerifyWithAnotherDevice_ != canVerifyWithAnotherDevice) {
        canVerifyWithAnotherDevice_ = canVerifyWithAnotherDevice;
        emit canVerifyWithAnotherDeviceChanged();
    }
}

#include "moc_SelfVerificationStatus.cpp"
