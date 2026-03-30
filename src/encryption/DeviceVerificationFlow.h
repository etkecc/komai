// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <cstdint>
#include <string>
#include <vector>

// clang-format off
/*
 * Stolen from fluffy chat :D
 *
 *      State         |   +-------------+                    +-----------+                                  |
 *                    |   | AliceDevice |                    | BobDevice |                                  |
 *                    |   | (sender)    |                    |           |                                  |
 *                    |   +-------------+                    +-----------+                                  |
 * promptStartVerify  |         |                                 |                                         |
 *                    |      o  | (m.key.verification.request)    |                                         |
 *                    |      p  |-------------------------------->| (ASK FOR VERIFICATION REQUEST)          |
 * waitForOtherAccept |      t  |                                 |                                         | promptStartVerify
 * &&                 |      i  |      (m.key.verification.ready) |                                         |
 * no commitment      |      o  |<--------------------------------|                                         |
 * &&                 |      n  |                                 |                                         |
 * no canonical_json  |      a  |      (m.key.verification.start) |                                         | waitingForKeys
 *                    |      l  |<--------------------------------| Not sending to prevent the glare resolve| && no commitment
 *                    |         |                                 |                               (1)       | && no canonical_json
 *                    |         | m.key.verification.start        |                                         |
 * waitForOtherAccept |         |-------------------------------->| (IF NOT ALREADY ASKED,                  |
 * &&                 |         |                                 |  ASK FOR VERIFICATION REQUEST)          | promptStartVerify, if not accepted
 * canonical_json     |         |       m.key.verification.accept |                                         |
 *                    |         |<--------------------------------|                                         |
 * waitForOtherAccept |         |                                 |                                         | waitingForKeys
 * &&                 |         | m.key.verification.key          |                                         | && canonical_json
 * commitment         |         |-------------------------------->|                                         | && commitment
 *                    |         |                                 |                                         |
 *                    |         |          m.key.verification.key |                                         |
 *                    |         |<--------------------------------|                                         |
 * compareEmoji/Number|         |                                 |                                         | compareEmoji/Number
 *                    |         |     COMPARE EMOJI / NUMBERS     |                                         |
 *                    |         |                                 |                                         |
 * waitingForMac      |         |     m.key.verification.mac      |                                         | waitingForMac
 *                    | success |<------------------------------->|  success                                |
 *                    |         |                                 |                                         |
 * success/fail       |         |         m.key.verification.done |                                         | success/fail
 *                    |         |<------------------------------->|                                         |
 *
 *  (1) Sometimes the other side does send this start. In this case we run the glare algorithm and send an accept only if
 *      We are the bigger mxid and deviceid (since we discard our start message). <- GLARE RESOLUTION
 */
// clang-format on
class DeviceVerificationFlow final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString userId READ getUserId NOTIFY detailsChanged)
    Q_PROPERTY(QString deviceId READ getDeviceId NOTIFY detailsChanged)
    Q_PROPERTY(bool sender READ getSender NOTIFY detailsChanged)
    Q_PROPERTY(std::vector<int> sasList READ getSasList NOTIFY detailsChanged)
    Q_PROPERTY(bool isDeviceVerification READ isDeviceVerification CONSTANT)
    Q_PROPERTY(bool isSelfVerification READ isSelfVerification NOTIFY detailsChanged)
    Q_PROPERTY(bool isMultiDeviceVerification READ isMultiDeviceVerification NOTIFY detailsChanged)

public:
    enum State
    {
        PromptStartVerification,
        WaitingForOtherToAccept,
        WaitingForKeys,
        CompareEmoji,
        CompareNumber,
        WaitingForMac,
        Success,
        Failed,
    };
    Q_ENUM(State)

    enum Type
    {
        ToDevice,
        RoomMsg
    };

    enum Error
    {
        UnknownMethod,
        MismatchedCommitment,
        MismatchedSAS,
        KeyMismatch,
        Timeout,
        User,
        AcceptedOnOtherDevice,
        OutOfOrder,
    };
    Q_ENUM(Error)

    static DeviceVerificationFlow *
    InitiateUserVerification(QObject *parent_, QObject *timelineContext_, const QString &userid);
    static DeviceVerificationFlow *InitiateDeviceVerification(QObject *parent,
                                                              const QString &userid,
                                                              const std::vector<QString> &devices);
    static DeviceVerificationFlow *
    InitiateMatrixSelfVerification(QObject *parent, uint64_t handleId, QString *errorOut = nullptr);
    static DeviceVerificationFlow *InitiateMatrixVerificationSession(QObject *parent,
                                                                     uint64_t handleId,
                                                                     const QString &flowId,
                                                                     QString *errorOut = nullptr);

    // getters
    QString state();
    Error error() { return error_; }
    QString getUserId();
    QString getDeviceId();
    bool getSender();
    std::vector<int> getSasList();
    QString transactionId() { return QString::fromStdString(this->transaction_id); }
    // setters
    void setDeviceId(QString deviceID);
    void setEventId(const std::string &event_id);
    bool isDeviceVerification() const
    {
        return this->type == DeviceVerificationFlow::Type::ToDevice;
    }
    bool isSelfVerification() const;
    bool isMultiDeviceVerification() const { return isMultiDeviceVerification_; }

public slots:
    //! unverifies a device
    void unverify();
    //! Continues the flow
    void next();
    //! Cancel the flow
    void cancel();

signals:
    void refreshProfile();
    void stateChanged();
    void errorChanged();
    void detailsChanged();

private:
    DeviceVerificationFlow(QObject *,
                           DeviceVerificationFlow::Type flow_type,
                           const QString &userID,
                           const std::vector<QString> &deviceIds_);
    void setState(State state)
    {
        if (state != state_) {
            state_ = state;
            emit stateChanged();
        }
    }

    //! cancels a verification flow
    void cancelVerification(DeviceVerificationFlow::Error error_code);
    void failUnavailable();
    void refreshFromMatrixRuntime();
    void startMatrixRefreshTimer();
    void applyMatrixSession(const QString &flowId,
                            const QString &deviceId,
                            const QString &state,
                            const QString &error,
                            bool sender,
                            bool isSelfVerification,
                            bool isMultiDeviceVerification,
                            const std::vector<int> &sasList);
    Error mapMatrixError(const QString &error) const;

    std::string transaction_id;
    uint64_t backendHandleId_ = 0;
    bool pendingAutoStart_    = false;

    bool sender;
    Type type;
    QString userId_;
    QString deviceId;
    std::vector<QString> deviceIds;
    bool isSelfVerification_        = false;
    bool isMultiDeviceVerification_ = false;

    std::vector<int> sasList;

    State state_ = PromptStartVerification;
    Error error_ = UnknownMethod;
};
