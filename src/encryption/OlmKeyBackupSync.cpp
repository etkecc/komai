// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Olm.h"

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/TimelineViewManager.h"

namespace olm {

void
download_full_keybackup()
{
    nhlog::crypto()->warn(
      "Skipping legacy full online key-backup download; this flow is not migrated to the "
      "matrix-sdk backend yet");
}

void
lookup_keybackup(const std::string &room, const std::string &session_id)
{
    nhlog::crypto()->warn("Skipping legacy online key-backup lookup for ({}, {}); this flow is "
                          "not migrated to the matrix-sdk backend yet",
                          room,
                          session_id);
}

} // namespace olm
