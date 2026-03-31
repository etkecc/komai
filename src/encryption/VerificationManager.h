// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QString>
#include <QTimer>

#include <functional>
#include <vector>

#include "DeviceVerificationFlow.h"

class TimelineViewManager;

class VerificationManager final : public QObject
{
    Q_OBJECT

    QML_ELEMENT
    QML_SINGLETON

public:
    using FailureCallback = std::function<void(const QString &)>;

    VerificationManager(TimelineViewManager *o);
    static VerificationManager *instance() { return instance_; }

    static VerificationManager *create(QQmlEngine *qmlEngine, QJSEngine *)
    {
        // The instance has to exist before it is used. We cannot replace it.
        Q_ASSERT(instance_);

        // The engine has to have the same thread affinity as the singleton.
        Q_ASSERT(qmlEngine->thread() == instance_->thread());

        // There can only be one engine accessing the singleton.
        static QJSEngine *s_engine = nullptr;
        if (s_engine)
            Q_ASSERT(qmlEngine == s_engine);
        else
            s_engine = qmlEngine;

        QJSEngine::setObjectOwnership(instance_, QJSEngine::CppOwnership);
        return instance_;
    }

    Q_INVOKABLE void removeVerificationFlow(DeviceVerificationFlow *flow);
    void verifySelf(FailureCallback onFailure = {});
    void verifyUser(QString userid, FailureCallback onFailure = {});
    void verifyDevice(QString userid, QString deviceid, FailureCallback onFailure = {});
    void verifyOneOfDevices(QString userid,
                            std::vector<QString> deviceids,
                            FailureCallback onFailure = {});

signals:
    void newDeviceVerificationRequest(DeviceVerificationFlow *flow);
    void verificationStateChanged(const QString &userId);

public slots:
    void pollPendingMatrixVerifications();

private:
    void adoptMatrixVerificationSession(uint64_t handleId,
                                        const komai::MatrixVerificationSession &session);
    void requestMatrixVerificationFlow(uint64_t handleId, const QString &flowId);

    inline static VerificationManager *instance_ = nullptr;
    QTimer *matrixVerificationPollTimer_         = nullptr;
    QSet<QString> activeMatrixFlowIds_;
    QSet<QString> openingMatrixFlowIds_;
    bool matrixVerificationPollInFlight_ = false;
    bool matrixVerificationPollPending_  = false;
};
