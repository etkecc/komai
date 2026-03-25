// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chat/ChatPage.h"

#include "logging/Logging.h"

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
        nhlog::net()->info(
          "Skipping legacy server-side logout attempt on the matrix-sdk migration branch");
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
    nhlog::crypto()->warn(
      "Ignoring legacy secret-download unlock flow on the matrix-sdk migration branch");
    emit showNotification(tr("Key backup recovery has not been migrated to the Rust backend yet."));
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
                                              const QString &)
{
    pendingSecretsUnlockRequest_.reset();
    nhlog::crypto()->warn(
      "Ignoring legacy secret unlock processing on the matrix-sdk migration branch");
    emit showNotification(tr("Key backup recovery has not been migrated to the Rust backend yet."));
}
