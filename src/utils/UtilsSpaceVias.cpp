// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/Utils.h"

#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"

void
utils::updateSpaceVias()
{
    if (!UserSettings::instance()->networkSpacesMaintainJoinMetadata())
        return;

    nhlog::ui()->warn(
      "Automatic space via maintenance is not migrated to matrix-sdk yet; skipping run");
}
