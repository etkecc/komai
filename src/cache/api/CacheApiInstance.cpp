// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/api/CacheApiContext.h"
#include "cache/api/CacheApiLifecycle.h"
#include "cache/core/Cache_p.h"

#include <memory>

namespace {
std::unique_ptr<MatrixStore> instance_ = nullptr;
}

namespace cache {

std::unique_ptr<MatrixStore> &
cacheInstance()
{
    return instance_;
}

void
init(const QString &user_id)
{
    instance_ = std::make_unique<MatrixStore>(user_id);
}

bool
isAvailable() noexcept
{
    return instance_ != nullptr;
}

bool
isDatabaseReady()
{
    return instance_ && instance_->isDatabaseReady();
}

bool
isMapFullError(const std::exception &e) noexcept
{
    return instance_ && instance_->isMapFullError(e);
}

} // namespace cache
