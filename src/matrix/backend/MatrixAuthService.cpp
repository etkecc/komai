// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixAuthService.h"

#include "komai-rust-cxxbridge/lib.h"

#include <stdexcept>

namespace komai {

namespace {

MatrixLoginResult
fromRustResult(const ::komai::rust::MatrixLoginResult &result)
{
    return MatrixLoginResult{
      .userId        = QString::fromStdString(std::string(result.user_id)),
      .accessToken   = QString::fromStdString(std::string(result.access_token)),
      .deviceId      = QString::fromStdString(std::string(result.device_id)),
      .homeserverUrl = QString::fromStdString(std::string(result.homeserver_url)),
    };
}

} // namespace

std::optional<MatrixLoginResult>
MatrixAuthService::loginWithPassword(const QString &profileId,
                                     const QString &homeserverUrl,
                                     const QString &userId,
                                     const QString &password,
                                     const QString &deviceId,
                                     const QString &initialDeviceDisplayName,
                                     bool verifyCertificates,
                                     QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_login_password(profileId.toStdString(),
                                                           homeserverUrl.toStdString(),
                                                           userId.toStdString(),
                                                           password.toStdString(),
                                                           deviceId.toStdString(),
                                                           initialDeviceDisplayName.toStdString(),
                                                           verifyCertificates);
        return fromRustResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixLoginResult>
MatrixAuthService::loginWithToken(const QString &profileId,
                                  const QString &homeserverUrl,
                                  const QString &loginToken,
                                  const QString &deviceId,
                                  const QString &initialDeviceDisplayName,
                                  bool verifyCertificates,
                                  QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_login_token(profileId.toStdString(),
                                                        homeserverUrl.toStdString(),
                                                        loginToken.toStdString(),
                                                        deviceId.toStdString(),
                                                        initialDeviceDisplayName.toStdString(),
                                                        verifyCertificates);
        return fromRustResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

} // namespace komai
