// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDesktopServices>
#include <QUrl>

#include <set>

#include <mtx/identifiers.hpp>
#include <mtx/requests.hpp>
#include <mtx/responses/login.hpp>
#include <mtx/responses/version.hpp>

#include "Logging.h"
#include "LoginPage.h"
#include "MatrixClient.h"
#include "SSOHandler.h"
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

QString
requestErrorDetails(const mtx::http::ClientError &err)
{
    if (!err.matrix_error.error.empty())
        return QString::fromStdString(err.matrix_error.error);

    if (!err.parse_error.empty())
        return QString::fromStdString(err.parse_error);

    if (err.error_code != 0)
        return QString::fromLatin1(err.error_code_string());

    if (err.status_code != 0)
        return QStringLiteral("HTTP %1").arg(err.status_code);

    return {};
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

    http::client()->set_server(normalized.toStdString());
    const auto effectiveHomeserver = QString::fromStdString(http::client()->server_url());

    if (effectiveHomeserver != homeserver_) {
        homeserver_      = effectiveHomeserver;
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
    } else {
        nhlog::net()->debug("hostname: {}", user.hostname());
    }

    if (user.hostname() != inferredServerAddress_.toStdString()) {
        homeserverNeeded_ = false;
        lookingUpHs_      = true;
        emit lookingUpHsChanged();

        http::client()->set_server(
          normalizeHomeserverUrl(QString::fromStdString(user.hostname())).toStdString());
        http::client()->verify_certificates(
          UserSettings::instance()->networkTlsEnableCertificateValidation());
        homeserver_ = QString::fromStdString(http::client()->server_url());
        emit homeserverChanged();

        http::client()->well_known(
          [this, orginal_hostname = user.hostname()](const mtx::responses::WellKnown &res,
                                                     mtx::http::RequestErr err) {
              // Ignore if server changed
              auto currentUser = parse<User>(mxid_.toStdString());
              if (currentUser.hostname() != orginal_hostname)
                  return;

              if (err) {
                  if (err->status_code == 404) {
                      nhlog::net()->info("Autodiscovery: No .well-known.");
                      checkHomeserverVersion();
                      return;
                  }

                  if (!err->parse_error.empty()) {
                      emit versionErrorCb(tr("Autodiscovery failed. Received malformed response."));
                      nhlog::net()->error("Autodiscovery failed. Received malformed response. {}",
                                          err->parse_error);
                      return;
                  }

                  const auto details = requestErrorDetails(*err);
                  if (details.isEmpty())
                      emit versionErrorCb(
                        tr("Autodiscovery failed. Unknown error when requesting .well-known."));
                  else
                      emit versionErrorCb(
                        tr("Autodiscovery failed while requesting .well-known: %1").arg(details));
                  nhlog::net()->error("Autodiscovery failed. Unknown error when "
                                      "requesting .well-known. {}",
                                      *err);
                  return;
              }

              nhlog::net()->info("Autodiscovery: Discovered '" + res.homeserver.base_url + "'");
              http::client()->set_server(
                normalizeHomeserverUrl(QString::fromStdString(res.homeserver.base_url))
                  .toStdString());
              homeserver_ = QString::fromStdString(http::client()->server_url());
              emit homeserverChanged();
              checkHomeserverVersion();
          });
    }
}

