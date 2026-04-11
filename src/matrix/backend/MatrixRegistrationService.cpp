// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixRegistrationService.h"

#include "komai-rust-cxxbridge/ffi.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"

#include <stdexcept>

namespace komai {

namespace {

template<typename Func>
auto
invokeRegistrationCall(const char *operation, Func &&func)
{
    return matrix_backend::invokeBlockingCall(
      operation,
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      std::forward<Func>(func));
}

QString
fromRust(const ::rust::String &s)
{
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

QStringList
fromRustVec(const ::rust::Vec<::rust::String> &v)
{
    QStringList result;
    result.reserve(static_cast<int>(v.size()));
    for (const auto &s : v)
        result.append(fromRust(s));
    return result;
}

RegistrationTermsPolicy
fromRustPolicy(const ::komai::rust::RegistrationTermsPolicy &p)
{
    return RegistrationTermsPolicy{
      .id      = fromRust(p.id),
      .version = fromRust(p.version),
      .name    = fromRust(p.name),
      .url     = fromRust(p.url),
    };
}

std::vector<RegistrationTermsPolicy>
fromRustPolicies(const ::rust::Vec<::komai::rust::RegistrationTermsPolicy> &policies)
{
    std::vector<RegistrationTermsPolicy> result;
    result.reserve(policies.size());
    for (const auto &p : policies)
        result.push_back(fromRustPolicy(p));
    return result;
}

} // namespace

std::vector<ServerListEntry>
MatrixRegistrationService::loadServerList()
{
    auto result = ::komai::rust::serverlist_entries();

    if (!result.error_message.empty()) {
        // Log but don't fail — return whatever entries we got
        qWarning("Server list error: %s",
                 QString::fromUtf8(result.error_message.data(),
                                   static_cast<qsizetype>(result.error_message.size()))
                   .toUtf8()
                   .constData());
    }

    std::vector<ServerListEntry> entries;
    entries.reserve(result.entries.size());
    for (const auto &e : result.entries) {
        entries.push_back(ServerListEntry{
          .name            = fromRust(e.name),
          .clientDomain    = fromRust(e.client_domain),
          .description     = fromRust(e.description),
          .homepage        = fromRust(e.homepage),
          .usingVanillaReg = e.using_vanilla_reg,
          .languages       = fromRustVec(e.languages),
          .software        = fromRust(e.software),
          .staffJur        = fromRust(e.staff_jur),
          .rules           = fromRust(e.rules),
          .privacy         = fromRust(e.privacy),
          .captcha         = e.captcha,
          .email           = e.email,
          .features        = fromRustVec(e.features),
          .slidingSync     = e.sliding_sync,
          .regLink         = fromRust(e.reg_link),
          .regNote         = fromRust(e.reg_note),
          .rank            = e.rank,
          .category        = fromRust(e.category),
          .editorial       = fromRust(e.editorial),
          .featured        = e.featured,
        });
    }

    return entries;
}

std::optional<RegistrationProbeResult>
MatrixRegistrationService::probeRegistration(matrix_backend::BlockingCallContext context,
                                             const QString &serverNameOrUrl,
                                             bool verifyCertificates,
                                             QString *errorOut)
{
    try {
        auto result = invokeRegistrationCall("matrix_registration_probe",
                                             [context, &serverNameOrUrl, verifyCertificates]() {
                                                 return ::komai::rust::matrix_registration_probe(
                                                   matrix_backend::toRustBlockingContext(context),
                                                   serverNameOrUrl.toStdString(),
                                                   verifyCertificates);
                                             });

        RegistrationProbeResult probe;
        probe.registrationId   = result.registration_id;
        probe.homeserverUrl    = fromRust(result.homeserver_url);
        probe.session          = fromRust(result.session);
        probe.chosenFlowStages = fromRustVec(result.chosen_flow_stages);

        probe.allFlows.reserve(result.all_flows.size());
        for (const auto &flow : result.all_flows)
            probe.allFlows.push_back(fromRustVec(flow.stages));

        probe.termsPolicies = fromRustPolicies(result.terms_policies);

        return probe;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<bool>
MatrixRegistrationService::checkUsernameAvailable(matrix_backend::BlockingCallContext context,
                                                  uint64_t registrationId,
                                                  const QString &username,
                                                  QString *errorOut)
{
    try {
        auto result = invokeRegistrationCall(
          "matrix_registration_check_username", [context, registrationId, &username]() {
              return ::komai::rust::matrix_registration_check_username(
                matrix_backend::toRustBlockingContext(context),
                registrationId,
                username.toStdString());
          });
        return result.available;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<RegistrationSubmitResult>
MatrixRegistrationService::submitStage(matrix_backend::BlockingCallContext context,
                                       uint64_t registrationId,
                                       const QString &username,
                                       const QString &password,
                                       const QString &deviceName,
                                       const QString &stageType,
                                       const QString &token,
                                       const QString &emailSid,
                                       const QString &emailClientSecret,
                                       QString *errorOut)
{
    try {
        auto result =
          invokeRegistrationCall("matrix_registration_submit_stage",
                                 [context,
                                  registrationId,
                                  &username,
                                  &password,
                                  &deviceName,
                                  &stageType,
                                  &token,
                                  &emailSid,
                                  &emailClientSecret]() {
                                     return ::komai::rust::matrix_registration_submit_stage(
                                       matrix_backend::toRustBlockingContext(context),
                                       registrationId,
                                       username.toStdString(),
                                       password.toStdString(),
                                       deviceName.toStdString(),
                                       stageType.toStdString(),
                                       token.toStdString(),
                                       emailSid.toStdString(),
                                       emailClientSecret.toStdString());
                                 });

        RegistrationSubmitResult submit;
        submit.completed       = result.completed;
        submit.userId          = fromRust(result.user_id);
        submit.accessToken     = fromRust(result.access_token);
        submit.deviceId        = fromRust(result.device_id);
        submit.homeserverUrl   = fromRust(result.homeserver_url);
        submit.session         = fromRust(result.session);
        submit.remainingStages = fromRustVec(result.remaining_stages);
        submit.completedStages = fromRustVec(result.completed_stages);
        submit.termsPolicies   = fromRustPolicies(result.terms_policies);

        return submit;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<RegistrationEmailTokenResult>
MatrixRegistrationService::requestEmailToken(matrix_backend::BlockingCallContext context,
                                             uint64_t registrationId,
                                             const QString &email,
                                             const QString &clientSecret,
                                             uint64_t sendAttempt,
                                             QString *errorOut)
{
    try {
        auto result =
          invokeRegistrationCall("matrix_registration_request_email_token",
                                 [context, registrationId, &email, &clientSecret, sendAttempt]() {
                                     return ::komai::rust::matrix_registration_request_email_token(
                                       matrix_backend::toRustBlockingContext(context),
                                       registrationId,
                                       email.toStdString(),
                                       clientSecret.toStdString(),
                                       sendAttempt);
                                 });

        return RegistrationEmailTokenResult{
          .sid = fromRust(result.sid),
        };
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixRegistrationService::cancelRegistration(uint64_t registrationId, QString *errorOut)
{
    try {
        ::komai::rust::matrix_registration_cancel(registrationId);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

} // namespace komai
