// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "VerificationManager.h"

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"

namespace {
std::vector<QString>
unverifiedDeviceIdsFromRuntimeState(const komai::MatrixUserVerificationState &state)
{
    std::vector<QString> deviceIds;
    deviceIds.reserve(static_cast<size_t>(state.devices.size()));
    for (const auto &device : state.devices) {
        if (device.verificationState == QLatin1String("unverified"))
            deviceIds.push_back(device.deviceId);
    }

    return deviceIds;
}

struct MatrixVerificationStartTaskResult
{
    std::optional<komai::MatrixVerificationSession> session;
    QString error;
};

struct MatrixVerificationSessionTaskResult
{
    std::optional<komai::MatrixVerificationSession> session;
    QString error;
};

struct MatrixVerificationActionTaskResult
{
    bool ok = false;
    QString error;
};

struct MatrixPendingVerificationFlowIdsTaskResult
{
    std::optional<QVector<QString>> flowIds;
    QString error;
};

template<typename WorkFnT, typename UiFnT>
void
runVerificationManagerTask(VerificationManager *manager, WorkFnT work, UiFnT ui)
{
    komai::qt_worker_task::runQueued(manager, std::move(work), std::move(ui));
}
}

VerificationManager::VerificationManager(TimelineViewManager *o)
  : QObject(o)
{
    instance_                    = this;
    matrixVerificationPollTimer_ = new QTimer(this);
    matrixVerificationPollTimer_->setInterval(500);
    connect(matrixVerificationPollTimer_,
            &QTimer::timeout,
            this,
            &VerificationManager::pollPendingMatrixVerifications);
    matrixVerificationPollTimer_->start();
}

void
VerificationManager::verifySelf(FailureCallback onFailure)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        if (onFailure)
            onFailure(tr("Matrix backend runtime is not available."));
        return;
    }

    auto *flow = DeviceVerificationFlow::createPending(nullptr,
                                                       handleId,
                                                       /*isSelfVerification=*/true,
                                                       /*isMultiDeviceVerification=*/true,
                                                       /*userId=*/QString(),
                                                       /*deviceId=*/QString());

    connect(flow,
            &DeviceVerificationFlow::startRequested,
            this,
            [this, flow = QPointer<DeviceVerificationFlow>(flow), handleId]() {
                runVerificationManagerTask(
                  this,
                  [handleId]() {
                      MatrixVerificationStartTaskResult result;
                      const auto context = komai::matrix_backend::blockingCallContext();
                      result.session = komai::MatrixBackendRuntimeService::startSelfVerification(
                        context, handleId, &result.error);
                      return result;
                  },
                  [this, flow, handleId](VerificationManager *,
                                         MatrixVerificationStartTaskResult result) {
                      if (!flow)
                          return;
                      if (!result.session) {
                          flow->handleStartFailure();
                          return;
                      }
                      flow->adoptStartedSession(*result.session);
                      const auto flowId = flow->transactionId();
                      if (!flowId.isEmpty())
                          activeMatrixFlowIds_.insert(flowId);
                  });
            });

    connect(flow, &QObject::destroyed, this, [this, flow]() {
        const auto flowId = flow->transactionId();
        if (!flowId.isEmpty())
            activeMatrixFlowIds_.remove(flowId);
    });
    emit newDeviceVerificationRequest(flow);
}

void
VerificationManager::verifyUser(QString userid, FailureCallback onFailure)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        if (onFailure)
            onFailure(tr("Matrix backend runtime is not available."));
        return;
    }

    auto *flow = DeviceVerificationFlow::createPending(nullptr,
                                                       handleId,
                                                       /*isSelfVerification=*/false,
                                                       /*isMultiDeviceVerification=*/false,
                                                       /*userId=*/userid,
                                                       /*deviceId=*/QString());

    connect(flow,
            &DeviceVerificationFlow::startRequested,
            this,
            [this, flow = QPointer<DeviceVerificationFlow>(flow), handleId, userid]() {
                runVerificationManagerTask(
                  this,
                  [handleId, userid]() {
                      MatrixVerificationStartTaskResult result;
                      const auto context = komai::matrix_backend::blockingCallContext();
                      result.session = komai::MatrixBackendRuntimeService::startUserVerification(
                        context, handleId, userid, &result.error);
                      if (result.session)
                          return result;

                      QString stateError;
                      const auto verificationState =
                        komai::MatrixBackendRuntimeService::fetchUserVerificationState(
                          context, handleId, userid, &stateError);
                      if (!verificationState) {
                          if (result.error.isEmpty() && !stateError.isEmpty())
                              result.error = stateError;
                          return result;
                      }

                      const auto candidateDeviceIds =
                        unverifiedDeviceIdsFromRuntimeState(*verificationState);
                      QString fallbackError;
                      for (const auto &deviceId : candidateDeviceIds) {
                          auto fallbackSession =
                            komai::MatrixBackendRuntimeService::startDeviceVerification(
                              context, handleId, userid, deviceId, &fallbackError);
                          if (fallbackSession) {
                              result.session = std::move(fallbackSession);
                              result.error.clear();
                              return result;
                          }
                      }

                      if (!candidateDeviceIds.empty() && !fallbackError.isEmpty()) {
                          result.error =
                            QObject::tr("%1 Device verification fallback also failed: %2")
                              .arg(result.error.isEmpty()
                                     ? QObject::tr("Failed to start user verification.")
                                     : result.error,
                                   fallbackError);
                      } else if (result.error.isEmpty()) {
                          result.error =
                            QObject::tr("Failed to start verification for \"%1\".").arg(userid);
                      }

                      return result;
                  },
                  [this, flow, handleId](VerificationManager *,
                                         MatrixVerificationStartTaskResult result) {
                      if (!flow)
                          return;
                      if (!result.session) {
                          flow->handleStartFailure();
                          return;
                      }
                      flow->adoptStartedSession(*result.session);
                      const auto flowId = flow->transactionId();
                      if (!flowId.isEmpty())
                          activeMatrixFlowIds_.insert(flowId);
                  });
            });

    connect(flow, &QObject::destroyed, this, [this, flow]() {
        const auto flowId = flow->transactionId();
        if (!flowId.isEmpty())
            activeMatrixFlowIds_.remove(flowId);
    });
    emit newDeviceVerificationRequest(flow);
}