void
LoginPage::checkHomeserverVersion()
{
    clearErrors();

    try {
        User user = parse<User>(mxid_.toStdString());
    } catch (const std::exception &) {
        mxidError_ = tr("You have entered an invalid Matrix ID e.g. @user:yourserver.example.com");
        emit mxidErrorChanged();
        return;
    }

    http::client()->versions([this](const mtx::responses::Versions &versions,
                                    mtx::http::RequestErr err) {
        if (err) {
            if (err->status_code == 404) {
                emit versionErrorCb(tr("The required endpoints were not found. "
                                       "Possibly not a Matrix server."));
                return;
            }

            if (!err->parse_error.empty()) {
                emit versionErrorCb(tr("Received malformed response. Make sure "
                                       "the homeserver domain is valid."));
                return;
            }

            nhlog::net()->error("Error requesting versions: {}", *err);

            const auto details = requestErrorDetails(*err);
            if (details.isEmpty()) {
                emit versionErrorCb(
                  tr("An unknown error occured. Make sure the homeserver domain is valid."));
            } else {
                emit versionErrorCb(tr("Failed to contact the homeserver: %1").arg(details));
            }
            return;
        }

        if (std::find_if(
              versions.versions.cbegin(), versions.versions.cend(), [](const std::string &v) {
                  static const std::set<std::string_view, std::less<>> supported{
                    "v1.1",
                    "v1.2",
                    "v1.3",
                    "v1.4",
                    "v1.5",
                    "v1.6",
                    "v1.7",
                    "v1.8",
                    "v1.9",
                    "v1.10",
                  };
                  return supported.count(v) != 0;
              }) == versions.versions.cend()) {
            emit versionErrorCb(
              tr("The selected server does not support a version of the Matrix protocol, that this "
                 "client understands (%1 to %2). You can't sign in.")
                .arg(u"v1.1", u"v1.10"));
            return;
        }

        http::client()->get_login([this](mtx::responses::LoginFlows flows,
                                         mtx::http::RequestErr err) {
            if (err || flows.flows.empty())
                emit versionOkCb(true, false, {});

            QVariantList idps;
            bool ssoSupported      = false;
            bool passwordSupported = false;
            for (const auto &flow : flows.flows) {
                if (flow.type == mtx::user_interactive::auth_types::sso) {
                    ssoSupported = true;

                    for (const auto &idp : flow.identity_providers) {
                        SSOProvider prov;
                        if (idp.brand == "apple")
                            prov.name_ = tr("Sign in with Apple");
                        else if (idp.brand == "facebook")
                            prov.name_ = tr("Continue with Facebook");
                        else if (idp.brand == "google")
                            prov.name_ = tr("Sign in with Google");
                        else if (idp.brand == "twitter")
                            prov.name_ = tr("Sign in with Twitter");
                        else
                            prov.name_ = tr("Login using %1").arg(QString::fromStdString(idp.name));

                        prov.avatarUrl_ = QString::fromStdString(idp.icon);
                        prov.id_        = QString::fromStdString(idp.id);
                        idps.push_back(QVariant::fromValue(prov));
                    }

                    if (flow.identity_providers.empty()) {
                        SSOProvider prov;
                        prov.name_ = tr("SSO LOGIN");
                        idps.push_back(QVariant::fromValue(prov));
                    }
                } else if (flow.type == mtx::user_interactive::auth_types::password) {
                    passwordSupported = true;
                }
            }
            emit versionOkCb(passwordSupported, ssoSupported, idps);
        });
    });
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
LoginPage::versionOk(bool passwordSupported, bool ssoSupported, QVariantList idps)
{
    passwordSupported_ = passwordSupported;
    ssoSupported_      = ssoSupported;
    identityProviders_ = idps;

    lookingUpHs_     = false;
    homeserverValid_ = true;
    emit homeserverChanged();
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

        mtx::requests::Login req{};
        req.identifier = mtx::requests::login_identifier::User{user.localpart()};
        req.password   = password.toStdString();
        req.initial_device_display_name =
          deviceName.trimmed().isEmpty() ? initialDeviceName_() : deviceName.toStdString();

        const auto existingDeviceId = UserSettings::instance()->deviceId().trimmed();
        if (!existingDeviceId.isEmpty()) {
            req.device_id = existingDeviceId.toStdString();
            nhlog::net()->info("Login reusing existing device ID: {}", req.device_id);
        }

        http::client()->login(
          req, [this](const mtx::responses::Login &res, mtx::http::RequestErr err) {
              if (err) {
                  auto error = err->matrix_error.error;
                  if (error.empty())
                      error = err->parse_error;

                  showError(QString::fromStdString(error));
                  return;
              }

              if (res.well_known) {
                  http::client()->set_server(
                    normalizeHomeserverUrl(
                      QString::fromStdString(res.well_known->homeserver.base_url))
                      .toStdString());
                  nhlog::net()->info("Login requested to use server: " +
                                     res.well_known->homeserver.base_url);
              }

              emit loginOk(res);
          });
    } else {
        auto sso = new SSOHandler();
        connect(
          sso,
          &SSOHandler::ssoSuccess,
          this,
          [this, sso, userid, deviceName](const std::string &token) {
              mtx::requests::Login req{};
              req.token = token;
              req.type  = mtx::user_interactive::auth_types::token;
              req.initial_device_display_name =
                deviceName.trimmed().isEmpty() ? initialDeviceName_() : deviceName.toStdString();

              const auto existingDeviceId = UserSettings::instance()->deviceId().trimmed();
              if (!existingDeviceId.isEmpty()) {
                  req.device_id = existingDeviceId.toStdString();
                  nhlog::net()->info("SSO login reusing existing device ID: {}", req.device_id);
              }

              http::client()->login(
                req, [this](const mtx::responses::Login &res, mtx::http::RequestErr err) {
                    if (err) {
                        showError(QString::fromStdString(err->matrix_error.error));
                        emit errorOccurred();
                        return;
                    }

                    if (res.well_known) {
                        http::client()->set_server(
                          normalizeHomeserverUrl(
                            QString::fromStdString(res.well_known->homeserver.base_url))
                            .toStdString());
                        nhlog::net()->info("Login requested to use server: " +
                                           res.well_known->homeserver.base_url);
                    }

                    emit loginOk(res);
                });
              sso->deleteLater();
          });
        connect(sso, &SSOHandler::ssoFailed, this, [this, sso]() {
            showError(tr("SSO login failed"));
            emit errorOccurred();
            sso->deleteLater();
        });

        // password doubles as the idp id for SSO login
        QDesktopServices::openUrl(QString::fromStdString(
          http::client()->login_sso_redirect(sso->url(), password.toStdString())));
    }

    loggingIn_ = true;
    emit loggingInChanged();
}
