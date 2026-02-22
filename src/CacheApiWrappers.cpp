// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <spdlog/logger.h>

#include <utility>

// Keep logger holder definition alongside this source translation unit.
#include "CacheApiWrappers.h"

namespace cache {

namespace {

CacheLoggers
defaultLoggers()
{
    return {};
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
    currentLoggers() = std::move(loggers);
}

CacheLoggers
activeLoggers()
{
    return currentLoggers();
}

} // namespace cache
