// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include <yaml-cpp/yaml.h>

#include "settings/core/SettingsStore.h"

namespace settings::core::serializer {

enum class ValueKind
{
    Unknown,
    Bool,
    Int,
    Double,
    String,
    StringList,
};

[[nodiscard]] ValueKind
kindOf(const SettingsStore::Value &value);

[[nodiscard]] YAML::Node
toYamlNode(const SettingsStore::Value &value);

[[nodiscard]] std::optional<SettingsStore::Value>
fromYamlNode(const YAML::Node &node, ValueKind expectedKind);

[[nodiscard]] SettingsStore::Value
fromYamlNodeOrDefault(const YAML::Node &node, const SettingsStore::Value &defaultValue);

[[nodiscard]] SettingsStore::SetResult
setFromYamlNodeOrDefault(SettingsStore &store,
                         SettingId id,
                         const YAML::Node &node,
                         const SettingsStore::Value &defaultValue);

} // namespace settings::core::serializer
