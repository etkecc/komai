// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMetaType>
#include <QString>
#include <optional>
#include <vector>

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
    std::vector<MatrixLoginIdentityProvider> identityProviders;
};

struct MatrixLoginResult
{
    QString userId;
    QString accessToken;
    QString deviceId;
    QString homeserverUrl;
};

class MatrixAuthService
{
public:
    static std::optional<MatrixLoginFlows> discoverLoginFlows(const QString &serverNameOrUrl,
                                                              bool verifyCertificates,
                                                              QString *errorOut = nullptr);

    static std::optional<QString> getSsoLoginUrl(const QString &homeserverUrl,
                                                 const QString &redirectUrl,
                                                 const QString &identityProviderId,
                                                 bool verifyCertificates,
                                                 QString *errorOut = nullptr);

    static std::optional<MatrixLoginResult>
    loginWithPassword(const QString &profileId,
                      const QString &homeserverUrl,
                      const QString &userId,
                      const QString &password,
                      const QString &deviceId,
                      const QString &initialDeviceDisplayName,
                      bool verifyCertificates,
                      QString *errorOut = nullptr);

    static std::optional<MatrixLoginResult> loginWithToken(const QString &profileId,
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
