// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

//! flag to be set, when the db should be compacted on startup
bool needsCompact = false;

namespace cache {
void
setNeedsCompactFlag()
{
    needsCompact = true;
}

} // namespace cache

#include "moc_Cache_p.cpp"
