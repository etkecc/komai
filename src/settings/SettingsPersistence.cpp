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
storeInternalSessionMetadata(QMap<QString, QString> &secrets,
                             const QString &userId,
                             const QString &deviceId,
                             const QString &homeserver)
{
    constexpr auto sessionUserIdKey     = "__session.user_id";
    constexpr auto sessionDeviceIdKey   = "__session.device_id";
    constexpr auto sessionHomeserverKey = "__session.homeserver";

    if (userId.isEmpty())
        secrets.remove(sessionUserIdKey);
    else
        secrets[sessionUserIdKey] = userId;

    if (deviceId.isEmpty())
        secrets.remove(sessionDeviceIdKey);
    else
        secrets[sessionDeviceIdKey] = deviceId;

    if (homeserver.isEmpty())
        secrets.remove(sessionHomeserverKey);
    else
        secrets[sessionHomeserverKey] = homeserver;
}

void
extractInternalSessionMetadata(SecretsPayload &payload)
{
    constexpr auto sessionUserIdKey     = "__session.user_id";
    constexpr auto sessionDeviceIdKey   = "__session.device_id";
    constexpr auto sessionHomeserverKey = "__session.homeserver";

    payload.sessionUserId     = payload.secrets.value(sessionUserIdKey);
    payload.sessionDeviceId   = payload.secrets.value(sessionDeviceIdKey);
    payload.sessionHomeserver = payload.secrets.value(sessionHomeserverKey);

    payload.secrets.remove(sessionUserIdKey);
    payload.secrets.remove(sessionDeviceIdKey);
    payload.secrets.remove(sessionHomeserverKey);
}

} // namespace detail

} // namespace settings::persistence
