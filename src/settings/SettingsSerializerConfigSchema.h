// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>

#include <QString>

#include "SettingKeys.h"
#include "settings/core/SettingsDefinitions.h"

class UserSettings;

namespace settings::serializer::config {

struct BoolSettingDescriptor
{
    const char *key;
    bool defaultValue;
    bool (UserSettings::*getter)() const;
    void (UserSettings::*setter)(bool);
};

struct IntSettingDescriptor
{
    const char *key;
    int defaultValue;
    int (UserSettings::*getter)() const;
    void (UserSettings::*setter)(int);
};

struct UintSettingDescriptor
{
    const char *key;
    uint defaultValue;
    uint (UserSettings::*getter)() const;
    void (UserSettings::*setter)(uint);
};

struct ULongLongSettingDescriptor
{
    const char *key;
    qulonglong defaultValue;
    qulonglong (UserSettings::*getter)() const;
    void (UserSettings::*setter)(qulonglong);
};

struct DoubleSettingDescriptor
{
    const char *key;
    double defaultValue;
    double (UserSettings::*getter)() const;
    void (UserSettings::*setter)(double);
};

struct StringSettingDescriptor
{
    const char *key;
    QString defaultValue;
    QString (UserSettings::*getter)() const;
    void (UserSettings::*setter)(QString);
};

std::span<const BoolSettingDescriptor>
boolConfigSettings();
std::span<const IntSettingDescriptor>
intConfigSettings();
std::span<const UintSettingDescriptor>
uintConfigSettings();
std::span<const ULongLongSettingDescriptor>
ulonglongConfigSettings();
std::span<const DoubleSettingDescriptor>
doubleConfigSettings();
std::span<const StringSettingDescriptor>
stringConfigSettings();

void
validateConfigSchemaDescriptors();

inline constexpr bool kDefaultUiMotionAnimationsEnabled =
  settings::core::definitions::kDefaultUiMotionAnimationsEnabled;
inline constexpr bool kDefaultUiInputMode = settings::core::definitions::kDefaultUiInputMode;
inline constexpr bool kDefaultCertificateValidationEnabled =
  settings::core::definitions::kDefaultCertificateValidationEnabled;
inline constexpr bool kDefaultNetworkHttp3Enabled =
  settings::core::definitions::kDefaultNetworkHttp3Enabled;
inline constexpr bool kDefaultNetworkMrsEnabled =
  settings::core::definitions::kDefaultNetworkMrsEnabled;
inline constexpr const char *kDefaultNetworkMrsServerName =
  settings::core::definitions::kDefaultNetworkMrsServerName;
inline constexpr double kDefaultScaleFactor = settings::core::definitions::kDefaultScaleFactor;
inline constexpr double kDefaultFontSizePt  = settings::core::definitions::kDefaultFontSizePt;
inline constexpr double kDefaultTimelineMediaAudioPlaybackSpeed =
  settings::core::definitions::kDefaultTimelineMediaAudioPlaybackSpeed;
inline constexpr int kDefaultScreenShareFrameRate =
  settings::core::definitions::kDefaultScreenShareFrameRate;
inline constexpr int kDefaultDesktopWindowFocusBlurDelaySeconds =
  settings::core::definitions::kDefaultDesktopWindowFocusBlurDelaySeconds;
} // namespace settings::serializer::config
