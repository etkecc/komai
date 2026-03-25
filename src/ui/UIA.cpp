// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UIA.h"

#include <algorithm>

#include <QInputDialog>
#include <QTimer>

#include <mtx/requests.hpp>
#include <mtx/responses/common.hpp>

#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "utils/Utils.h"

namespace {
void
reportUia3pidUnavailable(UIA *uia, QString action)
{
    nhlog::ui()->warn(
      "Refusing legacy UIA action '{}'; this flow is not migrated to the matrix-sdk backend yet",
      action.toStdString());
    emit uia->error(
      UIA::tr("%1 is not migrated to the matrix-sdk backend yet.").arg(std::move(action)));
}
}

UIA *
UIA::instance()
{
    static UIA uia{nullptr};
    return &uia;
}

mtx::http::UIAHandler
UIA::genericHandler(QString context)
{
    return mtx::http::UIAHandler([this, context](const mtx::http::UIAHandler &h,
                                                 const mtx::user_interactive::Unauthorized &u) {
        QTimer::singleShot(0, this, [this, h, u, context]() {
            this->currentHandler = h;
            this->currentStatus  = u;
            this->title_         = context;
            emit titleChanged();

            std::vector<mtx::user_interactive::Flow> flows = u.flows;

            nhlog::ui()->info("Completed stages: {}", u.completed.size());

            if (!u.completed.empty()) {
                // Get rid of all flows which don't start with the sequence of
                // stages that have already been completed.
                flows.erase(std::remove_if(flows.begin(),
                                           flows.end(),
                                           [completed_stages = u.completed](auto flow) {
                                               if (completed_stages.size() > flow.stages.size())
                                                   return true;
                                               for (size_t f = 0; f < completed_stages.size(); f++)
                                                   if (completed_stages[f] != flow.stages[f])
                                                       return true;
                                               return false;
                                           }),
                            flows.end());
            }

            if (flows.empty()) {
                nhlog::ui()->error("No available registration flows!");
                emit error(tr("No available registration flows!"));
                return;
            }

            // sort flows with known stages first
            std::sort(
              flows.begin(),
              flows.end(),
              [](const mtx::user_interactive::Flow &a, const mtx::user_interactive::Flow &b) {
                  auto calcWeight = [](const mtx::user_interactive::Flow &f) {
                      using namespace mtx::user_interactive::auth_types;
                      const static std::map<std::string_view, int> weights{
                        {mtx::user_interactive::auth_types::password, 0},
                        {mtx::user_interactive::auth_types::email_identity, 0},
                        {mtx::user_interactive::auth_types::msisdn, 0},
                        {mtx::user_interactive::auth_types::dummy, 0},
                        {mtx::user_interactive::auth_types::registration_token, 0},
                        // recaptcha is known, but we'd like to avoid it, because it calls out to
                        // the browser
                        {mtx::user_interactive::auth_types::recaptcha, 1},
                      };
                      int weight = 0;
                      for (const auto &s : f.stages) {
                          if (!weights.count(s))
                              weight += 3;
                          else
                              weight += weights.at(s);
                      }
                      return weight;
                  };

                  return calcWeight(a) < calcWeight(b);
              });

            auto current_stage = flows.front().stages.at(u.completed.size());

            if (current_stage == mtx::user_interactive::auth_types::password) {
                emit password();
            } else if (current_stage == mtx::user_interactive::auth_types::email_identity) {
                emit email();
            } else if (current_stage == mtx::user_interactive::auth_types::msisdn) {
                emit phoneNumber();
            } else if (current_stage == mtx::user_interactive::auth_types::recaptcha) {
                auto captcha = new ReCaptcha(QString::fromStdString(u.session), context, nullptr);
                QQmlEngine::setObjectOwnership(captcha, QQmlEngine::JavaScriptOwnership);
                connect(captcha, &ReCaptcha::confirmation, this, [h, u]() {
                    h.next(mtx::user_interactive::Auth{u.session,
                                                       mtx::user_interactive::auth::Fallback{}});
                });
                connect(captcha, &ReCaptcha::cancelled, this, [this]() {
                    emit error(tr("Registration aborted"));
                });
                emit reCaptcha(captcha);
            } else if (current_stage == mtx::user_interactive::auth_types::dummy) {
                h.next(
                  mtx::user_interactive::Auth{u.session, mtx::user_interactive::auth::Dummy{}});

            } else if (current_stage == mtx::user_interactive::auth_types::registration_token) {
                bool ok;
                QString token =
                  QInputDialog::getText(nullptr,
                                        context,
                                        tr("Please enter a valid registration token."),
                                        QLineEdit::Normal,
                                        QString(),
                                        &ok);

                if (ok) {
                    h.next(mtx::user_interactive::Auth{
                      u.session,
                      mtx::user_interactive::auth::RegistrationToken{token.toStdString()}});
                } else {
                    emit error(tr("Registration aborted"));
                }
            } else {
                // use fallback
                auto fallback = new FallbackAuth(QString::fromStdString(u.session),
                                                 QString::fromStdString(current_stage),
                                                 nullptr);
                QQmlEngine::setObjectOwnership(fallback, QQmlEngine::JavaScriptOwnership);
                connect(fallback, &FallbackAuth::confirmation, this, [h, u]() {
                    h.next(mtx::user_interactive::Auth{u.session,
                                                       mtx::user_interactive::auth::Fallback{}});
                });
                connect(fallback, &FallbackAuth::cancelled, this, [this]() {
                    emit error(tr("Registration aborted"));
                });
                emit fallbackAuth(fallback);
            }
        });
    });
}

void
UIA::continuePassword(const QString &password)
{
    mtx::user_interactive::auth::Password p{};
    p.identifier_type = mtx::user_interactive::auth::Password::UserId;
    p.password        = password.toStdString();
    p.identifier_user = utils::localUser().toStdString();

    if (currentHandler)
        currentHandler->next(mtx::user_interactive::Auth{currentStatus.session, p});
}

void
UIA::continueEmail(const QString &email)
{
    (void)email;
    reportUia3pidUnavailable(this, tr("Email verification"));
}
void
UIA::continuePhoneNumber(const QString &countryCode, const QString &phoneNumber)
{
    (void)countryCode;
    (void)phoneNumber;
    reportUia3pidUnavailable(this, tr("Phone-number verification"));
}

void
UIA::continue3pidReceived()
{
    mtx::user_interactive::auth::ThreePIDCred c{};
    c.client_secret = this->client_secret;
    c.sid           = this->sid;

    if (this->email_) {
        mtx::user_interactive::auth::EmailIdentity i{};
        i.threepidCred = c;
        this->currentHandler->next(mtx::user_interactive::Auth{currentStatus.session, i});
    } else {
        mtx::user_interactive::auth::MSISDN i{};
        i.threepidCred = c;
        this->currentHandler->next(mtx::user_interactive::Auth{currentStatus.session, i});
    }
}

void
UIA::submit3pidToken(const QString &token)
{
    (void)token;
    reportUia3pidUnavailable(this, tr("3PID token submission"));
}

#include "moc_UIA.cpp"
