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

// Custom-QML keyword lists, grouped by section. Keep these in sync with
// what each custom subpage actually shows the user. Prefer concrete
// user-visible terms (labels, control captions) plus the obvious synonyms
// a user would type to find them ("logout"/"sign out", "purge"/"clear",
// etc.).
//
// The QML on each tab gates its sections' visibility on `sectionId`, so a
// query that matches only one bucket narrows the visible content to that
// section.
//
// These strings are matched in both source-English (these arrays) and
// translated form (via tr()), so wrap each with QT_TRANSLATE_NOOP so
// translators see them.

static const char *const accountProfileKeywordsEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Profile"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Avatar"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Display name"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "User ID"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Homeserver"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Identity"),
  nullptr,
};

static const char *const accountThisDeviceKeywordsEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "This device"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "This session"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Current device"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Device name"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Access token"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Sign out"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Logout"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Encryption keys"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Export encryption keys"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Import encryption keys"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Key backup file"),
  nullptr,
};

static const char *const accountOtherDevicesKeywordsEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Other devices"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Other sessions"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Sessions"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Verify"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Verification"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Verified"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Encryption keys"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Refresh devices"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Sign out other devices"),
  nullptr,
};

static const char *const accountUsersKeywordsEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Users"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Ignored users"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Block user"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Mute user"),
  nullptr,
};

static const char *const accountLocalCacheKeywordsEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Local cache"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Cache"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Matrix SDK state store"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Matrix SDK cache"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Media cache"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Cache backend"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Cache size"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Cache directory"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Purge cache"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Clear cache"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Storage"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Disk usage"),
  nullptr,
};

static const TabSearchSection accountTabSections[] = {
  {"profile", accountProfileKeywordsEnglish},
  {"thisDevice", accountThisDeviceKeywordsEnglish},
  {"otherDevices", accountOtherDevicesKeywordsEnglish},
  {"users", accountUsersKeywordsEnglish},
  {"localCache", accountLocalCacheKeywordsEnglish},
  {nullptr, nullptr},
};

static const char *const applicationProfilesMainKeywordsEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Application profile"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Multi-account"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Multiple accounts"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Switch profile"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Create profile"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Delete profile"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Desktop launcher"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Work profile"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Personal profile"),
  nullptr,
};

static const TabSearchSection applicationProfilesTabSections[] = {
  {"main", applicationProfilesMainKeywordsEnglish},
  {nullptr, nullptr},
};

static const char *const integrationsTranscriptionKeywordsEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Voice transcription"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Speech to text"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Whisper"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "OpenAI"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "API key"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Transcription provider"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Transcription model"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Transcription prompt"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Transcription hosting"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Realtime transcription"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Streaming transcription"),
  nullptr,
};

static const char *const integrationsBrowserKeywordsEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Link browser command"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Custom browser"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Default browser"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Open links"),
  nullptr,
};

static const TabSearchSection integrationsTabSections[] = {
  {"transcription", integrationsTranscriptionKeywordsEnglish},
  {"browser", integrationsBrowserKeywordsEnglish},
  {nullptr, nullptr},
};

static const char *const timelineStateEventsKeywordsEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "State events"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Joins and leaves"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Member events"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Topic changes"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Name changes"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Avatar changes"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Power level changes"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Kicks"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Bans"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Noisy events"),
  nullptr,
};

static const TabSearchSection timelineTabSections[] = {
  {"stateEvents", timelineStateEventsKeywordsEnglish},
  {nullptr, nullptr},
};

const TabSearchSection *
customSectionsForTab(int tab)
{
    switch (tab) {
    case UserSettingsModel::TabAccount:
        return accountTabSections;
    case UserSettingsModel::TabApplicationProfiles:
        return applicationProfilesTabSections;
    case UserSettingsModel::TabIntegrations:
        return integrationsTabSections;
    case UserSettingsModel::TabTimeline:
        return timelineTabSections;
    }
    return nullptr;
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
    if (helper == &desktopSystemTrayIconStyleValues)
        return desktopSystemTrayIconStyleValuesEnglish;
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
    if (helper == &timelineRoomHeaderButtonLabelsValues)
        return timelineRoomHeaderButtonLabelsValuesEnglish;
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
