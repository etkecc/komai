// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsPersistence.h"
#include "settings/SettingsPersistenceInternal.h"

#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>

#include <string>
#include <string_view>

namespace settings::persistence {

namespace {

std::shared_ptr<spdlog::logger>
nullLogger(std::string_view name)
{
    static auto sink   = std::make_shared<spdlog::sinks::null_sink_mt>();
    static auto logger = std::make_shared<spdlog::logger>(std::string(name), sink);
    return logger;
}

PersistenceLoggers
defaultLoggers()
{
    return {.ui = nullLogger("settings-persistence-ui")};
}

PersistenceLoggers &
currentLoggers()
{
    static PersistenceLoggers loggers = defaultLoggers();
    return loggers;
}

} // namespace

void
setLoggers(PersistenceLoggers loggers)
{
    const auto &defaults = defaultLoggers();
    if (!loggers.ui)
        loggers.ui = defaults.ui;
    currentLoggers() = std::move(loggers);
}

const PersistenceLoggers &
activeLoggers()
{
    return currentLoggers();
}

staged_load_plan::SecretsProvider
providerFromConfig(const YAML::Node &configRoot)
{
    return staged_load_plan::providerFromConfig(configRoot);
}

namespace detail {

void
storeInternalSessionMetadata(QMap<QString, QString> &secrets, const QString &accessToken)
{
    constexpr auto sessionAccessTokenKey = "__session.access_token";

    if (accessToken.isEmpty())
        secrets.remove(sessionAccessTokenKey);
    else
        secrets[sessionAccessTokenKey] = accessToken;
}

bool
extractInternalSessionMetadata(SecretsPayload &payload)
{
    constexpr auto sessionAccessTokenKey = "__session.access_token";
    constexpr auto sessionKeyPrefix      = "__session.";

    const auto internalAccessToken = payload.secrets.value(sessionAccessTokenKey);
    if (!internalAccessToken.isEmpty())
        payload.accessToken = internalAccessToken;

    payload.secrets.remove(sessionAccessTokenKey);

    bool prunedUnexpectedInternalKeys = false;
    for (auto it = payload.secrets.begin(); it != payload.secrets.end();) {
        if (it.key().startsWith(QLatin1String(sessionKeyPrefix))) {
            it                           = payload.secrets.erase(it);
            prunedUnexpectedInternalKeys = true;
        } else {
            ++it;
        }
    }

    return prunedUnexpectedInternalKeys;
}

} // namespace detail

} // namespace settings::persistence
