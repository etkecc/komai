// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DeviceVerificationFlow.h"

#include <QTimer>

#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"

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

    if (state_ == PromptStartVerification) {
        pendingAutoStart_ = sender && deviceId.isEmpty();
        setState(sender ? WaitingForOtherToAccept : WaitingForKeys);
    }

    const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
    QString error;
    if (!komai::MatrixBackendRuntimeService::advanceVerificationSession(
          context, backendHandleId_, transactionId(), &error)) {
        nhlog::crypto()->warn("Failed to advance matrix-sdk verification flow {}: {}",
                              transaction_id,
                              error.toStdString());
        cancelVerification(UnknownMethod);
        emit refreshProfile();
        return;
    }

    refreshFromMatrixRuntime();
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
    if (backendHandleId_ != 0 && !transaction_id.empty()) {
        const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
        QString error;
        const auto mismatch = state_ == CompareEmoji || state_ == CompareNumber;
        if (!komai::MatrixBackendRuntimeService::cancelVerificationSession(
              context, backendHandleId_, transactionId(), mismatch, &error)) {
            nhlog::crypto()->warn("Failed to cancel matrix-sdk verification flow {}: {}",
                                  transaction_id,
                                  error.toStdString());
        }
    }

    cancelVerification(User);
    emit refreshProfile();
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

    const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
    QString error;
    if (!komai::MatrixBackendRuntimeService::unverifyDevice(
          context, backendHandleId_, userId_, deviceId, &error)) {
        nhlog::crypto()->warn("Failed to clear matrix-sdk local trust for {}:{}: {}",
                              userId_.toStdString(),
                              deviceId.toStdString(),
                              error.toStdString());
        emit refreshProfile();
        return;
    }

    emit refreshProfile();
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

    const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
    QString error;
    const auto session = komai::MatrixBackendRuntimeService::fetchVerificationSession(
      context, backendHandleId_, transactionId(), &error);
    if (!session) {
        nhlog::crypto()->warn("Failed to fetch matrix-sdk verification flow {}: {}",
                              transaction_id,
                              error.toStdString());
        return;
    }

    std::vector<int> numbers;
    numbers.reserve(session->sasNumbers.size());
    for (const auto number : session->sasNumbers)
        numbers.push_back(number);

    applyMatrixSession(session->flowId,
                       session->deviceId,
                       session->state,
                       session->error,
                       session->sender,
                       session->isSelfVerification,
                       session->isMultiDeviceVerification,
                       numbers);

    if (pendingAutoStart_ && state_ == PromptStartVerification) {
        if (deviceId.isEmpty()) {
            setState(WaitingForOtherToAccept);
        } else {
            pendingAutoStart_ = false;
            QTimer::singleShot(0, this, &DeviceVerificationFlow::next);
        }
    }
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
DeviceVerificationFlow::applyMatrixSession(const QString &flowId,
                                           const QString &newDeviceId,
                                           const QString &state,
                                           const QString &error,
                                           bool isSender,
                                           bool isSelfVerification,
                                           bool isMultiDeviceVerification,
                                           const std::vector<int> &newSasList)
{
    transaction_id = flowId.toStdString();

    const bool detailsChangedNow =
      sender != isSender || deviceId != newDeviceId || isSelfVerification_ != isSelfVerification ||
      isMultiDeviceVerification_ != isMultiDeviceVerification || sasList != newSasList;

    sender                     = isSender;
    deviceId                   = newDeviceId;
    isSelfVerification_        = isSelfVerification;
    isMultiDeviceVerification_ = isMultiDeviceVerification;
    sasList                    = newSasList;

    const auto nextError = mapMatrixError(error);
    if (error_ != nextError) {
        error_ = nextError;
        emit errorChanged();
    }

    setState(matrixStateFromString(state));
    if (detailsChangedNow)
        emit detailsChanged();
    if (state_ == Success || state_ == Failed)
        emit refreshProfile();
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
DeviceVerificationFlow::InitiateUserVerification(QObject *parent, QObject *, const QString &userid)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        nhlog::crypto()->warn("Cannot start matrix-sdk user verification for '{}' without an "
                              "active runtime",
                              userid.toStdString());
        return nullptr;
    }

    const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
    QString error;
    const auto session =
      komai::MatrixBackendRuntimeService::startUserVerification(context, handleId, userid, &error);
    if (!session) {
        nhlog::crypto()->warn("Failed to start matrix-sdk user verification for '{}': {}",
                              userid.toStdString(),
                              error.toStdString());
        return nullptr;
    }

    return InitiateMatrixVerificationSession(parent, handleId, session->flowId, &error);
}

