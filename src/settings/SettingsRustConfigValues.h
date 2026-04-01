// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "komai-rust-cxxbridge/lib.h"

#include <QMap>
#include <QString>
#include <QStringList>

#include <optional>
#include <string>

namespace settings::rust_config_values {

inline const ::komai::rust::SettingsConfigValue *
find(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values, const char *key)
{
    for (const auto &value : values) {
        if (static_cast<std::string>(value.key) == key)
            return &value;
    }
    return nullptr;
}

inline QString
toString(const ::komai::rust::SettingsConfigValue &value)
{
    switch (value.kind) {
    case ::komai::rust::SettingsConfigValueKind::String:
        return QString::fromStdString(static_cast<std::string>(value.string_value));
    case ::komai::rust::SettingsConfigValueKind::Bool:
        return value.bool_value ? QStringLiteral("true") : QStringLiteral("false");
    case ::komai::rust::SettingsConfigValueKind::Int:
        return QString::number(value.int_value);
    case ::komai::rust::SettingsConfigValueKind::Double:
        return QString::number(value.double_value, 'g', 16);
    default:
        return {};
    }
}

inline std::optional<bool>
toBool(const ::komai::rust::SettingsConfigValue &value)
{
    switch (value.kind) {
    case ::komai::rust::SettingsConfigValueKind::Bool:
        return value.bool_value;
    case ::komai::rust::SettingsConfigValueKind::Int:
        if (value.int_value == 0 || value.int_value == 1)
            return value.int_value == 1;
        return std::nullopt;
    case ::komai::rust::SettingsConfigValueKind::Double:
        if (value.double_value == 0.0 || value.double_value == 1.0)
            return value.double_value == 1.0;
        return std::nullopt;
    case ::komai::rust::SettingsConfigValueKind::String: {
        const auto normalized = toString(value).trimmed().toLower();
        if (normalized == QLatin1String("true") || normalized == QLatin1String("yes") ||
            normalized == QLatin1String("on") || normalized == QLatin1String("1"))
            return true;
        if (normalized == QLatin1String("false") || normalized == QLatin1String("no") ||
            normalized == QLatin1String("off") || normalized == QLatin1String("0"))
            return false;
        return std::nullopt;
    }
    default:
        return std::nullopt;
    }
}

inline std::optional<int>
toInt(const ::komai::rust::SettingsConfigValue &value)
{
    switch (value.kind) {
    case ::komai::rust::SettingsConfigValueKind::Int:
        return value.int_value;
    case ::komai::rust::SettingsConfigValueKind::Double:
        return static_cast<int>(value.double_value);
    case ::komai::rust::SettingsConfigValueKind::String: {
        bool ok           = false;
        const auto parsed = toString(value).trimmed().toInt(&ok);
        return ok ? std::optional<int>(parsed) : std::nullopt;
    }
    default:
        return std::nullopt;
    }
}

inline std::optional<double>
toDouble(const ::komai::rust::SettingsConfigValue &value)
{
    switch (value.kind) {
    case ::komai::rust::SettingsConfigValueKind::Double:
        return value.double_value;
    case ::komai::rust::SettingsConfigValueKind::Int:
        return static_cast<double>(value.int_value);
    case ::komai::rust::SettingsConfigValueKind::String: {
        bool ok           = false;
        const auto parsed = toString(value).trimmed().toDouble(&ok);
        return ok ? std::optional<double>(parsed) : std::nullopt;
    }
    default:
        return std::nullopt;
    }
}

inline QString
readStringValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values,
                const char *key,
                const QString &defaultValue)
{
    const auto *value = find(values, key);
    if (!value)
        return defaultValue;

    const auto stringValue = toString(*value);
    return stringValue.isNull() ? defaultValue : stringValue;
}

inline bool
readBoolValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values,
              const char *key,
              bool defaultValue)
{
    const auto *value = find(values, key);
    if (!value)
        return defaultValue;

    return toBool(*value).value_or(defaultValue);
}

inline int
readIntValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values,
             const char *key,
             int defaultValue)
{
    const auto *value = find(values, key);
    if (!value)
        return defaultValue;

    return toInt(*value).value_or(defaultValue);
}

inline double
readDoubleValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values,
                const char *key,
                double defaultValue)
{
    const auto *value = find(values, key);
    if (!value)
        return defaultValue;

    return toDouble(*value).value_or(defaultValue);
}

inline QStringList
readStringListValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values,
                    const char *key,
                    const QStringList &defaultValue = {})
{
    const auto *value = find(values, key);
    if (!value || value->kind != ::komai::rust::SettingsConfigValueKind::StringList)
        return defaultValue;

    QStringList result;
    for (const auto &entry : value->string_list_value)
        result.push_back(QString::fromStdString(static_cast<std::string>(entry)));
    return result;
}

inline QMap<QString, QStringList>
readStringListMapValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values,
                       const char *key)
{
    const auto *value = find(values, key);
    if (!value || value->kind != ::komai::rust::SettingsConfigValueKind::StringListMap)
        return {};

    QMap<QString, QStringList> result;
    for (const auto &entry : value->string_list_map_value) {
        QStringList valuesForKey;
        for (const auto &item : entry.values)
            valuesForKey.push_back(QString::fromStdString(static_cast<std::string>(item)));
        result.insert(QString::fromStdString(static_cast<std::string>(entry.key)), valuesForKey);
    }
    return result;
}

} // namespace settings::rust_config_values
