// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerConfigSchema.h"

#include <QString>

#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::serializer::config {

namespace {

const BoolSettingDescriptor BoolSettings[] = {
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

const IntSettingDescriptor IntSettings[] = {
#include "SettingsSerializerConfigSchemaIntCalls.inc"
#include "SettingsSerializerConfigSchemaIntLookFeel.inc"
#include "SettingsSerializerConfigSchemaIntPrivacy.inc"
};

const UintSettingDescriptor UintSettings[] = {
  UintSettingDescriptor{SettingKey::DbMaxStores,
                        kDefaultMaxStores,
                        &UserSettings::dbMaxStores,
                        &UserSettings::setDbMaxStores},
};

const ULongLongSettingDescriptor ULongLongSettings[] = {
  ULongLongSettingDescriptor{SettingKey::DbMaxSizeBytes,
                             kDefaultMaxDbSizeBytes,
                             &UserSettings::dbMaxSizeBytes,
                             &UserSettings::setDbMaxSizeBytes},
};

const DoubleSettingDescriptor DoubleSettings[] = {
  DoubleSettingDescriptor{SettingKey::UiFontSizePt,
                          kDefaultFontSizePt,
                          &UserSettings::uiFontSizePt,
                          &UserSettings::setUiFontSizePt},
  DoubleSettingDescriptor{SettingKey::TimelineMediaDefaultAudioPlaybackSpeed,
                          kDefaultTimelineMediaAudioPlaybackSpeed,
                          &UserSettings::timelineMediaDefaultAudioPlaybackSpeed,
                          &UserSettings::setTimelineMediaDefaultAudioPlaybackSpeed},
};

const StringSettingDescriptor StringSettings[] = {
#include "SettingsSerializerConfigSchemaStringCalls.inc"
#include "SettingsSerializerConfigSchemaStringIntegrations.inc"
#include "SettingsSerializerConfigSchemaStringLookFeel.inc"
#include "SettingsSerializerConfigSchemaStringNetwork.inc"
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
