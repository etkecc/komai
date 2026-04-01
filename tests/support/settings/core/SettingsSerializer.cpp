// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/core/SettingsSerializer.h"

#include <string>
#include <vector>

namespace settings::core::serializer {

namespace {

template<typename T>
std::optional<SettingsStore::Value>
asYamlValue(const YAML::Node &node)
{
    if (!node.IsDefined() || node.IsNull())
        return std::nullopt;

    try {
        return SettingsStore::Value{node.as<T>()};
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<SettingsStore::Value>
asStringListYamlValue(const YAML::Node &node)
{
    if (!node.IsDefined() || node.IsNull())
        return std::nullopt;

    if (!node.IsSequence())
        return std::nullopt;

    SettingsStore::StringList values;
    values.reserve(node.size());
    try {
        for (const auto &entry : node) {
            values.push_back(entry.as<std::string>());
        }
    } catch (...) {
        return std::nullopt;
    }

    return SettingsStore::Value{std::move(values)};
}

} // namespace

ValueKind
kindOf(const SettingsStore::Value &value)
{
    if (std::holds_alternative<bool>(value))
        return ValueKind::Bool;
    if (std::holds_alternative<int>(value))
        return ValueKind::Int;
    if (std::holds_alternative<double>(value))
        return ValueKind::Double;
    if (std::holds_alternative<std::string>(value))
        return ValueKind::String;
    if (std::holds_alternative<SettingsStore::StringList>(value))
        return ValueKind::StringList;
    return ValueKind::Unknown;
}

YAML::Node
toYamlNode(const SettingsStore::Value &value)
{
    YAML::Node node;
    if (const auto *asBool = std::get_if<bool>(&value)) {
        node = *asBool;
    } else if (const auto *asInt = std::get_if<int>(&value)) {
        node = *asInt;
    } else if (const auto *asDouble = std::get_if<double>(&value)) {
        node = *asDouble;
    } else if (const auto *asString = std::get_if<std::string>(&value)) {
        node = *asString;
    } else if (const auto *asList = std::get_if<SettingsStore::StringList>(&value)) {
        node = YAML::Node(YAML::NodeType::Sequence);
        for (const auto &entry : *asList)
            node.push_back(entry);
    } else {
        node = YAML::Node();
    }

    return node;
}

std::optional<SettingsStore::Value>
fromYamlNode(const YAML::Node &node, ValueKind expectedKind)
{
    switch (expectedKind) {
    case ValueKind::Bool:
        return asYamlValue<bool>(node);
    case ValueKind::Int:
        return asYamlValue<int>(node);
    case ValueKind::Double:
        return asYamlValue<double>(node);
    case ValueKind::String:
        return asYamlValue<std::string>(node);
    case ValueKind::StringList:
        return asStringListYamlValue(node);
    case ValueKind::Unknown:
        return std::nullopt;
    }
    return std::nullopt;
}

SettingsStore::Value
fromYamlNodeOrDefault(const YAML::Node &node, const SettingsStore::Value &defaultValue)
{
    if (!node.IsDefined() || node.IsNull())
        return defaultValue;

    const auto parsed = fromYamlNode(node, kindOf(defaultValue));
    if (!parsed.has_value())
        return defaultValue;

    return *parsed;
}

SettingsStore::SetResult
setFromYamlNodeOrDefault(SettingsStore &store,
                         SettingId id,
                         const YAML::Node &node,
                         const SettingsStore::Value &defaultValue)
{
    return store.setValue(id, fromYamlNodeOrDefault(node, defaultValue));
}

} // namespace settings::core::serializer
