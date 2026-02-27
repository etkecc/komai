// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/core/Cache.h"
#include "cache/core/Cache_p.h"

#include <algorithm>
#include <mtxclient/utils.hpp>

#include <spdlog/logger.h>

#include "cache/api/CacheApiWrappers.h"

std::string
Cache::pickleSecret()
{
    return pickle_secret_;
}

std::string
Cache::createPickleSecret()
{
    if (!this->pickle_secret_.empty()) {
        cache::activeLoggers().crypto->warn(
          "pickle secret already loaded; reusing existing secret");
        return this->pickle_secret_;
    }

    this->pickle_secret_ = mtx::client::utils::random_token(64, true);
    storeSecretInStore("pickle_secret", pickle_secret_);
    return pickle_secret_;
}
