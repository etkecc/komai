// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "matrix/backend/MatrixRegistrationService.h"
#include "utils/DeviceName.h"

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QUuid>
#include <QVariantList>

#include <cstdint>

class RegisterPage : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Registration)

    // Server list (loaded once at construction)
    Q_PROPERTY(QVariantList serverList READ serverList CONSTANT)

    // Probing state
    Q_PROPERTY(bool probing READ probing NOTIFY probingChanged)
    Q_PROPERTY(bool probed READ probed NOTIFY probedChanged)
    Q_PROPERTY(QString homeserverUrl READ homeserverUrl NOTIFY probedChanged)
    Q_PROPERTY(QStringList flowStages READ flowStages NOTIFY probedChanged)
    Q_PROPERTY(QVariantList termsPolicies READ termsPolicies NOTIFY probedChanged)

    // Username state
    Q_PROPERTY(bool checkingUsername READ checkingUsername NOTIFY checkingUsernameChanged)
    Q_PROPERTY(bool usernameAvailable READ usernameAvailable NOTIFY usernameChecked)

    // Registration state
    Q_PROPERTY(bool registering READ registering NOTIFY registeringChanged)
    Q_PROPERTY(QStringList completedStages READ completedStages NOTIFY stageCompleted)
    Q_PROPERTY(QStringList remainingStages READ remainingStages NOTIFY stageCompleted)

    // Email token state
    Q_PROPERTY(bool requestingEmail READ requestingEmail NOTIFY emailStateChanged)
    Q_PROPERTY(QString emailSid READ emailSid NOTIFY emailStateChanged)
    Q_PROPERTY(QString emailClientSecret READ emailClientSecret NOTIFY emailStateChanged)

    // Error
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit RegisterPage(QObject *parent = nullptr);
    ~RegisterPage() override;

    QVariantList serverList() const { return serverList_; }
    bool probing() const { return probing_; }
    bool probed() const { return probed_; }
    QString homeserverUrl() const { return homeserverUrl_; }
    QStringList flowStages() const { return flowStages_; }
    QVariantList termsPolicies() const { return termsPolicies_; }
    bool checkingUsername() const { return checkingUsername_; }
    bool usernameAvailable() const { return usernameAvailable_; }
    bool registering() const { return registering_; }
    QStringList completedStages() const { return completedStages_; }
    QStringList remainingStages() const { return remainingStages_; }
    bool requestingEmail() const { return requestingEmail_; }
    QString emailSid() const { return emailSid_; }
    QString emailClientSecret() const { return clientSecret_; }
    QString error() const { return error_; }

    Q_INVOKABLE void probeServer(const QString &serverNameOrUrl);
    Q_INVOKABLE void checkUsername(const QString &username);
    Q_INVOKABLE void submitStage(const QString &username,
                                 const QString &password,
                                 const QString &deviceName,
                                 const QString &stageType,
                                 const QString &token,
                                 const QString &emailSid,
                                 const QString &emailClientSecret);
    Q_INVOKABLE void requestEmailToken(const QString &email);
    Q_INVOKABLE void cancelRegistration();
    Q_INVOKABLE void reset();

    Q_INVOKABLE QString generateClientSecret() const
    {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    Q_INVOKABLE QString initialDeviceName() const
    {
        return QString::fromStdString(komai::device_name::initial());
    }

    Q_INVOKABLE QString deviceNameOS() const { return komai::device_name::os(); }
    Q_INVOKABLE QString deviceNameHostname() const { return komai::device_name::hostname(); }
    Q_INVOKABLE QString deviceNameRandom() const { return komai::device_name::random(); }
    Q_INVOKABLE QString deviceNameRandomMax() const { return komai::device_name::randomMax(); }

signals:
    void probingChanged();
    void probedChanged();
    void checkingUsernameChanged();
    void usernameChecked();
    void registeringChanged();
    void stageCompleted();
    void emailStateChanged();
    void errorChanged();
    void registrationComplete(const QString &userId,
                              const QString &accessToken,
                              const QString &deviceId,
                              const QString &homeserverUrl);

private:
    void showError(const QString &msg);
    void clearError();
    void performPostRegistrationLogin(const QString &userId,
                                      const QString &deviceId,
                                      const QString &homeserverUrl);

    QVariantList serverList_;

    // Probe state
    bool probing_            = false;
    bool probed_             = false;
    uint64_t registrationId_ = 0;
    QString homeserverUrl_;
    QStringList flowStages_;
    QVariantList termsPolicies_;

    // Username state
    bool checkingUsername_  = false;
    bool usernameAvailable_ = false;

    // Registration state
    bool registering_ = false;
    QStringList completedStages_;
    QStringList remainingStages_;

    // Credentials used during stage submission (kept for post-registration login)
    QString lastUsername_;
    QString lastPassword_;
    QString lastDeviceName_;

    // Email state
    bool requestingEmail_ = false;
    QString emailSid_;
    QString clientSecret_;
    uint64_t sendAttempt_ = 0;

    // Error
    QString error_;
};
