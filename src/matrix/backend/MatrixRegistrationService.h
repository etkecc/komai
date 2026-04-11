// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <optional>
#include <vector>

#include "matrix/backend/MatrixBlockingCall.h"

namespace komai {

struct ServerListEntry
{
    QString name;
    QString clientDomain;
    QString description;
    QString homepage;
    bool usingVanillaReg = true;
    QStringList languages;
    QString software;
    QString staffJur;
    QString rules;
    QString privacy;
    bool captcha = false;
    bool email   = false;
    QStringList features;
    bool slidingSync = false;
    QString regLink;
    QString regNote;
    int rank = 500;
    QString category;
    QString editorial;
    bool featured = false;
};

struct RegistrationTermsPolicy
{
    QString id;
    QString version;
    QString name;
    QString url;
};

struct RegistrationProbeResult
{
    uint64_t registrationId = 0;
    QString homeserverUrl;
    QString session;
    QStringList chosenFlowStages;
    std::vector<QStringList> allFlows;
    std::vector<RegistrationTermsPolicy> termsPolicies;
};

struct RegistrationSubmitResult
{
    bool completed = false;
    QString userId;
    QString accessToken;
    QString deviceId;
    QString homeserverUrl;
    QString session;
    QStringList remainingStages;
    QStringList completedStages;
    std::vector<RegistrationTermsPolicy> termsPolicies;
};

struct RegistrationEmailTokenResult
{
    QString sid;
};

class MatrixRegistrationService
{
public:
    static std::vector<ServerListEntry> loadServerList();

    static std::optional<RegistrationProbeResult>
    probeRegistration(matrix_backend::BlockingCallContext context,
                      const QString &serverNameOrUrl,
                      bool verifyCertificates,
                      QString *errorOut = nullptr);

    static std::optional<bool> checkUsernameAvailable(matrix_backend::BlockingCallContext context,
                                                      uint64_t registrationId,
                                                      const QString &username,
                                                      QString *errorOut = nullptr);

    static std::optional<RegistrationSubmitResult>
    submitStage(matrix_backend::BlockingCallContext context,
                uint64_t registrationId,
                const QString &username,
                const QString &password,
                const QString &deviceName,
                const QString &stageType,
                const QString &token,
                const QString &emailSid,
                const QString &emailClientSecret,
                QString *errorOut = nullptr);

    static std::optional<RegistrationEmailTokenResult>
    requestEmailToken(matrix_backend::BlockingCallContext context,
                      uint64_t registrationId,
                      const QString &email,
                      const QString &clientSecret,
                      uint64_t sendAttempt,
                      QString *errorOut = nullptr);

    static bool cancelRegistration(uint64_t registrationId, QString *errorOut = nullptr);
};

} // namespace komai

Q_DECLARE_METATYPE(komai::ServerListEntry)
Q_DECLARE_METATYPE(komai::RegistrationTermsPolicy)
Q_DECLARE_METATYPE(komai::RegistrationProbeResult)
Q_DECLARE_METATYPE(komai::RegistrationSubmitResult)
