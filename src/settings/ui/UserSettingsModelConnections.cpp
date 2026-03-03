// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/UserSettingsModel.h"

#include <array>

#include "Logging.h"
#include "settings/ui/SettingDescriptor.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"

using settings::ui::rowForSettingId;

void
UserSettingsModel::wireSettingConnections(UserSettings *settings)
{
    if (!settings)
        return;

#define CONNECT_SETTING_ID(id, sig, ...)                                                           \
    if (const int idx = rowForSettingId(settings::core::SettingId::id); idx >= 0) {                \
        connect(settings, &UserSettings::sig, this, [this, idx]() {                                \
            emit dataChanged(index(idx), index(idx), {__VA_ARGS__});                               \
        });                                                                                        \
    } else {                                                                                       \
        nhlog::ui()->warn(                                                                         \
          "Missing settings row for SettingId::{} while wiring signal '{}'", #id, #sig);           \
    }

#include "settings/ui/connections/UserSettingsModelConnectionsCalls.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsComposer.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsEncryption.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsIntegrations.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsLookFeel.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsNetwork.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsNotifications.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsPrivacy.inc"
#include "settings/ui/connections/UserSettingsModelConnectionsTimeline.inc"

#undef CONNECT_SETTING_ID
}
