// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include "Cache.h"

class Cache;

namespace cache {
std::unique_ptr<Cache> &
cacheInstance();
}
