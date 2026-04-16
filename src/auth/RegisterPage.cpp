// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RegisterPage.h"

#include "logging/Logging.h"
#include "matrix/backend/MatrixAuthService.h"
#include "matrix/backend/MatrixRegistrationService.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/StateEventText.h"
#include "ui/MainWindow.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QPointer>
#include <QUrl>

#include <thread>

RegisterPage::RegisterPage(QObject *parent)
  : QObject(parent)
{
    // Load server list from embedded data (synchronous, no network)
    auto entries = komai::MatrixRegistrationService::loadServerList();
    for (const auto &entry : entries) {
        QVariantMap m;
        m[QStringLiteral("name")]            = entry.name;
        m[QStringLiteral("clientDomain")]    = entry.clientDomain;
        m[QStringLiteral("description")]     = entry.description;
        m[QStringLiteral("homepage")]        = entry.homepage;
        m[QStringLiteral("usingVanillaReg")] = entry.usingVanillaReg;
        m[QStringLiteral("languages")]       = entry.languages;
        m[QStringLiteral("software")]        = entry.software;
        m[QStringLiteral("staffJur")]        = entry.staffJur;
        m[QStringLiteral("captcha")]         = entry.captcha;
        m[QStringLiteral("email")]           = entry.email;
        m[QStringLiteral("features")]        = entry.features;
        m[QStringLiteral("regLink")]         = entry.regLink;
        m[QStringLiteral("rank")]            = entry.rank;
        m[QStringLiteral("category")]        = entry.category;
        m[QStringLiteral("editorial")]       = entry.editorial;
        m[QStringLiteral("featured")]        = entry.featured;
        serverList_.append(m);
    }

    connect(
      this,
      &RegisterPage::registrationComplete,
      this,
      [this](const QString &userId,
             const QString & /*accessToken*/,
             const QString &deviceId,
             const QString &homeserverUrl) {
          // Registration created the account — now perform a proper login to set up the
          // matrix-sdk session with a state store. The registration's access_token came
          // from a stateless discovery Client, so we can't use it directly.
          performPostRegistrationLogin(userId, deviceId, homeserverUrl);
      },
      Qt::QueuedConnection);
}

RegisterPage::~RegisterPage()
{
    if (registrationId_ != 0) {
        komai::MatrixRegistrationService::cancelRegistration(registrationId_);
        registrationId_ = 0;
    }
}

void
RegisterPage::probeServer(const QString &serverNameOrUrl)
{
    clearError();

    if (registrationId_ != 0) {
        komai::MatrixRegistrationService::cancelRegistration(registrationId_);
        registrationId_ = 0;
    }

    probing_ = true;
    probed_  = false;
    emit probingChanged();
    emit probedChanged();

    QPointer<RegisterPage> guard(this);
    const bool verifyCertificates =
      UserSettings::instance()->networkTlsEnableCertificateValidation();
    const auto server = serverNameOrUrl.trimmed();

    std::thread([guard, server, verifyCertificates]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        auto result = komai::MatrixRegistrationService::probeRegistration(
          context, server, verifyCertificates, &error);

        QMetaObject::invokeMethod(QCoreApplication::instance(), [guard, result, error]() {
            if (!guard)
                return;

            guard->probing_ = false;
            emit guard->probingChanged();

            if (!result) {
                guard->showError(error);
                return;
            }

            guard->registrationId_ = result->registrationId;
            guard->homeserverUrl_  = result->homeserverUrl;
            guard->flowStages_     = result->chosenFlowStages;

            guard->termsPolicies_.clear();
            for (const auto &p : result->termsPolicies) {
                QVariantMap m;
                m[QStringLiteral("id")]      = p.id;
                m[QStringLiteral("version")] = p.version;
                m[QStringLiteral("name")]    = p.name;
                m[QStringLiteral("url")]     = p.url;
                guard->termsPolicies_.append(m);
            }

            guard->probed_ = true;
            emit guard->probedChanged();
        });
    }).detach();
}

