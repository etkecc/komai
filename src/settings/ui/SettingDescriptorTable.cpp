// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/SettingDescriptor.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <array>

#include "config/komai.h"
#include "settings/core/StartupConfig.h"
#include "settings/ui/SettingDescriptorValueAccessors.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/Theme.h"
#include "ui/ThemeRegistry.h"
#include "utils/Utils.h"
#include "voip/CallDevices.h"

namespace settings::ui {

#define I UserSettings::instance()
#define SM UserSettingsModel

using descriptor_value::enumMaxValue;
using descriptor_value::getCoreBoolValue;
using descriptor_value::getCoreDoubleValue;
using descriptor_value::getCoreEnumValue;
using descriptor_value::getCoreIntValue;
using descriptor_value::getCoreStringValue;
using descriptor_value::getSettingEnumValue;
using descriptor_value::getSettingValue;
using descriptor_value::setSettingEnumValue;
using descriptor_value::setSettingValue;

// Row macros require `SM` and accessors above, so include them locally where
// descriptor table wiring is assembled.
#include "settings/ui/SettingDescriptorCallbacks.inc"
#include "settings/ui/SettingDescriptorRowMacros.inc"

// clang-format off
const SettingMeta settingsTable[] = {
    #include "rows/UserSettingsModelLookFeel.inc"
    #include "rows/UserSettingsModelTimeline.inc"
    #include "rows/UserSettingsModelComposer.inc"
    #include "rows/UserSettingsModelDesktop.inc"
    #include "rows/UserSettingsModelCalls.inc"
    #include "rows/UserSettingsModelIntegrations.inc"
    #include "rows/UserSettingsModelNetwork.inc"
    #include "rows/UserSettingsModelAccount.inc"
    #include "rows/UserSettingsModelAbout.inc"
};
// clang-format on

int
settingsTableRowCount()
{
    return static_cast<int>(std::size(settingsTable));
}

#undef I
#undef SM
#include "settings/ui/SettingDescriptorRowMacrosUndef.inc"

} // namespace settings::ui
