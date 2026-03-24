// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <optional>

namespace komai {

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

Q_DECLARE_METATYPE(komai::MatrixLoginResult)
