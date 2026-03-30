// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixAuthService.h"

#include "komai-rust-cxxbridge/lib.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"
#include "profile/ProfileId.h"

#include <QUrl>

#include <stdexcept>

namespace komai {

namespace {

QString
normalizeProfileId(QStringView profileId)
{
    return profile_id::normalized(profileId);
}

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
      .ready         = status.ready,
      .success       = status.success,
      .loginToken    = QString::fromStdString(std::string(status.login_token)),
      .callbackQuery = QString::fromStdString(std::string(status.callback_query)),
    };
}

MatrixOauthLoginStartResult
fromRustOauthLoginStartResult(const ::komai::rust::MatrixOauthLoginStartResult &result)
{
    return MatrixOauthLoginStartResult{
      .loginId  = result.login_id,
      .loginUrl = QString::fromStdString(std::string(result.login_url)),
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
    result.oauthSupported    = flows.oauth_supported;

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
MatrixAuthService::discoverLoginFlows(matrix_backend::BlockingCallContext context,
                                      const QString &serverNameOrUrl,
                                      bool verifyCertificates,
                                      QString *errorOut)
{
    try {
        auto result =
          ::komai::rust::matrix_discover_login_flows(matrix_backend::toRustBlockingContext(context),
                                                     serverNameOrUrl.toStdString(),
                                                     verifyCertificates);
        return fromRustFlows(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QString>
MatrixAuthService::getSsoLoginUrl(matrix_backend::BlockingCallContext context,
                                  const QString &homeserverUrl,
                                  const QString &redirectUrl,
                                  const QString &identityProviderId,
                                  bool verifyCertificates,
                                  QString *errorOut)
{
    try {
        auto result =
          ::komai::rust::matrix_get_sso_login_url(matrix_backend::toRustBlockingContext(context),
                                                  homeserverUrl.toStdString(),
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

std::optional<MatrixOauthLoginStartResult>
MatrixAuthService::startOauthLogin(matrix_backend::BlockingCallContext context,
                                   const QString &profileId,
                                   const QString &homeserverUrl,
                                   const QString &redirectUrl,
                                   const QString &userIdHint,
                                   const QString &deviceId,
                                   const QString &initialDeviceDisplayName,
                                   bool verifyCertificates,
                                   QString *errorOut)
{
    try {
        const auto normalizedProfileId = normalizeProfileId(profileId);
        auto result =
          ::komai::rust::matrix_start_oauth_login(matrix_backend::toRustBlockingContext(context),
                                                  normalizedProfileId.toStdString(),
                                                  homeserverUrl.toStdString(),
                                                  redirectUrl.toStdString(),
                                                  userIdHint.toStdString(),
                                                  deviceId.toStdString(),
                                                  initialDeviceDisplayName.toStdString(),
                                                  verifyCertificates);
        return fromRustOauthLoginStartResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixLoginResult>
MatrixAuthService::finishOauthLogin(matrix_backend::BlockingCallContext context,
                                    uint64_t loginId,
                                    const QString &callbackQuery,
                                    QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_finish_oauth_login(
          matrix_backend::toRustBlockingContext(context), loginId, callbackQuery.toStdString());
        return fromRustResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixAuthService::cancelOauthLogin(uint64_t loginId, QString *errorOut)
{
    try {
        ::komai::rust::matrix_cancel_oauth_login(loginId);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixLoginResult>
MatrixAuthService::loginWithPassword(matrix_backend::BlockingCallContext context,
                                     const QString &profileId,
                                     const QString &homeserverUrl,
                                     const QString &userId,
                                     const QString &password,
                                     const QString &deviceId,
                                     const QString &initialDeviceDisplayName,
                                     bool verifyCertificates,
                                     QString *errorOut)
{
    try {
        const auto normalizedProfileId = normalizeProfileId(profileId);
        auto result =
          ::komai::rust::matrix_login_password(matrix_backend::toRustBlockingContext(context),
                                               normalizedProfileId.toStdString(),
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
MatrixAuthService::loginWithToken(matrix_backend::BlockingCallContext context,
                                  const QString &profileId,
                                  const QString &homeserverUrl,
                                  const QString &loginToken,
                                  const QString &deviceId,
                                  const QString &initialDeviceDisplayName,
                                  bool verifyCertificates,
                                  QString *errorOut)
{
    try {
        const auto normalizedProfileId = normalizeProfileId(profileId);
        auto result =
          ::komai::rust::matrix_login_token(matrix_backend::toRustBlockingContext(context),
                                            normalizedProfileId.toStdString(),
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