DeviceVerificationFlow *
DeviceVerificationFlow::InitiateDeviceVerification(QObject *parent,
                                                   const QString &userid,
                                                   const std::vector<QString> &devices)
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0) {
        nhlog::crypto()->warn("Cannot start matrix-sdk device verification for '{}' without an "
                              "active runtime",
                              userid.toStdString());
        return nullptr;
    }

    const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
    QString error;
    for (const auto &device : devices) {
        const auto session = komai::MatrixBackendRuntimeService::startDeviceVerification(
          context, handleId, userid, device, &error);
        if (!session)
            continue;

        if (auto *flow =
              InitiateMatrixVerificationSession(parent, handleId, session->flowId, &error)) {
            return flow;
        }
    }

    nhlog::crypto()->warn("Failed to start matrix-sdk device verification for '{}': {}",
                          userid.toStdString(),
                          error.toStdString());
    return nullptr;
}

DeviceVerificationFlow *
DeviceVerificationFlow::InitiateMatrixSelfVerification(QObject *parent,
                                                       uint64_t handleId,
                                                       QString *errorOut)
{
    const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
    QString error;
    const auto session =
      komai::MatrixBackendRuntimeService::startSelfVerification(context, handleId, &error);
    if (!session) {
        if (errorOut)
            *errorOut = error;
        return nullptr;
    }

    auto *flow = new DeviceVerificationFlow(parent, Type::ToDevice, session->userId, {});
    flow->backendHandleId_ = handleId;
    std::vector<int> numbers;
    numbers.reserve(session->sasNumbers.size());
    for (const auto number : session->sasNumbers)
        numbers.push_back(number);
    flow->applyMatrixSession(session->flowId,
                             session->deviceId,
                             session->state,
                             session->error,
                             session->sender,
                             session->isSelfVerification,
                             session->isMultiDeviceVerification,
                             numbers);
    flow->startMatrixRefreshTimer();

    return flow;
}

DeviceVerificationFlow *
DeviceVerificationFlow::InitiateMatrixVerificationSession(QObject *parent,
                                                          uint64_t handleId,
                                                          const QString &flowId,
                                                          QString *errorOut)
{
    const auto context = komai::matrix_backend::allowUiThreadBlockingCallContext();
    QString error;
    const auto session = komai::MatrixBackendRuntimeService::fetchVerificationSession(
      context, handleId, flowId, &error);
    if (!session) {
        if (errorOut)
            *errorOut = error;
        return nullptr;
    }

    auto *flow = new DeviceVerificationFlow(parent, Type::ToDevice, session->userId, {});
    flow->backendHandleId_ = handleId;
    std::vector<int> numbers;
    numbers.reserve(session->sasNumbers.size());
    for (const auto number : session->sasNumbers)
        numbers.push_back(number);
    flow->applyMatrixSession(session->flowId,
                             session->deviceId,
                             session->state,
                             session->error,
                             session->sender,
                             session->isSelfVerification,
                             session->isMultiDeviceVerification,
                             numbers);
    flow->startMatrixRefreshTimer();

    return flow;
}

#include "moc_DeviceVerificationFlow.cpp"
