// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerConfigSchema.h"

#include <QString>

#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::serializer::config {

std::span<const BoolSettingDescriptor>
boolConfigSettings()
{
    return {};
}

std::span<const IntSettingDescriptor>
intConfigSettings()
{
    return {};
}

std::span<const UintSettingDescriptor>
uintConfigSettings()
{
    return {};
}

std::span<const ULongLongSettingDescriptor>
ulonglongConfigSettings()
{
    return {};
}

std::span<const DoubleSettingDescriptor>
doubleConfigSettings()
{
    return {};
}

std::span<const StringSettingDescriptor>
stringConfigSettings()
{
    return {};
}

} // namespace settings::serializer::config
