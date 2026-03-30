// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMetaType>
#include <QString>
#include <cstdint>
#include <optional>
#include <vector>

#include "matrix/backend/MatrixBlockingCall.h"

namespace komai {

struct MatrixLoginIdentityProvider
{
    QString id;
    QString name;
    QString iconUrl;
    QString brand;
};

struct MatrixLoginFlows
{
    QString homeserverUrl;
    bool passwordSupported = true;
    bool ssoSupported      = false;
    bool oauthSupported    = false;
    std::vector<MatrixLoginIdentityProvider> identityProviders;
};

struct MatrixLoginResult
{
    QString userId;
    QString accessToken;
    QString deviceId;
    QString homeserverUrl;
};

struct MatrixSsoCallbackServer
{
    uint64_t listenerId = 0;
    QString callbackUrl;
};

struct MatrixSsoCallbackStatus
{
    bool ready   = false;
    bool success = false;
    QString loginToken;
    QString callbackQuery;
};

struct MatrixOauthLoginStartResult
{
    uint64_t loginId = 0;
    QString loginUrl;
};

class MatrixAuthService
{
public:
    static std::optional<MatrixSsoCallbackServer>
    startSsoCallbackServer(const QString &successHtml,
                           const QString &failureHtml,
                           uint32_t timeoutMs,
                           QString *errorOut = nullptr);

    static std::optional<MatrixSsoCallbackStatus>
    pollSsoCallbackServer(uint64_t listenerId, QString *errorOut = nullptr);

    static bool stopSsoCallbackServer(uint64_t listenerId, QString *errorOut = nullptr);

    static std::optional<MatrixLoginFlows>
    discoverLoginFlows(matrix_backend::BlockingCallContext context,
                       const QString &serverNameOrUrl,
                       bool verifyCertificates,
                       QString *errorOut = nullptr);

    static std::optional<QString> getSsoLoginUrl(matrix_backend::BlockingCallContext context,
                                                 const QString &homeserverUrl,
                                                 const QString &redirectUrl,
                                                 const QString &identityProviderId,
                                                 bool verifyCertificates,
                                                 QString *errorOut = nullptr);
    static std::optional<MatrixOauthLoginStartResult>
    startOauthLogin(matrix_backend::BlockingCallContext context,
                    const QString &profileId,
                    const QString &homeserverUrl,
                    const QString &redirectUrl,
                    const QString &userIdHint,
                    const QString &deviceId,
                    const QString &initialDeviceDisplayName,
                    bool verifyCertificates,
                    QString *errorOut = nullptr);

    static std::optional<MatrixLoginResult>
    finishOauthLogin(matrix_backend::BlockingCallContext context,
                     uint64_t loginId,
                     const QString &callbackQuery,
                     QString *errorOut = nullptr);

    static bool cancelOauthLogin(uint64_t loginId, QString *errorOut = nullptr);

    static std::optional<MatrixLoginResult>
    loginWithPassword(matrix_backend::BlockingCallContext context,
                      const QString &profileId,
                      const QString &homeserverUrl,
                      const QString &userId,
                      const QString &password,
                      const QString &deviceId,
                      const QString &initialDeviceDisplayName,
                      bool verifyCertificates,
                      QString *errorOut = nullptr);

    static std::optional<MatrixLoginResult>
    loginWithToken(matrix_backend::BlockingCallContext context,
                   const QString &profileId,
                   const QString &homeserverUrl,
                   const QString &loginToken,
                   const QString &deviceId,
                   const QString &initialDeviceDisplayName,
                   bool verifyCertificates,
                   QString *errorOut = nullptr);
};

} // namespace komai

Q_DECLARE_METATYPE(komai::MatrixLoginIdentityProvider)
Q_DECLARE_METATYPE(komai::MatrixLoginResult)
