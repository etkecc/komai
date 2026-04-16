// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SelfVerificationStatus.h"

#include <array>

#include <QPointer>
#include <QTimer>

#include "VerificationManager.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"
#include "utils/Utils.h"

namespace {
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

struct SetupRecoveryTaskResult
{
    std::optional<komai::MatrixSetupRecoveryResult> result;
    QString error;
};

struct BoolTaskResult
{
    bool ok = false;
    QString error;
};

struct ResetEncryptionIdentityTaskResult
{
    std::optional<komai::MatrixResetEncryptionIdentityResult> result;
    QString error;
};

struct RecoveryStatusTaskResult
{
    std::optional<komai::MatrixRecoveryStatus> result;
    QString error;
};

struct VerifyUnverifiedDevicesTaskResult
{
    bool fetched = false;
    std::vector<QString> candidateDeviceIds;
    QString error;
};

template<typename WorkFnT, typename UiFnT>
void
runSelfVerificationTask(SelfVerificationStatus *status, WorkFnT work, UiFnT ui)
{
    komai::qt_worker_task::runQueued(status, std::move(work), std::move(ui));
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
                &SelfVerificationStatus::scheduleRuntimeStateRefresh);
        connect(chatPage, &ChatPage::loggedOut, this, &SelfVerificationStatus::invalidate);
    }

    if (auto *verificationManager = VerificationManager::instance()) {
        connect(verificationManager,
                &VerificationManager::verificationStateChanged,
                this,
                [this](const QString &userId) {
                    if (userId.trimmed() != utils::localUser())
                        return;

                    scheduleRuntimeStateRefresh();
                });
    }

    if (auto *app = qGuiApp) {
        connect(
          app, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
              if (state == Qt::ApplicationActive)
                  scheduleRuntimeStateRefresh();
          });
    }

    scheduleRuntimeStateRefresh();
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

    runSelfVerificationTask(
      this,
      [handleId, useSSSS, password, encryptionBackupOnlineEnabled]() {
          SetupRecoveryTaskResult taskResult;
          const auto context = komai::matrix_backend::blockingCallContext();
          taskResult.result  = komai::MatrixBackendRuntimeService::setupRecovery(
            context, handleId, useSSSS, password, encryptionBackupOnlineEnabled, &taskResult.error);
          return taskResult;
      },
      [useSSSS, encryptionBackupOnlineEnabled](SelfVerificationStatus *status,
                                               const SetupRecoveryTaskResult &taskResult) {
          if (!taskResult.result) {
              emit status->setupFailed(
                status->tr("Failed to set up encryption recovery: %1").arg(taskResult.error));
              return;
          }

          komai::logging::crypto()->info("Configured matrix-sdk encryption recovery "
                                         "(store_secrets_online={}, online_backup_enabled={})",
                                         useSSSS,
                                         encryptionBackupOnlineEnabled);

          status->scheduleRuntimeStateRefresh();
          notifyLocalVerificationStateRefresh();

          if (!taskResult.result->recoveryKey.trimmed().isEmpty()) {
              emit status->showRecoveryKey(formattedRecoveryKey(taskResult.result->recoveryKey));
          } else {
              emit status->setupCompleted();
          }
      });
}

bool
SelfVerificationStatus::verifyMasterKey()
{
    scheduleRuntimeStateRefresh();

    if (!canVerifyWithAnotherDevice_) {
        emit setupFailed(tr("No other signed-in device is currently available for verification."));
        return false;
    }

    auto *verificationManager = VerificationManager::instance();
    if (!verificationManager) {
        emit setupFailed(tr("The verification manager is not available."));
        return false;
    }

    const QPointer<SelfVerificationStatus> guard(this);
    verificationManager->verifySelf([guard](const QString &error) {
        if (!guard)
            return;

        emit guard->setupFailed(error);
    });
    return true;
}

void
SelfVerificationStatus::verifyMasterKeyWithPassphrase()
{
    scheduleRuntimeStateRefresh();

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

    runSelfVerificationTask(
      this,
      [handleId, keyOrPassphrase]() {
          BoolTaskResult taskResult;
          const auto context = komai::matrix_backend::blockingCallContext();
          taskResult.ok      = komai::MatrixBackendRuntimeService::recoverEncryptionSecrets(
            context, handleId, keyOrPassphrase, &taskResult.error);
          return taskResult;
      },
      [](SelfVerificationStatus *status, const BoolTaskResult &taskResult) {
          if (!taskResult.ok) {
              emit status->setupFailed(
                status->tr("Failed to unlock key backup: %1").arg(taskResult.error));
              return;
          }

          komai::logging::crypto()->info(
            "Recovered encryption secrets through matrix-sdk recovery");
          status->scheduleRuntimeStateRefresh();
          notifyLocalVerificationStateRefresh();
          emit status->unlockKeyBackupCompleted();
      });
}

