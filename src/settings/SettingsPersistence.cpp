// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsPersistence.h"
#include "settings/SettingsPersistenceInternal.h"

#include "logging/Logging.h"

#include <string>
#include <string_view>

namespace settings::persistence {

namespace {

PersistenceLoggers
defaultLoggers()
{
    return {.ui = std::make_shared<nhlog::Logger>("settings-persistence-ui")};
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
providerFromConfigValue(QStringView providerValue)
{
    return staged_load_plan::providerFromConfigValue(providerValue);
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
