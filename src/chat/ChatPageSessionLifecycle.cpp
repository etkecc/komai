// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chat/ChatPage.h"

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"

void
ChatPage::getBackupVersion()
{
    nhlog::crypto()->info(
      "Skipping legacy online key-backup lookup on the matrix-sdk migration branch");
}

void
ChatPage::prepareShutdown()
{
    shuttingDown_ = true;
    connectivityTimer_.stop();
    disconnect();
}

void
ChatPage::initiateLogout()
{
    performLogout(LogoutPolicy::BestEffortServerFirst, LogoutRoute::ViaClosingSignal);
}

void
ChatPage::performLogout(LogoutPolicy policy, LogoutRoute route, const QString &loginMessage)
{
    shuttingDown_ = true;

    if (policy == LogoutPolicy::BestEffortServerFirst) {
        const auto *mainWindow = MainWindow::instance();
        const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;

        if (handleId == 0) {
            nhlog::net()->info("Skipping server-side logout because no matrix-sdk backend handle "
                               "is active for the current session");
        } else {
            QString error;
            if (komai::MatrixBackendRuntimeService::logoutBackend(handleId, &error)) {
                nhlog::net()->info("Completed server-side matrix-sdk logout using auth_type='{}'",
                                   mainWindow->matrixBackendAuthType().toStdString());
            } else {
                nhlog::net()->warn(
                  "Best-effort server-side matrix-sdk logout failed for auth_type='{}': {}",
                  mainWindow->matrixBackendAuthType().toStdString(),
                  error.toStdString());
            }
        }
    }

    finalizeLogout(route, loginMessage);
}

void
ChatPage::finalizeLogout(LogoutRoute route, const QString &loginMessage)
{
    resetUI();
    deleteConfigs();
    emit loggedOut();
    connectivityTimer_.stop();

    if (route == LogoutRoute::ViaClosingSignal)
        emit closing();
    else
        emit showLoginPage(loginMessage);
}

void
ChatPage::decryptDownloadedSecrets(mtx::secret_storage::AesHmacSha2KeyDescription keyDesc,
                                   const SecretsToDecrypt &secrets)
{
    pendingSecretsUnlockRequest_ = PendingSecretsUnlockRequest{std::move(keyDesc), secrets};
    nhlog::crypto()->info("Redirecting legacy downloaded-secret unlock prompt to matrix-sdk "
                          "recovery");
    emit promptUnlockKeyBackup();
}

void
ChatPage::submitSecretUnlockInput(const QString &text)
{
    if (!pendingSecretsUnlockRequest_) {
        nhlog::crypto()->warn(
          "Received unlock input, but no pending secrets unlock request exists.");
        return;
    }

    auto request = std::move(*pendingSecretsUnlockRequest_);
    pendingSecretsUnlockRequest_.reset();
    processDownloadedSecretsUnlockInput(std::move(request.keyDesc), request.secrets, text);
}

void
ChatPage::cancelSecretUnlockInput()
{
    if (!pendingSecretsUnlockRequest_)
        return;

    nhlog::crypto()->info("Secrets unlock prompt dismissed by user.");
    pendingSecretsUnlockRequest_.reset();
}

void
ChatPage::processDownloadedSecretsUnlockInput(mtx::secret_storage::AesHmacSha2KeyDescription,
                                              const SecretsToDecrypt &,
                                              const QString &text)
{
    pendingSecretsUnlockRequest_.reset();

    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        emit showNotification(
          tr("Key backup recovery requires an active matrix-sdk backend runtime."));
        return;
    }

    const auto trimmedSecret = text.trimmed();
    if (trimmedSecret.isEmpty()) {
        emit showNotification(tr("A recovery key or passphrase is required to unlock key backup."));
        return;
    }

    QString error;
    if (!komai::MatrixBackendRuntimeService::recoverEncryptionSecrets(
          handleId, trimmedSecret, &error)) {
        emit showNotification(error.isEmpty() ? tr("Failed to unlock key backup.")
                                              : tr("Failed to unlock key backup: %1").arg(error));
        return;
    }

    nhlog::crypto()->info("Recovered encryption secrets through the legacy ChatPage unlock entry "
                          "using matrix-sdk recovery");
    emit showNotification(tr("Encryption secrets unlocked."));
}