void
SelfVerificationStatus::cancelUnlockKeyBackup()
{
    komai::logging::crypto()->debug("Cancelled matrix-sdk key-backup unlock prompt");
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

    runSelfVerificationTask(
      this,
      [handleId]() {
          VerifyUnverifiedDevicesTaskResult taskResult;
          const auto context = komai::matrix_backend::blockingCallContext();
          const auto ownVerificationState =
            komai::MatrixBackendRuntimeService::fetchUserVerificationState(
              context, handleId, utils::localUser(), &taskResult.error);
          if (!ownVerificationState)
              return taskResult;

          taskResult.fetched = true;
          taskResult.candidateDeviceIds.reserve(
            static_cast<size_t>(ownVerificationState->devices.size()));
          for (const auto &device : ownVerificationState->devices) {
              if (device.verificationState == QLatin1String("unverified"))
                  taskResult.candidateDeviceIds.push_back(device.deviceId);
          }

          return taskResult;
      },
      [verificationManager](SelfVerificationStatus *status,
                            const VerifyUnverifiedDevicesTaskResult &taskResult) {
          if (!taskResult.fetched) {
              emit status->setupFailed(taskResult.error.isEmpty()
                                         ? status->tr("Failed to inspect your signed-in devices.")
                                         : taskResult.error);
              return;
          }

          if (taskResult.candidateDeviceIds.empty()) {
              status->scheduleRuntimeStateRefresh();
              emit status->setupFailed(
                status->tr("No unverified signed-in devices are currently available."));
              return;
          }

          const QPointer<SelfVerificationStatus> guard(status);
          verificationManager->verifyOneOfDevices(
            utils::localUser(), taskResult.candidateDeviceIds, [guard](const QString &error) {
                if (!guard)
                    return;

                emit guard->setupFailed(
                  error.isEmpty()
                    ? guard->tr("Failed to start verification for your other signed-in devices.")
                    : error);
            });
      });
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

    runSelfVerificationTask(
      this,
      [handleId]() {
          ResetEncryptionIdentityTaskResult taskResult;
          const auto context = komai::matrix_backend::blockingCallContext();
          taskResult.result  = komai::MatrixBackendRuntimeService::startResetEncryptionIdentity(
            context, handleId, &taskResult.error);
          return taskResult;
      },
      [](SelfVerificationStatus *status, const ResetEncryptionIdentityTaskResult &taskResult) {
          if (!taskResult.result) {
              emit status->setupFailed(
                status->tr("Failed to reset encryption identity: %1").arg(taskResult.error));
              return;
          }

          if (taskResult.result->completed) {
              komai::logging::crypto()->info(
                "Reset encryption identity through matrix-sdk without extra auth");
              status->scheduleRuntimeStateRefresh();
              notifyLocalVerificationStateRefresh();
              status->setupEncryptionBackup();
              return;
          }

          if (taskResult.result->authType == QLatin1String("password")) {
              emit status->promptResetEncryptionIdentityPassword();
              return;
          }

          if (taskResult.result->authType == QLatin1String("oauth")) {
              emit status->promptResetEncryptionIdentityApproval(taskResult.result->approvalUrl);
              return;
          }

          emit status->setupFailed(status->tr("The encryption identity reset flow returned an "
                                              "unsupported authentication type."));
      });
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

    runSelfVerificationTask(
      this,
      [handleId, password]() {
          BoolTaskResult taskResult;
          const auto context = komai::matrix_backend::blockingCallContext();
          taskResult.ok =
            komai::MatrixBackendRuntimeService::continueResetEncryptionIdentityWithPassword(
              context, handleId, password, &taskResult.error);
          return taskResult;
      },
      [](SelfVerificationStatus *status, const BoolTaskResult &taskResult) {
          if (!taskResult.ok) {
              emit status->setupFailed(
                status->tr("Failed to complete encryption identity reset: %1")
                  .arg(taskResult.error));
              return;
          }

          komai::logging::crypto()->info(
            "Completed matrix-sdk encryption identity reset using password authentication");
          status->scheduleRuntimeStateRefresh();
          notifyLocalVerificationStateRefresh();
          status->setupEncryptionBackup();
      });
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

    runSelfVerificationTask(
      this,
      [handleId]() {
          BoolTaskResult taskResult;
          const auto context = komai::matrix_backend::blockingCallContext();
          taskResult.ok =
            komai::MatrixBackendRuntimeService::continueResetEncryptionIdentityAfterApproval(
              context, handleId, &taskResult.error);
          return taskResult;
      },
      [](SelfVerificationStatus *status, const BoolTaskResult &taskResult) {
          if (!taskResult.ok) {
              emit status->setupFailed(
                status->tr("Failed to complete encryption identity reset: %1")
                  .arg(taskResult.error));
              return;
          }

          komai::logging::crypto()->info(
            "Completed matrix-sdk encryption identity reset after browser approval");
          status->scheduleRuntimeStateRefresh();
          notifyLocalVerificationStateRefresh();
          status->setupEncryptionBackup();
      });
}

