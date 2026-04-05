// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DeviceVerificationFlow.h"

#include <QTimer>

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "utils/QtWorkerTask.h"

namespace {
DeviceVerificationFlow::State
matrixStateFromString(const QString &state)
{
    if (state == QLatin1String("PromptStartVerification"))
        return DeviceVerificationFlow::PromptStartVerification;
    if (state == QLatin1String("WaitingForOtherToAccept"))
        return DeviceVerificationFlow::WaitingForOtherToAccept;
    if (state == QLatin1String("WaitingForKeys"))
        return DeviceVerificationFlow::WaitingForKeys;
    if (state == QLatin1String("CompareEmoji"))
        return DeviceVerificationFlow::CompareEmoji;
    if (state == QLatin1String("CompareNumber"))
        return DeviceVerificationFlow::CompareNumber;
    if (state == QLatin1String("WaitingForMac"))
        return DeviceVerificationFlow::WaitingForMac;
    if (state == QLatin1String("Success"))
        return DeviceVerificationFlow::Success;
    if (state == QLatin1String("Failed"))
        return DeviceVerificationFlow::Failed;
    return DeviceVerificationFlow::Failed;
}

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

std::vector<int>
toSasList(const QVector<int> &sasNumbers)
{
    std::vector<int> numbers;
    numbers.reserve(static_cast<size_t>(sasNumbers.size()));
    for (const auto number : sasNumbers)
        numbers.push_back(number);
    return numbers;
}

template<typename WorkFnT, typename UiFnT>
void
runVerificationFlowTask(DeviceVerificationFlow *flow, WorkFnT work, UiFnT ui)
{
    komai::qt_worker_task::runQueued(flow, std::move(work), std::move(ui));
}
}

DeviceVerificationFlow::DeviceVerificationFlow(QObject *parent,
                                               DeviceVerificationFlow::Type flow_type,
                                               const QString &userID,
                                               const std::vector<QString> &deviceIds_)
  : QObject(parent)
  , sender(false)
  , type(flow_type)
  , userId_(userID)
  , deviceIds(deviceIds_)
{
    if (deviceIds.size() == 1)
        deviceId = deviceIds.front();

    state_ = Failed;
    error_ = UnknownMethod;
}

QString
DeviceVerificationFlow::state()
{
    switch (state_) {
    case PromptStartVerification:
        return QStringLiteral("PromptStartVerification");
    case CompareEmoji:
        return QStringLiteral("CompareEmoji");
    case CompareNumber:
        return QStringLiteral("CompareNumber");
    case WaitingForKeys:
        return QStringLiteral("WaitingForKeys");
    case WaitingForOtherToAccept:
        return QStringLiteral("WaitingForOtherToAccept");
    case WaitingForMac:
        return QStringLiteral("WaitingForMac");
    case Success:
        return QStringLiteral("Success");
    case Failed:
        return QStringLiteral("Failed");
    default:
        return {};
    }
}

void
DeviceVerificationFlow::next()
{
    if (backendHandleId_ == 0 || transaction_id.empty()) {
        failUnavailable();
        return;
    }
    if (matrixAdvanceInFlight_)
        return;

    if (state_ == PromptStartVerification) {
        pendingAutoStart_ = sender && deviceId.isEmpty();
        setState(sender ? WaitingForOtherToAccept : WaitingForKeys);
    }

    const auto handleId    = backendHandleId_;
    const auto flowId      = transactionId();
    matrixAdvanceInFlight_ = true;

    runVerificationFlowTask(
      this,
      [handleId, flowId]() {
          MatrixVerificationActionTaskResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.ok          = komai::MatrixBackendRuntimeService::advanceVerificationSession(
            context, handleId, flowId, &result.error);
          return result;
      },
      [flowId](DeviceVerificationFlow *flow, const MatrixVerificationActionTaskResult &result) {
          flow->matrixAdvanceInFlight_ = false;
          if (!result.ok) {
              nhlog::crypto()->warn("Failed to advance matrix-sdk verification flow {}: {}",
                                    flowId.toStdString(),
                                    result.error.toStdString());
              flow->cancelVerification(UnknownMethod);
              emit flow->refreshProfile();
              return;
          }

          flow->refreshFromMatrixRuntime();
      });
}

