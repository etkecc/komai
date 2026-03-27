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
#include "utils/Utils.h"

namespace {
QString
notMigratedMessage()
{
    return SelfVerificationStatus::tr("Failed to start the matrix-sdk self-verification flow.");
}

QString
missingRuntimeMessage()
{
    return SelfVerificationStatus::tr(
      "The Rust Matrix backend is not active, so encryption recovery is unavailable.");
}

QString
formattedRecoveryKey(QString recoveryKey)
{
    recoveryKey = recoveryKey.trimmed();
    if (recoveryKey.contains(u' '))
        return recoveryKey;

    QString formatted;
    formatted.reserve(recoveryKey.size() + recoveryKey.size() / 4);
    for (qsizetype i = 0; i < recoveryKey.size(); i += 4) {
        if (!formatted.isEmpty())
            formatted += u' ';
        formatted += recoveryKey.mid(i, 4);
    }

    return formatted;
}

void
notifyLocalVerificationStateRefresh()
{
    if (auto *verificationManager = VerificationManager::instance()) {
        emit verificationManager->verificationStateChanged(utils::localUser());
    }
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

    if (auto *verificationManager = VerificationManager::instance()) {
        connect(verificationManager,
                &VerificationManager::verificationStateChanged,
                this,
                [this](const QString &userId) {
                    if (userId.trimmed() != utils::localUser())
                        return;

                    QTimer::singleShot(
                      0, this, &SelfVerificationStatus::refreshStateFromMatrixRuntime);
                });
    }

    QTimer::singleShot(0, this, &SelfVerificationStatus::refreshStateFromMatrixRuntime);
}

void
SelfVerificationStatus::setupCrosssigning(bool useSSSS,
                                          const QString &password,
                                          bool encryptionBackupOnlineEnabled)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        emit setupFailed(missingRuntimeMessage());
        return;
    }

    QString error;
    const auto result = komai::MatrixBackendRuntimeService::setupRecovery(
      handleId, useSSSS, password, encryptionBackupOnlineEnabled, &error);
    if (!result) {
        emit setupFailed(tr("Failed to set up encryption recovery: %1").arg(error));
        return;
    }

    nhlog::crypto()->info("Configured matrix-sdk encryption recovery "
                          "(store_secrets_online={}, online_backup_enabled={})",
                          useSSSS,
                          encryptionBackupOnlineEnabled);

    refreshStateFromMatrixRuntime();
    notifyLocalVerificationStateRefresh();

    if (!result->recoveryKey.trimmed().isEmpty()) {
        emit showRecoveryKey(formattedRecoveryKey(result->recoveryKey));
    } else {
        emit setupCompleted();
    }
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
    notifyLocalVerificationStateRefresh();
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
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        emit setupFailed(missingRuntimeMessage());
        return;
    }

    auto *verificationManager = VerificationManager::instance();
    if (!verificationManager) {
        emit setupFailed(tr("The verification manager is not available."));
        return;
    }

    QString error;
    const auto ownVerificationState =
      komai::MatrixBackendRuntimeService::fetchUserVerificationState(
        handleId, utils::localUser(), &error);
    if (!ownVerificationState) {
        emit setupFailed(error.isEmpty() ? tr("Failed to inspect your signed-in devices.") : error);
        return;
    }

    std::vector<QString> candidateDeviceIds;
    candidateDeviceIds.reserve(static_cast<size_t>(ownVerificationState->devices.size()));
    for (const auto &device : ownVerificationState->devices) {
        if (device.verificationState == QLatin1String("unverified")) {
            candidateDeviceIds.push_back(device.deviceId);
        }
    }

    if (candidateDeviceIds.empty()) {
        refreshStateFromMatrixRuntime();
        emit setupFailed(tr("No unverified signed-in devices are currently available."));
        return;
    }

    if (!verificationManager->verifyOneOfDevices(
          utils::localUser(), std::move(candidateDeviceIds), &error)) {
        emit setupFailed(error.isEmpty()
                           ? tr("Failed to start verification for your other signed-in devices.")
                           : error);
    }
}

void
SelfVerificationStatus::setupEncryptionBackup()
{
    setupCrosssigning(true, QString(), true);
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
        notifyLocalVerificationStateRefresh();
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
    notifyLocalVerificationStateRefresh();
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
    notifyLocalVerificationStateRefresh();
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

    const auto hasSSSS = recoveryStatus->state == QLatin1String("enabled") ||
                         recoveryStatus->state == QLatin1String("incomplete");

    Status nextStatus = AllVerified;
    if (recoveryStatus->state == QLatin1String("disabled")) {
        nextStatus = NoMasterKey;
    } else if (!recoveryStatus->ownDeviceIsVerified) {
        nextStatus = UnverifiedMasterKey;
    } else if (recoveryStatus->hasUnverifiedOwnDevices) {
        nextStatus = UnverifiedDevices;
    }

    applyRuntimeStatus(nextStatus, hasSSSS, recoveryStatus->hasDevicesToVerifyAgainst);
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
