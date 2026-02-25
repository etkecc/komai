// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMetaEnum>
#include <QString>
#include <QVariant>
#include <string>
#include <type_traits>
#include <utility>

#include "settings/ui/SettingDescriptor.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::ui::descriptor_value {

template<settings::core::SettingId Id, auto Get, typename CoreType, typename OutType = CoreType>
QVariant
getCoreValue()
{
    auto settings = UserSettings::instance();
    if (!settings)
        return {};

    if (const auto value = settings->coreStore().valueAs<CoreType>(Id); value.has_value()) {
        if constexpr (std::is_same_v<CoreType, std::string> && std::is_same_v<OutType, QString>)
            return QString::fromStdString(*value);
        else
            return *value;
    }

    return (settings.get()->*Get)();
}

template<auto Get>
QVariant
getSettingValue()
{
    auto settings = UserSettings::instance();
    if (!settings)
        return {};
    return (settings.get()->*Get)();
}

template<settings::core::SettingId Id, auto Get>
QVariant
getCoreBoolValue()
{
    return getCoreValue<Id, Get, bool>();
}

template<settings::core::SettingId Id, auto Get>
QVariant
getCoreIntValue()
{
    return getCoreValue<Id, Get, int>();
}

template<settings::core::SettingId Id, auto Get>
QVariant
getCoreDoubleValue()
{
    return getCoreValue<Id, Get, double>();
}

template<settings::core::SettingId Id, auto Get>
QVariant
getCoreStringValue()
{
    return getCoreValue<Id, Get, std::string, QString>();
}

template<auto Get>
QVariant
getSettingEnumValue()
{
    auto settings = UserSettings::instance();
    if (!settings)
        return {};
    return static_cast<int>((settings.get()->*Get)());
}

template<settings::core::SettingId Id, auto Get>
QVariant
getCoreEnumValue()
{
    auto settings = UserSettings::instance();
    if (!settings)
        return {};

    if (const auto value = settings->coreStore().valueAs<int>(Id); value.has_value())
        return *value;

    return static_cast<int>((settings.get()->*Get)());
}

template<typename Enum>
int
enumMaxValue()
{
    const auto meta = QMetaEnum::fromType<Enum>();
    return meta.isValid() ? (meta.keyCount() - 1) : -1;
}

template<auto Set, typename T>
using SetValueResult = decltype((std::declval<UserSettings *>()->*Set)(std::declval<T>()));

template<auto Set, typename T>
bool
setSettingValueImpl(UserSettings *settings, const T &castValue, std::true_type /*is_void*/)
{
    (settings->*Set)(castValue);
    return true;
}

template<auto Set, typename T>
bool
setSettingValueImpl(UserSettings *settings, const T &castValue, std::false_type /*is_void*/)
{
    return (settings->*Set)(castValue);
}

template<auto Set, typename T>
bool
setSettingValue(const QVariant &value)
{
    auto settings = UserSettings::instance();
    if (!settings)
        return false;

    T castValue{};
    if (!readSettingValue(value, castValue))
        return false;

    return setSettingValueImpl<Set, T>(
      settings.get(), castValue, std::is_void<SetValueResult<Set, T>>{});
}

template<auto Set, typename Enum>
using SetEnumResult = decltype((std::declval<UserSettings *>()->*Set)(std::declval<Enum>()));

template<auto Set, typename Enum>
bool
setSettingEnumValueImpl(UserSettings *settings, Enum enumValue, std::true_type /*is_void*/)
{
    (settings->*Set)(enumValue);
    return true;
}

template<auto Set, typename Enum>
bool
setSettingEnumValueImpl(UserSettings *settings, Enum enumValue, std::false_type /*is_void*/)
{
    return (settings->*Set)(enumValue);
}

template<auto Set, typename Enum>
bool
setSettingEnumValue(const QVariant &value)
{
    auto settings = UserSettings::instance();
    if (!settings)
        return false;

    int rawValue = 0;
    if (!readSettingValue(value, rawValue))
        return false;

    const auto meta = QMetaEnum::fromType<Enum>();
    if (!meta.isValid() || rawValue < 0 || rawValue >= meta.keyCount())
        return false;

    return setSettingEnumValueImpl<Set, Enum>(
      settings.get(), static_cast<Enum>(rawValue), std::is_void<SetEnumResult<Set, Enum>>{});
}

} // namespace settings::ui::descriptor_value
