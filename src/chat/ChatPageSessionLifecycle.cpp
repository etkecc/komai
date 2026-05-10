// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chat/ChatPage.h"

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"

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

    if (policy != LogoutPolicy::BestEffortServerFirst) {
        finalizeLogout(route, loginMessage);
        return;
    }

    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    const auto authType    = mainWindow ? mainWindow->matrixBackendAuthType() : QString{};

    if (handleId == 0) {
        komai::logging::net()->info(
          "Skipping server-side logout because no matrix-sdk backend handle is "
          "active for the current session");
        finalizeLogout(route, loginMessage);
        return;
    }

    komai::qt_worker_task::runQueued(
      this,
      [handleId]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok =
            komai::MatrixBackendRuntimeService::logoutBackend(context, handleId, &error);
          return std::make_pair(ok, error);
      },
      [route, loginMessage, authType](ChatPage *page, const std::pair<bool, QString> &result) {
          const auto &[ok, error] = result;
          if (ok) {
              komai::logging::net()->info(
                "Completed server-side matrix-sdk logout using auth_type='{}'",
                authType.toStdString());
          } else {
              komai::logging::net()->warn("Best-effort server-side matrix-sdk logout failed for "
                                          "auth_type='{}': {}",
                                          authType.toStdString(),
                                          error.toStdString());
          }

          page->finalizeLogout(route, loginMessage);
      });
}

void
ChatPage::finalizeLogout(LogoutRoute route, const QString &loginMessage)
{
    if (auto *mainWindow = MainWindow::instance())
        mainWindow->stopMatrixBackendHandle();

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
ChatPage::decryptDownloadedSecrets()
{
    pendingSecretsUnlockRequest_ = true;
    komai::logging::crypto()->info(
      "Redirecting downloaded-secret unlock prompt to matrix-sdk recovery");
    emit promptUnlockKeyBackup();
}

void
ChatPage::submitSecretUnlockInput(const QString &text)
{
    if (!pendingSecretsUnlockRequest_) {
        komai::logging::crypto()->warn(
          "Received unlock input, but no pending secrets unlock request exists.");
        return;
    }

    pendingSecretsUnlockRequest_ = false;
    processDownloadedSecretsUnlockInput(text);
}

void
ChatPage::cancelSecretUnlockInput()
{
    if (!pendingSecretsUnlockRequest_)
        return;

    komai::logging::crypto()->info("Secrets unlock prompt dismissed by user.");
    pendingSecretsUnlockRequest_ = false;
}

void
ChatPage::processDownloadedSecretsUnlockInput(const QString &text)
{
    pendingSecretsUnlockRequest_ = false;

    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        emit showNotification(
          tr("Key backup recovery requires an active matrix-sdk backend runtime."));
        return;
    }

    const auto trimmedSecret = text.trimmed();
    if (trimmedSecret.isEmpty()) {
        emit showNotification(
          tr("A recovery key or recovery passphrase is required to unlock key backup."));
        return;
    }

    komai::qt_worker_task::runQueued(
      this,
      [handleId, trimmedSecret]() {
          const auto context = komai::matrix_backend::blockingCallContext();
          QString error;
          const bool ok = komai::MatrixBackendRuntimeService::recoverEncryptionSecrets(
            context, handleId, trimmedSecret, &error);
          return std::make_pair(ok, error);
      },
      [](ChatPage *page, const std::pair<bool, QString> &result) {
          const auto &[ok, error] = result;
          if (!ok) {
              emit page->showNotification(error.isEmpty()
                                            ? tr("Failed to unlock key backup.")
                                            : tr("Failed to unlock key backup: %1").arg(error));
              return;
          }

          komai::logging::crypto()->info(
            "Recovered encryption secrets through the ChatPage unlock entry using matrix-sdk "
            "recovery");
          emit page->showNotification(tr("Encryption secrets unlocked."));
      });
}
