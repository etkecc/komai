// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "VerificationManager.h"

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"

namespace {
void
notifyNotMigrated()
{
    if (auto *chatPage = ChatPage::instance()) {
        emit chatPage->showNotification(
          VerificationManager::tr("Device verification is not migrated to the matrix-sdk "
                                  "backend yet."));
        return;
    }

    nhlog::crypto()->warn("Device verification is not migrated to matrix-sdk yet");
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
VerificationManager::receivedRoomDeviceVerificationRequest(
  const mtx::events::RoomEvent<mtx::events::msg::KeyVerificationRequest> &message,
  TimelineModel *model)
{
    Q_UNUSED(message);
    Q_UNUSED(model);
    nhlog::crypto()->warn("Ignoring room verification request until matrix-sdk verification is "
                          "implemented");
}

void
VerificationManager::receivedDeviceVerificationRequest(
  const mtx::events::msg::KeyVerificationRequest &msg,
  std::string sender)
{
    Q_UNUSED(msg);
    Q_UNUSED(sender);
    nhlog::crypto()->warn("Ignoring to-device verification request until matrix-sdk "
                          "verification is implemented");
}

void
VerificationManager::receivedDeviceVerificationStart(
  const mtx::events::msg::KeyVerificationStart &msg,
  std::string sender)
{
    Q_UNUSED(msg);
    Q_UNUSED(sender);
    nhlog::crypto()->warn("Ignoring verification start until matrix-sdk verification is "
                          "implemented");
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
    auto *flow = DeviceVerificationFlow::InitiateMatrixSelfVerification(nullptr, handleId, &error);
    if (!flow) {
        if (errorOut)
            *errorOut =
              error.isEmpty() ? tr("Failed to start verification with another device.") : error;
        return false;
    }

    emit newDeviceVerificationRequest(flow);
    return true;
}

void
VerificationManager::verifyUser(QString userid)
{
    Q_UNUSED(userid);
    notifyNotMigrated();
}

void
VerificationManager::removeVerificationFlow(DeviceVerificationFlow *flow)
{
    if (!flow)
        return;

    activeMatrixFlowIds_.remove(flow->transactionId());
    flow->deleteLater();
}

void
VerificationManager::verifyDevice(QString userid, QString deviceid)
{
    Q_UNUSED(userid);
    Q_UNUSED(deviceid);
    notifyNotMigrated();
}

void
VerificationManager::verifyOneOfDevices(QString userid, std::vector<QString> deviceids)
{
    Q_UNUSED(userid);
    Q_UNUSED(deviceids);
    notifyNotMigrated();
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
        emit newDeviceVerificationRequest(flow);
    }
}

#include "moc_VerificationManager.cpp"
