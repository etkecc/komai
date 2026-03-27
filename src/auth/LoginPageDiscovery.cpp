// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QDesktopServices>
#include <QPointer>
#include <QUrl>

#include <atomic>
#include <memory>
#include <optional>
#include <thread>

#include <mtx/identifiers.hpp>

#include "LoginPage.h"
#include "SSOHandler.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixAuthService.h"
#include "settings/ui/facade/UserSettingsPage.h"

using namespace mtx::identifiers;

namespace {
QString
normalizeHomeserverInput(QString homeserver)
{
    homeserver = homeserver.trimmed();
    homeserver.remove(u'\r');
    homeserver.remove(u'\n');
    return homeserver;
}

QString
normalizeHomeserverUrl(QString homeserver)
{
    homeserver = normalizeHomeserverInput(homeserver);
    if (homeserver.isEmpty())
        return homeserver;

    if (!homeserver.contains("://"))
        homeserver.prepend("https://");

    QUrl url(homeserver, QUrl::TolerantMode);
    if (!url.isValid() || url.host().isEmpty())
        return homeserver;

    if (url.port() < 0)
        url.setPort(443);

    // mtxclient expects only protocol + host + port in set_server().
    url.setPath(QString());
    url.setQuery(QString());
    url.setFragment(QString());
    url.setUserName(QString());
    url.setPassword(QString());

    return url.toString(QUrl::FullyEncoded);
}

} // namespace

void
LoginPage::setHomeserver(const QString &hs)
{
    const auto normalized = normalizeHomeserverUrl(hs);
    if (normalized.isEmpty()) {
        if (homeserver_.isEmpty())
            return;

        homeserver_.clear();
        homeserverValid_ = false;
        emit homeserverChanged();
        return;
    }

    if (normalized != homeserver_) {
        homeserver_      = normalized;
        homeserverValid_ = false;
        emit homeserverChanged();
        checkHomeserverVersion();
    } else if (!homeserverValid_) {
        checkHomeserverVersion();
    }
}

void
LoginPage::onMatrixIdEntered()
{
    clearErrors();

    homeserverValid_ = false;
    emit homeserverChanged();

    User user;
    try {
        user = parse<User>(mxid_.toStdString());
    } catch (const std::exception &) {
        mxidError_ = tr("You have entered an invalid Matrix ID e.g. @user:yourserver.example.com");
        emit mxidErrorChanged();
        return;
    }

    if (user.hostname().empty() || user.localpart().empty()) {
        mxidError_ = tr("You have entered an invalid Matrix ID e.g. @user:yourserver.example.com");
        emit mxidErrorChanged();
        return;
    }

    nhlog::net()->debug("hostname: {}", user.hostname());

    homeserverNeeded_ = false;
    lookingUpHs_      = true;
    homeserver_       = normalizeHomeserverUrl(QString::fromStdString(user.hostname()));
    emit homeserverChanged();
    emit lookingUpHsChanged();

    startLoginFlowDiscovery(QString::fromStdString(user.hostname()), homeserver_);
}

QVariantList
LoginPage::buildIdentityProviders(
  const std::vector<komai::MatrixLoginIdentityProvider> &identityProviders) const
{
    QVariantList idps;

    for (const auto &idp : identityProviders) {
        SSOProvider prov;
        if (idp.brand == QStringLiteral("apple"))
            prov.name_ = tr("Sign in with Apple");
        else if (idp.brand == QStringLiteral("facebook"))
            prov.name_ = tr("Continue with Facebook");
        else if (idp.brand == QStringLiteral("google"))
            prov.name_ = tr("Sign in with Google");
        else if (idp.brand == QStringLiteral("twitter"))
            prov.name_ = tr("Sign in with Twitter");
        else
            prov.name_ = tr("Login using %1").arg(idp.name);

        prov.avatarUrl_ = idp.iconUrl;
        prov.id_        = idp.id;
        idps.push_back(QVariant::fromValue(prov));
    }

    return idps;
}

void
LoginPage::startLoginFlowDiscovery(const QString &serverNameOrUrl,
                                   const QString &expectedHomeserver)
{
    QPointer<LoginPage> guard(this);
    const bool verifyCertificates =
      UserSettings::instance()->networkTlsEnableCertificateValidation();

    std::thread([guard, serverNameOrUrl, expectedHomeserver, verifyCertificates]() {
        QString error;
        auto result =
          komai::MatrixAuthService::discoverLoginFlows(serverNameOrUrl, verifyCertificates, &error);

        QMetaObject::invokeMethod(
          QCoreApplication::instance(), [guard, expectedHomeserver, result, error]() {
              if (!guard || guard->homeserver_ != expectedHomeserver)
                  return;

              if (!result) {
                  guard->versionError(error);
                  return;
              }

              if (guard->homeserver_ != result->homeserverUrl) {
                  guard->homeserver_ = result->homeserverUrl;
                  emit guard->homeserverChanged();
              }

              auto idps = guard->buildIdentityProviders(result->identityProviders);
              if (result->ssoSupported &&
                  (result->identityProviders.empty() || result->oauthSupported)) {
                  SSOProvider provider;
                  provider.name_ =
                    result->oauthSupported ? tr("Continue in Browser") : tr("SSO LOGIN");
                  idps.push_back(QVariant::fromValue(provider));
              }

              guard->versionOk(
                result->passwordSupported, result->ssoSupported, result->oauthSupported, idps);
          });
    }).detach();
}

