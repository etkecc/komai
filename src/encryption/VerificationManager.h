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

#include "DeviceVerificationFlow.h"

class TimelineViewManager;

class VerificationManager final : public QObject
{
    Q_OBJECT

    QML_ELEMENT
    QML_SINGLETON

public:
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
    bool verifySelf(QString *errorOut = nullptr);
    bool verifyUser(QString userid, QString *errorOut = nullptr);
    bool verifyDevice(QString userid, QString deviceid, QString *errorOut = nullptr);
    bool
    verifyOneOfDevices(QString userid, std::vector<QString> deviceids, QString *errorOut = nullptr);

signals:
    void newDeviceVerificationRequest(DeviceVerificationFlow *flow);
    void verificationStateChanged(const QString &userId);

public slots:
    void pollPendingMatrixVerifications();

private:
    bool openMatrixVerificationFlow(uint64_t handleId,
                                    const QString &flowId,
                                    QString *errorOut = nullptr);

    inline static VerificationManager *instance_ = nullptr;
    QTimer *matrixVerificationPollTimer_         = nullptr;
    QSet<QString> activeMatrixFlowIds_;
};
