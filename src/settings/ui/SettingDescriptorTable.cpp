// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/SettingDescriptor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <array>
#include <cstring>

#include "config/komai.h"
#include "settings/core/StartupConfig.h"
#include "settings/ui/LanguagePresentation.h"
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
    #include "rows/UserSettingsModelNavigation.inc"
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

const char *const *
valuesEnglishFor(QVariant (*helper)())
{
    if (!helper)
        return nullptr;

    // Each entry pairs a runtime helper (which returns translated values
    // via QCoreApplication::translate) with the matching source-English
    // array, so the search proxy can match enum options like "Compact" or
    // "Bubbles" cross-locale. Dynamic helpers — fonts, languages, audio
    // devices, themes — are intentionally absent: their lists are derived
    // from runtime data rather than translatable string literals.
    if (helper == &composerSendMessageKeyValues)
        return composerSendMessageKeyValuesEnglish;
    if (helper == &composerAutoReplaceEmojiValues)
        return composerAutoReplaceEmojiValuesEnglish;
    if (helper == &composerEmojiPreferredGenderValues)
        return composerEmojiPreferredGenderValuesEnglish;
    if (helper == &composerEmojiPreferredSkinToneValues)
        return composerEmojiPreferredSkinToneValuesEnglish;
    if (helper == &desktopNotificationsMessageContentPolicyValues)
        return desktopNotificationsMessageContentPolicyValuesEnglish;
    if (helper == &integrationsDbusApiAccessValues)
        return integrationsDbusApiAccessValuesEnglish;
    if (helper == &lookFeelDefaultAvatarStyleValues)
        return lookFeelDefaultAvatarStyleValuesEnglish;
    if (helper == &lookFeelDensityValues)
        return lookFeelDensityValuesEnglish;
    if (helper == &lookFeelScrollbarPolicyValues)
        return lookFeelScrollbarPolicyValuesEnglish;
    if (helper == &navigationLastMessagePreviewValues)
        return navigationLastMessagePreviewValuesEnglish;
    if (helper == &navigationRoomListOpeningPolicyValues)
        return navigationRoomListOpeningPolicyValuesEnglish;
    if (helper == &navigationRoomSortValues)
        return navigationRoomSortValuesEnglish;
    if (helper == &navigationTabsPinnedTabLabelValues)
        return navigationTabsPinnedTabLabelValuesEnglish;
    if (helper == &navigationTabsShowPinButtonValues)
        return navigationTabsShowPinButtonValuesEnglish;
    if (helper == &navigationTabsTabLabelValues)
        return navigationTabsTabLabelValuesEnglish;
    if (helper == &networkPresenceStatusPolicyValues)
        return networkPresenceStatusPolicyValuesEnglish;
    if (helper == &timelineAvatarSizeValues)
        return timelineAvatarSizeValuesEnglish;
    if (helper == &timelineMessageActionsActivationPolicyValues)
        return timelineMessageActionsActivationPolicyValuesEnglish;
    if (helper == &timelineMessagesLayoutPositioningValues)
        return timelineMessagesLayoutPositioningValuesEnglish;
    if (helper == &timelineMessagesStyleValues)
        return timelineMessagesStyleValuesEnglish;
    if (helper == &timelineSenderUsernameValues)
        return timelineSenderUsernameValuesEnglish;
    if (helper == &timelineShowImageValues)
        return timelineShowImageValuesEnglish;
    if (helper == &timelineUserColorCodingPolicyValues)
        return timelineUserColorCodingPolicyValuesEnglish;
    return nullptr;
}

#undef I
#undef SM
#include "settings/ui/SettingDescriptorRowMacrosUndef.inc"

} // namespace settings::ui