QString
DeviceVerificationFlow::getUserId()
{
    return userId_;
}

QString
DeviceVerificationFlow::getDeviceId()
{
    return deviceId;
}

bool
DeviceVerificationFlow::getSender()
{
    return sender;
}

std::vector<int>
DeviceVerificationFlow::getSasList()
{
    return sasList;
}

bool
DeviceVerificationFlow::isSelfVerification() const
{
    return isSelfVerification_;
}

void
DeviceVerificationFlow::setEventId(const std::string &event_id_)
{
    transaction_id = event_id_;
}

void
DeviceVerificationFlow::cancel()
{
    const auto handleId = backendHandleId_;
    const auto flowId   = transactionId();
    const auto mismatch = state_ == CompareEmoji || state_ == CompareNumber;

    cancelVerification(User);
    emit refreshProfile();

    if (handleId == 0 || flowId.trimmed().isEmpty())
        return;

    runVerificationFlowTask(
      this,
      [handleId, flowId, mismatch]() {
          MatrixVerificationActionTaskResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.ok          = komai::MatrixBackendRuntimeService::cancelVerificationSession(
            context, handleId, flowId, mismatch, &result.error);
          return result;
      },
      [flowId](DeviceVerificationFlow *, const MatrixVerificationActionTaskResult &result) {
          if (!result.ok) {
              nhlog::crypto()->warn("Failed to cancel matrix-sdk verification flow {}: {}",
                                    flowId.toStdString(),
                                    result.error.toStdString());
          }
      });
}

void
DeviceVerificationFlow::unverify()
{
    if (backendHandleId_ == 0 || userId_.trimmed().isEmpty() || deviceId.trimmed().isEmpty()) {
        nhlog::crypto()->warn("Cannot clear verification without an active matrix-sdk device "
                              "verification target");
        emit refreshProfile();
        return;
    }

    if (matrixUnverifyInFlight_)
        return;

    const auto handleId           = backendHandleId_;
    const auto userId             = userId_;
    const auto deviceIdToUnverify = deviceId;
    matrixUnverifyInFlight_       = true;

    runVerificationFlowTask(
      this,
      [handleId, userId, deviceIdToUnverify]() {
          MatrixVerificationActionTaskResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.ok          = komai::MatrixBackendRuntimeService::unverifyDevice(
            context, handleId, userId, deviceIdToUnverify, &result.error);
          return result;
      },
      [userId, deviceIdToUnverify](DeviceVerificationFlow *flow,
                                   const MatrixVerificationActionTaskResult &result) {
          flow->matrixUnverifyInFlight_ = false;
          if (!result.ok) {
              nhlog::crypto()->warn("Failed to clear matrix-sdk local trust for {}:{}: {}",
                                    userId.toStdString(),
                                    deviceIdToUnverify.toStdString(),
                                    result.error.toStdString());
          }

          emit flow->refreshProfile();
      });
}

void
DeviceVerificationFlow::cancelVerification(DeviceVerificationFlow::Error error_code)
{
    error_ = error_code;
    setState(Failed);
    emit errorChanged();
}

void
DeviceVerificationFlow::failUnavailable()
{
    nhlog::crypto()->warn("Device verification flow is unavailable because the matrix-sdk runtime "
                          "or flow id is missing");
    cancelVerification(UnknownMethod);
    emit refreshProfile();
}