void
VerificationManager::removeVerificationFlow(DeviceVerificationFlow *flow)
{
    if (!flow)
        return;

    const auto flowId   = flow->transactionId().trimmed();
    const auto handleId = flow->backendHandleId();
    activeMatrixFlowIds_.remove(flowId);
    openingMatrixFlowIds_.remove(flowId);
    flow->deleteLater();

    if (!flowId.isEmpty()) {
        if (handleId != 0) {
            runVerificationManagerTask(
              this,
              [handleId, flowId]() {
                  MatrixVerificationActionTaskResult result;
                  const auto context = komai::matrix_backend::blockingCallContext();
                  result.ok          = komai::MatrixBackendRuntimeService::clearVerificationSession(
                    context, handleId, flowId, &result.error);
                  return result;
              },
              [flowId](VerificationManager *, const MatrixVerificationActionTaskResult &result) {
                  if (!result.ok) {
                      komai::logging::crypto()->warn(
                        "Failed to clear matrix-sdk verification flow {}: {}",
                        flowId.toStdString(),
                        result.error.toStdString());
                  }
              });
        }
    }
}

void
VerificationManager::verifyDevice(QString userid, QString deviceid, FailureCallback onFailure)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        if (onFailure)
            onFailure(tr("Matrix backend runtime is not available."));
        return;
    }

    auto *flow = DeviceVerificationFlow::createPending(nullptr,
                                                       handleId,
                                                       /*isSelfVerification=*/false,
                                                       /*isMultiDeviceVerification=*/false,
                                                       /*userId=*/userid,
                                                       /*deviceId=*/deviceid);

    connect(flow,
            &DeviceVerificationFlow::startRequested,
            this,
            [this, flow = QPointer<DeviceVerificationFlow>(flow), handleId, userid, deviceid]() {
                runVerificationManagerTask(
                  this,
                  [handleId, userid, deviceid]() {
                      MatrixVerificationStartTaskResult result;
                      const auto context = komai::matrix_backend::blockingCallContext();
                      result.session = komai::MatrixBackendRuntimeService::startDeviceVerification(
                        context, handleId, userid, deviceid, &result.error);
                      return result;
                  },
                  [this, flow, handleId](VerificationManager *,
                                         MatrixVerificationStartTaskResult result) {
                      if (!flow)
                          return;
                      if (!result.session) {
                          flow->handleStartFailure();
                          return;
                      }
                      flow->adoptStartedSession(*result.session);
                      const auto flowId = flow->transactionId();
                      if (!flowId.isEmpty())
                          activeMatrixFlowIds_.insert(flowId);
                  });
            });

    connect(flow, &QObject::destroyed, this, [this, flow]() {
        const auto flowId = flow->transactionId();
        if (!flowId.isEmpty())
            activeMatrixFlowIds_.remove(flowId);
    });
    emit newDeviceVerificationRequest(flow);
}

