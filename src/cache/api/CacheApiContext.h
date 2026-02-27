// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

namespace spdlog {
class logger;
}

class MatrixStore;
using Cache = MatrixStore;

namespace cache {
std::unique_ptr<MatrixStore> &
cacheInstance();

struct CacheLoggers
{
    std::shared_ptr<spdlog::logger> db;
    std::shared_ptr<spdlog::logger> crypto;
    std::shared_ptr<spdlog::logger> net;
};

void
setLoggers(CacheLoggers loggers);
const CacheLoggers &
activeLoggers();
}
