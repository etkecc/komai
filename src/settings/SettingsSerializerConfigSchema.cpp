// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerConfigSchema.h"

#include <array>

#include <QString>

#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::serializer::config {

namespace {

const std::array<BoolSettingDescriptor, 49> BoolSettings{
#include "SettingsSerializerConfigSchemaBoolCalls.inc"
#include "SettingsSerializerConfigSchemaBoolComposer.inc"
#include "SettingsSerializerConfigSchemaBoolEncryption.inc"
#include "SettingsSerializerConfigSchemaBoolIntegrations.inc"
#include "SettingsSerializerConfigSchemaBoolLookFeel.inc"
#include "SettingsSerializerConfigSchemaBoolNetwork.inc"
#include "SettingsSerializerConfigSchemaBoolNotifications.inc"
#include "SettingsSerializerConfigSchemaBoolPrivacy.inc"
#include "SettingsSerializerConfigSchemaBoolSidebars.inc"
#include "SettingsSerializerConfigSchemaBoolTimeline.inc"
};

const std::array<IntSettingDescriptor, 3> IntSettings{
#include "SettingsSerializerConfigSchemaIntCalls.inc"
#include "SettingsSerializerConfigSchemaIntLookFeel.inc"
#include "SettingsSerializerConfigSchemaIntPrivacy.inc"
};

const std::array<UintSettingDescriptor, 1> UintSettings{
  UintSettingDescriptor{SettingKey::DbMaxStores,
                        kDefaultMaxStores,
                        &UserSettings::dbMaxStores,
                        &UserSettings::setDbMaxStores},
};

const std::array<ULongLongSettingDescriptor, 1> ULongLongSettings{
  ULongLongSettingDescriptor{SettingKey::DbMaxSizeBytes,
                             kDefaultMaxDbSizeBytes,
                             &UserSettings::dbMaxSizeBytes,
                             &UserSettings::setDbMaxSizeBytes},
};

const std::array<DoubleSettingDescriptor, 2> DoubleSettings{
  DoubleSettingDescriptor{SettingKey::UiFontSizePt,
                          kDefaultFontSizePt,
                          &UserSettings::uiFontSizePt,
                          &UserSettings::setUiFontSizePt},
  DoubleSettingDescriptor{SettingKey::TimelineMediaDefaultAudioPlaybackSpeed,
                          kDefaultTimelineMediaAudioPlaybackSpeed,
                          &UserSettings::timelineMediaDefaultAudioPlaybackSpeed,
                          &UserSettings::setTimelineMediaDefaultAudioPlaybackSpeed},
};

const std::array<StringSettingDescriptor, 9> StringSettings{
#include "SettingsSerializerConfigSchemaStringCalls.inc"
#include "SettingsSerializerConfigSchemaStringIntegrations.inc"
#include "SettingsSerializerConfigSchemaStringLookFeel.inc"
#include "SettingsSerializerConfigSchemaStringTimeline.inc"
};

} // namespace

std::span<const BoolSettingDescriptor>
boolConfigSettings()
{
    return BoolSettings;
}

std::span<const IntSettingDescriptor>
intConfigSettings()
{
    return IntSettings;
}

std::span<const UintSettingDescriptor>
uintConfigSettings()
{
    return UintSettings;
}

std::span<const ULongLongSettingDescriptor>
ulonglongConfigSettings()
{
    return ULongLongSettings;
}

std::span<const DoubleSettingDescriptor>
doubleConfigSettings()
{
    return DoubleSettings;
}

std::span<const StringSettingDescriptor>
stringConfigSettings()
{
    return StringSettings;
}

} // namespace settings::serializer::config
