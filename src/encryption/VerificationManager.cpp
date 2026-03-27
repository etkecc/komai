// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "VerificationManager.h"

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"

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
VerificationManager::receivedRoomDeviceVerificationRequest(
  const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationRequest> &message,
  TimelineModel *model)
{
    Q_UNUSED(message);
    Q_UNUSED(model);
    nhlog::crypto()->debug(
      "Ignoring legacy room verification request because matrix-sdk handles live verification "
      "sessions directly");
}

void
VerificationManager::receivedDeviceVerificationRequest(
  const mtx::events::msg::KeyVerificationRequest &msg,
  std::string sender)
{
    Q_UNUSED(msg);
    Q_UNUSED(sender);
    nhlog::crypto()->debug(
      "Ignoring legacy to-device verification request because matrix-sdk handles live "
      "verification sessions directly");
}

void
VerificationManager::receivedDeviceVerificationStart(
  const mtx::events::msg::KeyVerificationStart &msg,
  std::string sender)
{
    Q_UNUSED(msg);
    Q_UNUSED(sender);
    nhlog::crypto()->debug(
      "Ignoring legacy verification start because matrix-sdk handles live verification sessions "
      "directly");
}

bool
VerificationManager::verifySelf(QString *errorOut)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        if (errorOut)
            *errorOut = tr("Matrix backend runtime is not available.");
        return false;
    }

    QString error;
    const auto session =
      komai::MatrixBackendRuntimeService::startSelfVerification(handleId, &error);
    if (!session) {
        if (errorOut)
            *errorOut =
              error.isEmpty() ? tr("Failed to start verification with another device.") : error;
        return false;
    }

    return openMatrixVerificationFlow(handleId, session->flowId, errorOut);
}

bool
VerificationManager::verifyUser(QString userid, QString *errorOut)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        if (errorOut)
            *errorOut = tr("Matrix backend runtime is not available.");
        return false;
    }

    QString error;
    const auto session =
      komai::MatrixBackendRuntimeService::startUserVerification(handleId, userid, &error);
    if (!session) {
        if (errorOut)
            *errorOut =
              error.isEmpty() ? tr("Failed to start verification for \"%1\".").arg(userid) : error;
        return false;
    }

    return openMatrixVerificationFlow(handleId, session->flowId, errorOut);
}

void
VerificationManager::removeVerificationFlow(DeviceVerificationFlow *flow)
{
    if (!flow)
        return;

    activeMatrixFlowIds_.remove(flow->transactionId());
    flow->deleteLater();
}

bool
VerificationManager::verifyDevice(QString userid, QString deviceid, QString *errorOut)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        if (errorOut)
            *errorOut = tr("Matrix backend runtime is not available.");
        return false;
    }

    QString error;
    const auto session = komai::MatrixBackendRuntimeService::startDeviceVerification(
      handleId, userid, deviceid, &error);
    if (!session) {
        if (errorOut)
            *errorOut = error.isEmpty()
                          ? tr("Failed to start verification for device \"%1\".").arg(deviceid)
                          : error;
        return false;
    }

    return openMatrixVerificationFlow(handleId, session->flowId, errorOut);
}

bool
VerificationManager::verifyOneOfDevices(QString userid,
                                        std::vector<QString> deviceids,
                                        QString *errorOut)
{
    QString error;
    for (const auto &deviceId : deviceids) {
        if (verifyDevice(userid, deviceId, &error))
            return true;
    }

    if (errorOut) {
        *errorOut =
          error.isEmpty() ? tr("Failed to start verification for the available devices.") : error;
    }
    return false;
}

bool
VerificationManager::openMatrixVerificationFlow(uint64_t handleId,
                                                const QString &flowId,
                                                QString *errorOut)
{
    QString error;
    auto *flow =
      DeviceVerificationFlow::InitiateMatrixVerificationSession(nullptr, handleId, flowId, &error);
    if (!flow) {
        if (errorOut)
            *errorOut = error.isEmpty() ? tr("Failed to open the verification flow.") : error;
        return false;
    }

    activeMatrixFlowIds_.insert(flowId);
    connect(
      flow, &QObject::destroyed, this, [this, flowId]() { activeMatrixFlowIds_.remove(flowId); });
    connect(flow, &DeviceVerificationFlow::refreshProfile, this, [this, flow]() {
        emit verificationStateChanged(flow->getUserId());
    });
    emit newDeviceVerificationRequest(flow);
    return true;
}

void
VerificationManager::pollPendingMatrixVerifications()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0)
        return;

    QString error;
    const auto pendingFlowIds =
      komai::MatrixBackendRuntimeService::takePendingVerificationFlowIds(handleId, &error);
    if (!pendingFlowIds) {
        if (!error.isEmpty()) {
            nhlog::crypto()->warn("Failed to poll pending matrix-sdk verification requests: {}",
                                  error.toStdString());
        }
        return;
    }

    for (const auto &flowId : *pendingFlowIds) {
        if (flowId.trimmed().isEmpty() || activeMatrixFlowIds_.contains(flowId))
            continue;

        QString flowError;
        auto *flow = DeviceVerificationFlow::InitiateMatrixVerificationSession(
          nullptr, handleId, flowId, &flowError);
        if (!flow) {
            nhlog::crypto()->warn("Failed to adopt pending matrix-sdk verification flow {}: {}",
                                  flowId.toStdString(),
                                  flowError.toStdString());
            continue;
        }

        activeMatrixFlowIds_.insert(flowId);
        connect(flow, &QObject::destroyed, this, [this, flowId]() {
            activeMatrixFlowIds_.remove(flowId);
        });
        connect(flow, &DeviceVerificationFlow::refreshProfile, this, [this, flow]() {
            emit verificationStateChanged(flow->getUserId());
        });
        emit newDeviceVerificationRequest(flow);
    }
}

#include "moc_VerificationManager.cpp"
