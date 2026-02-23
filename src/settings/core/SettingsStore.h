// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "settings/core/SettingDefinition.h"

namespace settings::core {

class SettingsStore
{
public:
    using StringList = std::vector<std::string>;
    using Value      = std::variant<std::monostate, bool, int, double, std::string, StringList>;

    struct SetResult
    {
        bool success{false};
        bool changed{false};
        std::string validationError;
    };

    struct SettingIdHash
    {
        std::size_t operator()(SettingId id) const
        {
            using Underlying = std::underlying_type_t<SettingId>;
            return static_cast<std::size_t>(static_cast<Underlying>(id));
        }
    };

    [[nodiscard]] bool hasValue(SettingId id) const;
    [[nodiscard]] std::size_t size() const;
    void clear();
    [[nodiscard]] bool erase(SettingId id);

    [[nodiscard]] SetResult setValue(SettingId id, Value value);
    [[nodiscard]] std::optional<Value> value(SettingId id) const;

    template<typename T>
    [[nodiscard]] std::optional<T> valueAs(SettingId id) const
    {
        static_assert(!std::is_reference_v<T>);

        const auto rawValue = value(id);
        if (!rawValue.has_value())
            return std::nullopt;

        if (const auto typed = std::get_if<T>(&*rawValue))
            return *typed;

        return std::nullopt;
    }

    template<typename T>
    [[nodiscard]] SetResult set(SettingId id, T value)
    {
        return setValue(id, Value{std::move(value)});
    }

private:
    std::unordered_map<SettingId, Value, SettingIdHash> values_;
};

} // namespace settings::core
