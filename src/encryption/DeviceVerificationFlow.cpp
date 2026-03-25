// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DeviceVerificationFlow.h"

#include "logging/Logging.h"

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
    failNotMigrated();
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
    return false;
}

void
DeviceVerificationFlow::setEventId(const std::string &event_id_)
{
    transaction_id = event_id_;
}

void
DeviceVerificationFlow::unverify()
{
    failNotMigrated();
}

void
DeviceVerificationFlow::cancelVerification(DeviceVerificationFlow::Error error_code)
{
    error_ = error_code;
    setState(Failed);
    emit errorChanged();
}

void
DeviceVerificationFlow::failNotMigrated()
{
    nhlog::crypto()->warn("Device verification is not migrated to matrix-sdk yet");
    cancelVerification(UnknownMethod);
    emit refreshProfile();
}

QSharedPointer<DeviceVerificationFlow>
DeviceVerificationFlow::NewInRoomVerification(QObject *,
                                              TimelineModel *,
                                              const mtx::events::msg::KeyVerificationRequest &,
                                              const QString &,
                                              const QString &)
{
    nhlog::crypto()->warn("Ignoring in-room verification request until matrix-sdk verification "
                          "is implemented");
    return {};
}

QSharedPointer<DeviceVerificationFlow>
DeviceVerificationFlow::NewToDeviceVerification(QObject *,
                                                const mtx::events::msg::KeyVerificationRequest &,
                                                const QString &,
                                                const QString &)
{
    nhlog::crypto()->warn("Ignoring to-device verification request until matrix-sdk verification "
                          "is implemented");
    return {};
}

QSharedPointer<DeviceVerificationFlow>
DeviceVerificationFlow::NewToDeviceVerification(QObject *,
                                                const mtx::events::msg::KeyVerificationStart &,
                                                const QString &,
                                                const QString &)
{
    nhlog::crypto()->warn("Ignoring verification start until matrix-sdk verification is "
                          "implemented");
    return {};
}

QSharedPointer<DeviceVerificationFlow>
DeviceVerificationFlow::InitiateUserVerification(QObject *parent,
                                                 TimelineModel *,
                                                 const QString &userid)
{
    auto flow = QSharedPointer<DeviceVerificationFlow>(
      new DeviceVerificationFlow(parent, Type::RoomMsg, userid, {}));
    flow->failNotMigrated();
    return flow;
}

QSharedPointer<DeviceVerificationFlow>
DeviceVerificationFlow::InitiateDeviceVerification(QObject *parent,
                                                   const QString &userid,
                                                   const std::vector<QString> &devices)
{
    auto flow = QSharedPointer<DeviceVerificationFlow>(
      new DeviceVerificationFlow(parent, Type::ToDevice, userid, devices));
    flow->failNotMigrated();
    return flow;
}

#include "moc_DeviceVerificationFlow.cpp"