void
VerificationManager::verifyOneOfDevices(QString userid,
                                        std::vector<QString> deviceids,
                                        FailureCallback onFailure)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        if (onFailure)
            onFailure(tr("Matrix backend runtime is not available."));
        return;
    }

    const auto firstDeviceId = deviceids.empty() ? QString() : deviceids.front();
    auto *flow               = DeviceVerificationFlow::createPending(nullptr,
                                                       handleId,
                                                       /*isSelfVerification=*/false,
                                                       /*isMultiDeviceVerification=*/false,
                                                       /*userId=*/userid,
                                                       /*deviceId=*/firstDeviceId);

    connect(flow,
            &DeviceVerificationFlow::startRequested,
            this,
            [this,
             flow = QPointer<DeviceVerificationFlow>(flow),
             handleId,
             userid,
             deviceids = std::move(deviceids)]() {
                runVerificationManagerTask(
                  this,
                  [handleId, userid, deviceids]() {
                      MatrixVerificationStartTaskResult result;
                      const auto context = komai::matrix_backend::blockingCallContext();
                      for (const auto &deviceId : deviceids) {
                          result.session =
                            komai::MatrixBackendRuntimeService::startDeviceVerification(
                              context, handleId, userid, deviceId, &result.error);
                          if (result.session)
                              return result;
                      }
                      return result;
                  },
                  [this, flow, handleId](VerificationManager *,
                                         MatrixVerificationStartTaskResult result) {
                      if (!flow)
                          return;
                      if (!result.session) {
                          flow->handleStartFailure();
                          return;
                      }
                      flow->adoptStartedSession(*result.session);
                      const auto flowId = flow->transactionId();
                      if (!flowId.isEmpty())
                          activeMatrixFlowIds_.insert(flowId);
                  });
            });

    connect(flow, &QObject::destroyed, this, [this, flow]() {
        const auto flowId = flow->transactionId();
        if (!flowId.isEmpty())
            activeMatrixFlowIds_.remove(flowId);
    });
    emit newDeviceVerificationRequest(flow);
}

void
VerificationManager::adoptMatrixVerificationSession(uint64_t handleId,
                                                    const komai::MatrixVerificationSession &session)
{
    const auto flowId = session.flowId.trimmed();
    if (flowId.isEmpty())
        return;
    if (activeMatrixFlowIds_.contains(flowId)) {
        openingMatrixFlowIds_.remove(flowId);
        return;
    }

    auto *flow = DeviceVerificationFlow::createFromMatrixSession(nullptr, handleId, session);
    openingMatrixFlowIds_.remove(flowId);

    activeMatrixFlowIds_.insert(flowId);
    connect(
      flow, &QObject::destroyed, this, [this, flowId]() { activeMatrixFlowIds_.remove(flowId); });
    connect(flow, &DeviceVerificationFlow::refreshProfile, this, [this, flow]() {
        emit verificationStateChanged(flow->getUserId());
    });
    emit newDeviceVerificationRequest(flow);
}

void
VerificationManager::requestMatrixVerificationFlow(uint64_t handleId, const QString &flowId)
{
    const auto trimmedFlowId = flowId.trimmed();
    if (handleId == 0 || trimmedFlowId.isEmpty() || activeMatrixFlowIds_.contains(trimmedFlowId) ||
        openingMatrixFlowIds_.contains(trimmedFlowId)) {
        return;
    }

    openingMatrixFlowIds_.insert(trimmedFlowId);
    runVerificationManagerTask(
      this,
      [handleId, trimmedFlowId]() {
          MatrixVerificationSessionTaskResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.session     = komai::MatrixBackendRuntimeService::fetchVerificationSession(
            context, handleId, trimmedFlowId, &result.error);
          return result;
      },
      [this, handleId, trimmedFlowId](VerificationManager *,
                                      const MatrixVerificationSessionTaskResult &result) {
          openingMatrixFlowIds_.remove(trimmedFlowId);
          if (!result.session) {
              komai::logging::crypto()->warn(
                "Failed to adopt pending matrix-sdk verification flow {}: {}",
                trimmedFlowId.toStdString(),
                result.error.toStdString());
              return;
          }

          adoptMatrixVerificationSession(handleId, *result.session);
      });
}

void
VerificationManager::pollPendingMatrixVerifications()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0)
        return;
    if (matrixVerificationPollInFlight_) {
        matrixVerificationPollPending_ = true;
        return;
    }

    matrixVerificationPollInFlight_ = true;
    runVerificationManagerTask(
      this,
      [handleId]() {
          MatrixPendingVerificationFlowIdsTaskResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.flowIds     = komai::MatrixBackendRuntimeService::takePendingVerificationFlowIds(
            context, handleId, &result.error);
          return result;
      },
      [this, handleId](VerificationManager *,
                       const MatrixPendingVerificationFlowIdsTaskResult &result) {
          matrixVerificationPollInFlight_ = false;
          const bool rerun                = std::exchange(matrixVerificationPollPending_, false);

          const auto *mainWindow    = MainWindow::instance();
          const auto activeHandleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
          if (activeHandleId != handleId) {
              if (rerun)
                  QTimer::singleShot(0, this, &VerificationManager::pollPendingMatrixVerifications);
              return;
          }

          if (!result.flowIds) {
              if (!result.error.isEmpty()) {
                  komai::logging::crypto()->warn(
                    "Failed to poll pending matrix-sdk verification requests: {}",
                    result.error.toStdString());
              }
              if (rerun)
                  QTimer::singleShot(0, this, &VerificationManager::pollPendingMatrixVerifications);
              return;
          }

          for (const auto &flowId : *result.flowIds) {
              requestMatrixVerificationFlow(handleId, flowId);
          }

          if (rerun)
              QTimer::singleShot(0, this, &VerificationManager::pollPendingMatrixVerifications);
      });
}

#include "moc_VerificationManager.cpp"
