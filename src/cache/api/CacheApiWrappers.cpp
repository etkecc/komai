// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>

#include <string_view>
#include <utility>

// Keep logger holder definition alongside this source translation unit.
#include "cache/api/CacheApiWrappers.h"

namespace cache {

namespace {

std::shared_ptr<spdlog::logger>
nullCacheLogger(std::string_view name)
{
    static auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    if (name == "cache-db") {
        static auto logger = std::make_shared<spdlog::logger>("cache-db", sink);
        return logger;
    }
    if (name == "cache-crypto") {
        static auto logger = std::make_shared<spdlog::logger>("cache-crypto", sink);
        return logger;
    }
    static auto logger = std::make_shared<spdlog::logger>("cache-net", sink);
    return logger;
}

CacheLoggers
defaultLoggers()
{
    static const CacheLoggers loggers{
      .db     = nullCacheLogger("cache-db"),
      .crypto = nullCacheLogger("cache-crypto"),
      .net    = nullCacheLogger("cache-net"),
    };
    return loggers;
}

CacheLoggers &
currentLoggers()
{
    static CacheLoggers loggers = defaultLoggers();
    return loggers;
}

} // namespace

void
setLoggers(CacheLoggers loggers)
{
    const auto &defaults = defaultLoggers();
    if (!loggers.db)
        loggers.db = defaults.db;
    if (!loggers.crypto)
        loggers.crypto = defaults.crypto;
    if (!loggers.net)
        loggers.net = defaults.net;

    currentLoggers() = std::move(loggers);
}

const CacheLoggers &
activeLoggers()
{
    return currentLoggers();
}

} // namespace cache