void
RegisterPage::checkUsername(const QString &username)
{
    if (registrationId_ == 0)
        return;

    clearError();
    checkingUsername_ = true;
    emit checkingUsernameChanged();

    QPointer<RegisterPage> guard(this);
    const auto regId = registrationId_;
    const auto user  = username.trimmed();

    std::thread([guard, regId, user]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        auto result =
          komai::MatrixRegistrationService::checkUsernameAvailable(context, regId, user, &error);

        QMetaObject::invokeMethod(QCoreApplication::instance(), [guard, result, error]() {
            if (!guard)
                return;

            guard->checkingUsername_ = false;
            emit guard->checkingUsernameChanged();

            if (!result) {
                guard->usernameAvailable_ = false;
                guard->showError(error);
            } else {
                guard->usernameAvailable_ = *result;
            }
            emit guard->usernameChecked();
        });
    }).detach();
}

void
RegisterPage::submitStage(const QString &username,
                          const QString &password,
                          const QString &deviceName,
                          const QString &stageType,
                          const QString &token,
                          const QString &emailSid,
                          const QString &emailClientSecret)
{
    // Store credentials for post-registration login
    lastUsername_   = username.trimmed();
    lastPassword_   = password;
    lastDeviceName_ = deviceName.trimmed().isEmpty() ? initialDeviceName() : deviceName.trimmed();

    if (registrationId_ == 0)
        return;

    clearError();
    registering_ = true;
    emit registeringChanged();

    QPointer<RegisterPage> guard(this);
    const auto regId = registrationId_;
    const auto user  = username.trimmed();
    const auto pass  = password;
    const auto devName =
      deviceName.trimmed().isEmpty() ? initialDeviceName() : deviceName.trimmed();
    const auto stage  = stageType;
    const auto tok    = token;
    const auto sid    = emailSid;
    const auto secret = emailClientSecret;

    std::thread([guard, regId, user, pass, devName, stage, tok, sid, secret]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        auto result = komai::MatrixRegistrationService::submitStage(
          context, regId, user, pass, devName, stage, tok, sid, secret, &error);

        QMetaObject::invokeMethod(QCoreApplication::instance(), [guard, result, error]() {
            if (!guard)
                return;

            guard->registering_ = false;
            emit guard->registeringChanged();

            if (!result) {
                guard->showError(error);
                return;
            }

            if (result->completed) {
                emit guard->registrationComplete(
                  result->userId, result->accessToken, result->deviceId, result->homeserverUrl);
                return;
            }

            // Update stage progress
            guard->completedStages_ = result->completedStages;
            guard->remainingStages_ = result->remainingStages;

            // Update terms policies if newly available
            if (!result->termsPolicies.empty()) {
                guard->termsPolicies_.clear();
                for (const auto &p : result->termsPolicies) {
                    QVariantMap m;
                    m[QStringLiteral("id")]      = p.id;
                    m[QStringLiteral("version")] = p.version;
                    m[QStringLiteral("name")]    = p.name;
                    m[QStringLiteral("url")]     = p.url;
                    guard->termsPolicies_.append(m);
                }
                emit guard->probedChanged();
            }

            emit guard->stageCompleted();
        });
    }).detach();
}

void
RegisterPage::requestEmailToken(const QString &email)
{
    if (registrationId_ == 0)
        return;

    clearError();
    requestingEmail_ = true;
    emit emailStateChanged();

    if (clientSecret_.isEmpty())
        clientSecret_ = generateClientSecret();
    sendAttempt_++;

    QPointer<RegisterPage> guard(this);
    const auto regId   = registrationId_;
    const auto addr    = email.trimmed();
    const auto secret  = clientSecret_;
    const auto attempt = sendAttempt_;

    std::thread([guard, regId, addr, secret, attempt]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        auto result = komai::MatrixRegistrationService::requestEmailToken(
          context, regId, addr, secret, attempt, &error);

        QMetaObject::invokeMethod(QCoreApplication::instance(), [guard, result, error]() {
            if (!guard)
                return;

            guard->requestingEmail_ = false;

            if (!result) {
                guard->showError(error);
                emit guard->emailStateChanged();
                return;
            }

            guard->emailSid_ = result->sid;
            emit guard->emailStateChanged();
        });
    }).detach();
}