void
LoginPage::checkHomeserverVersion()
{
    clearErrors();

    try {
        [[maybe_unused]] auto user = parse<User>(mxid_.toStdString());
    } catch (const std::exception &) {
        mxidError_ = tr("You have entered an invalid Matrix ID e.g. @user:yourserver.example.com");
        emit mxidErrorChanged();
        return;
    }

    lookingUpHs_ = true;
    emit lookingUpHsChanged();
    startLoginFlowDiscovery(homeserver_, homeserver_);
}

void
LoginPage::versionError(const QString &error)
{
    showError(error);

    homeserverNeeded_ = true;
    lookingUpHs_      = false;
    homeserverValid_  = false;
    emit lookingUpHsChanged();
    emit versionLookedUp();
}

void
LoginPage::versionOk(bool passwordSupported,
                     bool ssoSupported,
                     bool oauthSupported,
                     QVariantList idps)
{
    passwordSupported_ = passwordSupported;
    ssoSupported_      = ssoSupported;
    oauthSupported_    = oauthSupported;
    identityProviders_ = idps;

    lookingUpHs_     = false;
    homeserverValid_ = true;
    emit lookingUpHsChanged();
    emit versionLookedUp();
}

void
LoginPage::onLoginButtonClicked(LoginMethod loginMethod,
                                const QString &userid,
                                const QString &password,
                                const QString &deviceName)
{
    clearErrors();

    User user;

    try {
        user = parse<User>(userid.toStdString());
    } catch (const std::exception &) {
        mxidError_ = tr("You have entered an invalid Matrix ID e.g. @user:yourserver.example.com");
        emit mxidErrorChanged();
        return;
    }

    if (loginMethod == LoginMethod::Password) {
        if (password.isEmpty())
            return showError(tr("Empty password"));

        const auto initialDeviceDisplayName = deviceName.trimmed().isEmpty()
                                                ? QString::fromStdString(initialDeviceName_())
                                                : deviceName;
        const auto existingDeviceId         = UserSettings::instance()->deviceId().trimmed();
        if (!existingDeviceId.isEmpty())
            nhlog::net()->info("Login reusing existing device ID: {}",
                               existingDeviceId.toStdString());

        QPointer<LoginPage> guard(this);
        const auto profileId  = UserSettings::instance()->profile();
        const auto homeserver = homeserver_;
        const bool verifyCertificates =
          UserSettings::instance()->networkTlsEnableCertificateValidation();
        std::thread([guard,
                     profileId,
                     homeserver,
                     userid,
                     password,
                     existingDeviceId,
                     initialDeviceDisplayName,
                     verifyCertificates]() {
            QString error;
            auto result = komai::MatrixAuthService::loginWithPassword(profileId,
                                                                      homeserver,
                                                                      userid,
                                                                      password,
                                                                      existingDeviceId,
                                                                      initialDeviceDisplayName,
                                                                      verifyCertificates,
                                                                      &error);

            QMetaObject::invokeMethod(QCoreApplication::instance(), [guard, result, error]() {
                if (!guard)
                    return;

                if (!result) {
                    guard->showError(error);
                    return;
                }

                emit guard->loginOk(*result);
            });
        }).detach();
    } else {
        auto sso          = new SSOHandler();
        auto oauthLoginId = std::make_shared<std::atomic<uint64_t>>(0);
        QPointer<LoginPage> guard(this);
        const auto profileId                = UserSettings::instance()->profile();
        const auto homeserver               = homeserver_;
        const auto matrixId                 = userid;
        const auto identityProviderId       = password;
        const auto callbackUrl              = sso->url();
        const auto initialDeviceDisplayName = deviceName.trimmed().isEmpty()
                                                ? QString::fromStdString(initialDeviceName_())
                                                : deviceName;
        const auto existingDeviceId         = UserSettings::instance()->deviceId().trimmed();
        if (!existingDeviceId.isEmpty())
            nhlog::net()->info("SSO login reusing existing device ID: {}",
                               existingDeviceId.toStdString());
        const bool verifyCertificates =
          UserSettings::instance()->networkTlsEnableCertificateValidation();
        const bool useOauthForGenericSso =
          oauthSupported_ && identityProviderId.trimmed().isEmpty();

        connect(sso,
                &SSOHandler::callbackSuccess,
                this,
                [this,
                 sso,
                 oauthLoginId,
                 profileId,
                 homeserver,
                 existingDeviceId,
                 initialDeviceDisplayName,
                 verifyCertificates,
                 useOauthForGenericSso](const QString &callbackQuery, const QString &loginToken) {
                    beginStartupRestoreHandoff();
                    QPointer<LoginPage> guard(this);
                    QPointer<SSOHandler> ssoGuard(sso);
                    const auto oauthFlowId = oauthLoginId->load(std::memory_order_relaxed);

                    std::thread([guard,
                                 ssoGuard,
                                 oauthFlowId,
                                 profileId,
                                 homeserver,
                                 callbackQuery,
                                 loginToken,
                                 existingDeviceId,
                                 initialDeviceDisplayName,
                                 verifyCertificates,
                                 useOauthForGenericSso]() {
                        QString error;
                        std::optional<komai::MatrixLoginResult> result;

                        if (useOauthForGenericSso) {
                            result = komai::MatrixAuthService::finishOauthLogin(
                              oauthFlowId, callbackQuery, &error);
                        } else {
                            result =
                              komai::MatrixAuthService::loginWithToken(profileId,
                                                                       homeserver,
                                                                       loginToken,
                                                                       existingDeviceId,
                                                                       initialDeviceDisplayName,
                                                                       verifyCertificates,
                                                                       &error);
                        }

                        QMetaObject::invokeMethod(QCoreApplication::instance(),
                                                  [guard, ssoGuard, result, error]() {
                                                      if (ssoGuard)
                                                          ssoGuard->deleteLater();
                                                      if (!guard)
                                                          return;

                                                      if (!result) {
                                                          guard->showError(error);
                                                          return;
                                                      }

                                                      emit guard->loginOk(*result);
                                                  });
                    }).detach();
                });
        connect(sso, &SSOHandler::ssoFailed, this, [this, sso, oauthLoginId]() {
            const auto pendingOauthLoginId = oauthLoginId->exchange(0, std::memory_order_relaxed);
            if (pendingOauthLoginId != 0) {
                QString error;
                if (!komai::MatrixAuthService::cancelOauthLogin(pendingOauthLoginId, &error) &&
                    !error.isEmpty()) {
                    nhlog::net()->warn("Failed to cancel pending OAuth login {}: {}",
                                       pendingOauthLoginId,
                                       error.toStdString());
                }
            }

            showError(tr("SSO login failed"));
            sso->deleteLater();
        });

        QPointer<SSOHandler> ssoGuard(sso);
        std::thread([guard,
                     ssoGuard,
                     oauthLoginId,
                     profileId,
                     homeserver,
                     matrixId,
                     identityProviderId,
                     callbackUrl,
                     existingDeviceId,
                     initialDeviceDisplayName,
                     verifyCertificates,
                     useOauthForGenericSso]() {
            QString error;

            if (useOauthForGenericSso) {
                auto oauthLogin =
                  komai::MatrixAuthService::startOauthLogin(profileId,
                                                            homeserver,
                                                            callbackUrl,
                                                            matrixId,
                                                            existingDeviceId,
                                                            initialDeviceDisplayName,
                                                            verifyCertificates,
                                                            &error);

                QMetaObject::invokeMethod(
                  QCoreApplication::instance(),
                  [guard, ssoGuard, oauthLoginId, oauthLogin, error]() {
                      if (!guard)
                          return;

                      if (!oauthLogin) {
                          if (ssoGuard)
                              ssoGuard->deleteLater();
                          guard->showError(error);
                          return;
                      }

                      oauthLoginId->store(oauthLogin->loginId, std::memory_order_relaxed);

                      if (!QDesktopServices::openUrl(QUrl(oauthLogin->loginUrl))) {
                          QString cancelError;
                          komai::MatrixAuthService::cancelOauthLogin(oauthLogin->loginId,
                                                                     &cancelError);
                          oauthLoginId->store(0, std::memory_order_relaxed);
                          if (ssoGuard)
                              ssoGuard->deleteLater();
                          guard->showError(tr("Failed to open the browser sign-in page."));
                      }
                  });

                return;
            }

            auto ssoUrl = komai::MatrixAuthService::getSsoLoginUrl(
              homeserver, callbackUrl, identityProviderId, verifyCertificates, &error);

            QMetaObject::invokeMethod(
              QCoreApplication::instance(), [guard, ssoGuard, ssoUrl, error]() {
                  if (!guard)
                      return;

                  if (!ssoUrl) {
                      if (ssoGuard)
                          ssoGuard->deleteLater();
                      guard->showError(error);
                      return;
                  }

                  if (!QDesktopServices::openUrl(QUrl(*ssoUrl))) {
                      if (ssoGuard)
                          ssoGuard->deleteLater();
                      guard->showError(tr("Failed to open the SSO login page."));
                  }
              });
        }).detach();
    }

    loggingIn_ = true;
    emit loggingInChanged();
}
