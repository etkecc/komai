// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "MatrixStateTypes.h"

namespace cache {
enum class CacheVersion : int
{
    Older   = -1,
    Current = 0,
    Newer   = 1,
};
}