void
DeviceVerificationFlow::refreshFromMatrixRuntime()
{
    if (backendHandleId_ == 0 || transaction_id.empty())
        return;
    if (matrixRefreshInFlight_) {
        matrixRefreshPending_ = true;
        return;
    }

    const auto handleId    = backendHandleId_;
    const auto flowId      = transactionId();
    matrixRefreshInFlight_ = true;

    runVerificationFlowTask(
      this,
      [handleId, flowId]() {
          MatrixVerificationSessionTaskResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.session     = komai::MatrixBackendRuntimeService::fetchVerificationSession(
            context, handleId, flowId, &result.error);
          return result;
      },
      [handleId, flowId](DeviceVerificationFlow *flow,
                         const MatrixVerificationSessionTaskResult &result) {
          flow->matrixRefreshInFlight_ = false;

          const bool rerun = std::exchange(flow->matrixRefreshPending_, false);
          if (flow->backendHandleId_ != handleId || flow->transactionId() != flowId) {
              if (rerun)
                  QTimer::singleShot(0, flow, &DeviceVerificationFlow::refreshFromMatrixRuntime);
              return;
          }

          if (!result.session) {
              nhlog::crypto()->warn("Failed to fetch matrix-sdk verification flow {}: {}",
                                    flowId.toStdString(),
                                    result.error.toStdString());
              if (rerun)
                  QTimer::singleShot(0, flow, &DeviceVerificationFlow::refreshFromMatrixRuntime);
              return;
          }

          flow->applyMatrixSession(*result.session);

          if (rerun && flow->state_ != Success && flow->state_ != Failed) {
              QTimer::singleShot(0, flow, &DeviceVerificationFlow::refreshFromMatrixRuntime);
          }
      });
}

void
DeviceVerificationFlow::startMatrixRefreshTimer()
{
    auto *timer = new QTimer(this);
    timer->setInterval(250);
    QObject::connect(
      timer, &QTimer::timeout, this, &DeviceVerificationFlow::refreshFromMatrixRuntime);
    timer->start();
}

void
DeviceVerificationFlow::applyMatrixSession(const komai::MatrixVerificationSession &session)
{
    transaction_id = session.flowId.toStdString();

    const bool detailsChangedNow =
      sender != session.sender || deviceId != session.deviceId ||
      isSelfVerification_ != session.isSelfVerification ||
      isMultiDeviceVerification_ != session.isMultiDeviceVerification ||
      sasList != toSasList(session.sasNumbers);

    sender                     = session.sender;
    deviceId                   = session.deviceId;
    isSelfVerification_        = session.isSelfVerification;
    isMultiDeviceVerification_ = session.isMultiDeviceVerification;
    sasList                    = toSasList(session.sasNumbers);

    const auto nextError = mapMatrixError(session.error);
    if (error_ != nextError) {
        error_ = nextError;
        emit errorChanged();
    }

    const auto nextState = matrixStateFromString(session.state);

    // When auto-start is pending and the backend still reports PromptStartVerification,
    // skip setting that state — we'll override it to WaitingForOtherToAccept below.
    // Without this guard, the state bounces every poll cycle and restarts the
    // StackView slide animation in the QML dialog.
    const bool suppressBackendState =
      pendingAutoStart_ && nextState == PromptStartVerification && deviceId.isEmpty();
    if (!suppressBackendState)
        setState(nextState);

    if (detailsChangedNow)
        emit detailsChanged();
    if (state_ == Success || state_ == Failed)
        emit refreshProfile();

    if (pendingAutoStart_ && state_ == PromptStartVerification) {
        if (deviceId.isEmpty()) {
            setState(WaitingForOtherToAccept);
        } else {
            pendingAutoStart_ = false;
            QTimer::singleShot(0, this, &DeviceVerificationFlow::next);
        }
    }
}

DeviceVerificationFlow::Error
DeviceVerificationFlow::mapMatrixError(const QString &error) const
{
    if (error == QLatin1String("MismatchedCommitment"))
        return MismatchedCommitment;
    if (error == QLatin1String("MismatchedSAS"))
        return MismatchedSAS;
    if (error == QLatin1String("KeyMismatch"))
        return KeyMismatch;
    if (error == QLatin1String("Timeout"))
        return Timeout;
    if (error == QLatin1String("User"))
        return User;
    if (error == QLatin1String("AcceptedOnOtherDevice"))
        return AcceptedOnOtherDevice;
    if (error == QLatin1String("OutOfOrder"))
        return OutOfOrder;
    return UnknownMethod;
}

DeviceVerificationFlow *
DeviceVerificationFlow::createFromMatrixSession(QObject *parent,
                                                uint64_t handleId,
                                                const komai::MatrixVerificationSession &session)
{
    auto *flow             = new DeviceVerificationFlow(parent, Type::ToDevice, session.userId, {});
    flow->backendHandleId_ = handleId;
    flow->applyMatrixSession(session);
    flow->startMatrixRefreshTimer();
    return flow;
}

#include "moc_DeviceVerificationFlow.cpp"