void
SelfVerificationStatus::cancelResetEncryptionIdentity()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0)
        return;

    runSelfVerificationTask(
      this,
      [handleId]() {
          BoolTaskResult taskResult;
          const auto context = komai::matrix_backend::blockingCallContext();
          taskResult.ok      = komai::MatrixBackendRuntimeService::cancelResetEncryptionIdentity(
            context, handleId, &taskResult.error);
          return taskResult;
      },
      [](SelfVerificationStatus *, const BoolTaskResult &taskResult) {
          if (!taskResult.ok) {
              komai::logging::crypto()->warn(
                "Failed to cancel pending matrix-sdk encryption identity reset: {}",
                taskResult.error.toStdString());
          }
      });
}

void
SelfVerificationStatus::promptCurrentVerificationAction()
{
    promptCurrentActionAfterRefresh_ = true;
    refreshStateFromMatrixRuntime();
}

void
SelfVerificationStatus::invalidate()
{
    runtimeStateRefreshInFlight_     = false;
    runtimeStateRefreshPending_      = false;
    promptCurrentActionAfterRefresh_ = false;
    applyRuntimeStatus(AllVerified, false, false);
}

void
SelfVerificationStatus::scheduleRuntimeStateRefresh()
{
    static constexpr std::array<int, 4> refreshDelaysMs = {0, 250, 1000, 3000};

    for (const auto delayMs : refreshDelaysMs) {
        QTimer::singleShot(delayMs, this, &SelfVerificationStatus::refreshStateFromMatrixRuntime);
    }
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

    if (runtimeStateRefreshInFlight_) {
        runtimeStateRefreshPending_ = true;
        return;
    }

    runtimeStateRefreshInFlight_ = true;
    runSelfVerificationTask(
      this,
      [handleId]() {
          RecoveryStatusTaskResult taskResult;
          const auto context = komai::matrix_backend::blockingCallContext();
          taskResult.result  = komai::MatrixBackendRuntimeService::fetchRecoveryStatus(
            context, handleId, &taskResult.error);
          return taskResult;
      },
      [handleId](SelfVerificationStatus *status, const RecoveryStatusTaskResult &taskResult) {
          status->runtimeStateRefreshInFlight_ = false;
          const bool rerun = std::exchange(status->runtimeStateRefreshPending_, false);

          const auto *mainWindow    = MainWindow::instance();
          const auto activeHandleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
          if (activeHandleId != handleId) {
              if (rerun)
                  QTimer::singleShot(
                    0, status, &SelfVerificationStatus::refreshStateFromMatrixRuntime);
              return;
          }

          if (!taskResult.result) {
              komai::logging::crypto()->warn("Failed to fetch matrix-sdk recovery status: {}",
                                             taskResult.error.toStdString());
              status->invalidate();
          } else {
              const auto hasSSSS = taskResult.result->state == QLatin1String("enabled") ||
                                   taskResult.result->state == QLatin1String("incomplete");

              komai::logging::crypto()->debug(
                "Matrix recovery status state='{}' has_ssss={} "
                "own_device_is_verified={} has_unverified_own_devices={} "
                "has_devices_to_verify_against={}",
                taskResult.result->state.toStdString(),
                hasSSSS,
                taskResult.result->ownDeviceIsVerified,
                taskResult.result->hasUnverifiedOwnDevices,
                taskResult.result->hasDevicesToVerifyAgainst);

              Status nextStatus = AllVerified;
              if (taskResult.result->state == QLatin1String("disabled")) {
                  nextStatus = NoMasterKey;
              } else if (!taskResult.result->ownDeviceIsVerified) {
                  nextStatus = UnverifiedMasterKey;
              } else if (taskResult.result->hasUnverifiedOwnDevices) {
                  nextStatus = UnverifiedDevices;
              }

              status->applyRuntimeStatus(
                nextStatus, hasSSSS, taskResult.result->hasDevicesToVerifyAgainst);
          }

          if (status->promptCurrentActionAfterRefresh_) {
              status->promptCurrentActionAfterRefresh_ = false;
              if (status->status_ != AllVerified)
                  emit status->promptForStatus(status->status_);
          }

          if (rerun)
              QTimer::singleShot(0, status, &SelfVerificationStatus::refreshStateFromMatrixRuntime);
      });
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
