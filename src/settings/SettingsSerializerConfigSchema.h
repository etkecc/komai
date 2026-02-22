// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>

#include <QString>

#include "SettingKeys.h"

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

constexpr bool kDefaultUiMotionAnimationsEnabled  = true;
constexpr bool kDefaultInputEnableTextSelection   = true;
constexpr double kDefaultScaleFactor              = -1.0;
constexpr double kDefaultFontSizePt               = 13.0;
constexpr int kDefaultScreenShareFrameRate        = 5;
constexpr int kDefaultPrivacyScreenTimeoutSeconds = 0;
constexpr int kDefaultTimelineMaxWidthPx          = 0;
constexpr int kDefaultMaxDbs                      = 0;
constexpr qulonglong kDefaultMaxDbSizeBytes       = 0;
constexpr uint kDefaultDbusApiAccess              = 0;

} // namespace settings::serializer::config