void
RegisterPage::cancelRegistration()
{
    if (registrationId_ != 0) {
        komai::MatrixRegistrationService::cancelRegistration(registrationId_);
        registrationId_ = 0;
    }

    probing_          = false;
    registering_      = false;
    requestingEmail_  = false;
    checkingUsername_ = false;

    emit probingChanged();
    emit registeringChanged();
    emit emailStateChanged();
    emit checkingUsernameChanged();
}

void
RegisterPage::reset()
{
    cancelRegistration();

    probed_ = false;
    homeserverUrl_.clear();
    flowStages_.clear();
    termsPolicies_.clear();
    usernameAvailable_ = false;
    completedStages_.clear();
    remainingStages_.clear();
    emailSid_.clear();
    clientSecret_.clear();
    sendAttempt_ = 0;
    error_.clear();

    emit probedChanged();
    emit usernameChecked();
    emit stageCompleted();
    emit emailStateChanged();
    emit errorChanged();
}

void
RegisterPage::showError(const QString &msg)
{
    error_ = StateEventText::translateAuthError(msg);
    emit errorChanged();
}

void
RegisterPage::clearError()
{
    if (!error_.isEmpty()) {
        error_.clear();
        emit errorChanged();
    }
}

void
RegisterPage::performPostRegistrationLogin(const QString &userId,
                                           const QString &deviceId,
                                           const QString &homeserverUrl)
{
    komai::logging::ui()->info(
      "Registration succeeded — performing login to establish matrix-sdk session "
      "(user_id='{}', device_id='{}', homeserver='{}')",
      userId.toStdString(),
      deviceId.toStdString(),
      homeserverUrl.toStdString());

    QPointer<RegisterPage> guard(this);
    const auto profileId   = UserSettings::instance()->profile();
    const auto password    = lastPassword_;
    const auto devId       = deviceId;
    const auto devName     = lastDeviceName_;
    const auto hs          = homeserverUrl;
    const bool verifyCerts = UserSettings::instance()->networkTlsEnableCertificateValidation();

    std::thread([guard, profileId, hs, userId, password, devId, devName, verifyCerts]() {
        const auto context = komai::matrix_backend::blockingCallContext();
        QString error;
        auto result = komai::MatrixAuthService::loginWithPassword(
          context, profileId, hs, userId, password, devId, devName, verifyCerts, &error);

        QMetaObject::invokeMethod(QCoreApplication::instance(), [guard, result, error]() {
            if (!guard)
                return;

            guard->registering_ = false;
            emit guard->registeringChanged();

            if (!result) {
                guard->showError(tr("Account created, but automatic sign-in failed: %1\n"
                                    "Please go back and sign in manually.")
                                   .arg(error));
                return;
            }

            auto *settings                = UserSettings::instance().get();
            const bool hadSessionIdentity = settings->hasPersistedSessionIdentity();

            const bool persisted = settings->persistSessionSnapshot(UserSettings::SessionSnapshot{
              .userId      = result->userId,
              .accessToken = result->accessToken,
              .deviceId    = result->deviceId,
              .homeserver  = result->homeserverUrl,
            });

            if (!persisted) {
                guard->showError(tr("Account created, but session data could not be saved.\n"
                                    "Please go back and sign in manually."));
                return;
            }

            komai::logging::ui()->info(
              "Post-registration login succeeded (user_id='{}', device_id='{}', homeserver='{}')",
              result->userId.toStdString(),
              result->deviceId.toStdString(),
              result->homeserverUrl.toStdString());

            auto *mainWindow = MainWindow::instance();
            mainWindow->showStartupRestorePage();
            QMetaObject::invokeMethod(
              mainWindow,
              [mainWindow, hadSessionIdentity] { mainWindow->showChatPage(hadSessionIdentity); },
              Qt::QueuedConnection);
        });
    }).detach();
}

#include "moc_RegisterPage.cpp"
