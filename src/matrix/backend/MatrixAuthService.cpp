// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixAuthService.h"

#include "komai-rust-cxxbridge/lib.h"

#include <QUrl>

#include <stdexcept>

namespace komai {

namespace {

QString
normalizeHomeserverUrl(QString homeserver)
{
    homeserver = homeserver.trimmed();
    homeserver.remove(u'\r');
    homeserver.remove(u'\n');

    if (homeserver.isEmpty())
        return homeserver;

    if (!homeserver.contains("://"))
        homeserver.prepend("https://");

    QUrl url(homeserver, QUrl::TolerantMode);
    if (!url.isValid() || url.host().isEmpty())
        return homeserver;

    if (url.port() < 0)
        url.setPort(443);

    url.setPath(QString());
    url.setQuery(QString());
    url.setFragment(QString());
    url.setUserName(QString());
    url.setPassword(QString());

    return url.toString(QUrl::FullyEncoded);
}

MatrixSsoCallbackServer
fromRustSsoCallbackServer(const ::komai::rust::MatrixSsoCallbackServer &server)
{
    return MatrixSsoCallbackServer{
      .listenerId  = server.listener_id,
      .callbackUrl = QString::fromStdString(std::string(server.callback_url)),
    };
}

MatrixSsoCallbackStatus
fromRustSsoCallbackStatus(const ::komai::rust::MatrixSsoCallbackStatus &status)
{
    return MatrixSsoCallbackStatus{
      .ready      = status.ready,
      .success    = status.success,
      .loginToken = QString::fromStdString(std::string(status.login_token)),
    };
}

MatrixLoginIdentityProvider
fromRustProvider(const ::komai::rust::MatrixLoginIdentityProvider &provider)
{
    return MatrixLoginIdentityProvider{
      .id      = QString::fromStdString(std::string(provider.id)),
      .name    = QString::fromStdString(std::string(provider.name)),
      .iconUrl = QString::fromStdString(std::string(provider.icon)),
      .brand   = QString::fromStdString(std::string(provider.brand)),
    };
}

MatrixLoginFlows
fromRustFlows(const ::komai::rust::MatrixLoginFlows &flows)
{
    MatrixLoginFlows result;
    result.homeserverUrl =
      normalizeHomeserverUrl(QString::fromStdString(std::string(flows.homeserver_url)));
    result.passwordSupported = flows.password_supported;
    result.ssoSupported      = flows.sso_supported;

    result.identityProviders.reserve(flows.identity_providers.size());
    for (const auto &provider : flows.identity_providers) {
        result.identityProviders.push_back(fromRustProvider(provider));
    }

    return result;
}

MatrixLoginResult
fromRustResult(const ::komai::rust::MatrixLoginResult &result)
{
    return MatrixLoginResult{
      .userId      = QString::fromStdString(std::string(result.user_id)),
      .accessToken = QString::fromStdString(std::string(result.access_token)),
      .deviceId    = QString::fromStdString(std::string(result.device_id)),
      .homeserverUrl =
        normalizeHomeserverUrl(QString::fromStdString(std::string(result.homeserver_url))),
    };
}

} // namespace

std::optional<MatrixSsoCallbackServer>
MatrixAuthService::startSsoCallbackServer(const QString &successHtml,
                                          const QString &failureHtml,
                                          uint32_t timeoutMs,
                                          QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_start_sso_callback_server(
          successHtml.toStdString(), failureHtml.toStdString(), timeoutMs);
        return fromRustSsoCallbackServer(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixSsoCallbackStatus>
MatrixAuthService::pollSsoCallbackServer(uint64_t listenerId, QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_poll_sso_callback_server(listenerId);
        return fromRustSsoCallbackStatus(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixAuthService::stopSsoCallbackServer(uint64_t listenerId, QString *errorOut)
{
    try {
        ::komai::rust::matrix_stop_sso_callback_server(listenerId);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixLoginFlows>
MatrixAuthService::discoverLoginFlows(const QString &serverNameOrUrl,
                                      bool verifyCertificates,
                                      QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_discover_login_flows(serverNameOrUrl.toStdString(),
                                                                 verifyCertificates);
        return fromRustFlows(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QString>
MatrixAuthService::getSsoLoginUrl(const QString &homeserverUrl,
                                  const QString &redirectUrl,
                                  const QString &identityProviderId,
                                  bool verifyCertificates,
                                  QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_get_sso_login_url(homeserverUrl.toStdString(),
                                                              redirectUrl.toStdString(),
                                                              identityProviderId.toStdString(),
                                                              verifyCertificates);
        return QString::fromStdString(std::string(result));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

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
