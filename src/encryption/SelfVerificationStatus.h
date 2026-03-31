// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class SelfVerificationStatus : public QObject
{
    Q_OBJECT

    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool hasSSSS READ hasSSSS NOTIFY hasSSSSChanged)
    Q_PROPERTY(bool canVerifyWithAnotherDevice READ canVerifyWithAnotherDevice NOTIFY
                 canVerifyWithAnotherDeviceChanged)

public:
    SelfVerificationStatus(QObject *o = nullptr);
    enum Status
    {
        AllVerified,
        NoMasterKey,
        UnverifiedMasterKey,
        UnverifiedDevices,
    };
    Q_ENUM(Status)

    Q_INVOKABLE void
    setupCrosssigning(bool useSSSS, const QString &password, bool encryptionBackupOnlineEnabled);
    Q_INVOKABLE bool verifyMasterKey();
    Q_INVOKABLE void verifyMasterKeyWithPassphrase();
    Q_INVOKABLE void submitUnlockKeyBackup(const QString &keyOrPassphrase);
    Q_INVOKABLE void cancelUnlockKeyBackup();
    Q_INVOKABLE void verifyUnverifiedDevices();
    Q_INVOKABLE void setupEncryptionBackup();
    Q_INVOKABLE void resetEncryptionIdentity();
    Q_INVOKABLE void submitResetEncryptionIdentityPassword(const QString &password);
    Q_INVOKABLE void continueResetEncryptionIdentityAfterApproval();
    Q_INVOKABLE void cancelResetEncryptionIdentity();
    Q_INVOKABLE void promptCurrentVerificationAction();

    Status status() const { return status_; }
    bool hasSSSS() const { return hasSSSS_; }
    bool canVerifyWithAnotherDevice() const { return canVerifyWithAnotherDevice_; }

signals:
    void statusChanged();
    void hasSSSSChanged();
    void canVerifyWithAnotherDeviceChanged();
    void setupCompleted();
    void showRecoveryKey(QString key);
    void setupFailed(QString message);
    void promptForStatus(int status);
    void promptUnlockKeyBackup();
    void promptResetEncryptionIdentityPassword();
    void promptResetEncryptionIdentityApproval(QString approvalUrl);
    void unlockKeyBackupCompleted();
    void resetEncryptionIdentityCompleted();

public slots:
    void invalidate();

private:
    void scheduleRuntimeStateRefresh();
    void refreshStateFromMatrixRuntime();
    void applyRuntimeStatus(Status status, bool hasSSSS, bool canVerifyWithAnotherDevice);

    Status status_                        = AllVerified;
    bool hasSSSS_                         = false;
    bool canVerifyWithAnotherDevice_      = false;
    bool runtimeStateRefreshInFlight_     = false;
    bool runtimeStateRefreshPending_      = false;
    bool promptCurrentActionAfterRefresh_ = false;
};
