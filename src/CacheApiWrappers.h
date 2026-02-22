// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include "Cache.h"

namespace spdlog {
class logger;
}

class Cache;

namespace cache {
std::unique_ptr<Cache> &
cacheInstance();

struct CacheLoggers
{
    std::shared_ptr<spdlog::logger> db;
};

void
setLoggers(CacheLoggers loggers);
CacheLoggers
activeLoggers();
}
