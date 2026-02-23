// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/core/SettingsStore.h"

#include <optional>
#include <string>

namespace settings::core {

namespace {

std::optional<std::string>
validateIntRangeValue(const SettingsStore::Value &value, const SettingsStore::IntRange &range)
{
    const auto asInt = std::get_if<int>(&value);
    if (asInt == nullptr) {
        return std::string("expected integer value in range [") + std::to_string(range.minValue) +
               ", " + std::to_string(range.maxValue) + "]";
    }

    if (*asInt < range.minValue || *asInt > range.maxValue) {
        return std::string("value '") + std::to_string(*asInt) + "' out of range [" +
               std::to_string(range.minValue) + ", " + std::to_string(range.maxValue) + "]";
    }

    return std::nullopt;
}

} // namespace

bool
SettingsStore::hasValue(SettingId id) const
{
    return values_.contains(id);
}

std::size_t
SettingsStore::size() const
{
    return values_.size();
}

void
SettingsStore::clear()
{
    values_.clear();
}

bool
SettingsStore::erase(SettingId id)
{
    return values_.erase(id) > 0;
}

void
SettingsStore::setIntRangeConstraint(SettingId id, int minValue, int maxValue)
{
    intRangeConstraints_[id] = IntRange{.minValue = minValue, .maxValue = maxValue};
}

void
SettingsStore::clearConstraints()
{
    intRangeConstraints_.clear();
}

SettingsStore::SetResult
SettingsStore::setValue(SettingId id, Value value)
{
    if (id == SettingId::Unknown) {
        return SetResult{
          .success         = false,
          .changed         = false,
          .validationError = "setting id must not be Unknown",
        };
    }

    if (const auto rangeIt = intRangeConstraints_.find(id); rangeIt != intRangeConstraints_.end()) {
        if (const auto error = validateIntRangeValue(value, rangeIt->second); error.has_value()) {
            return SetResult{
              .success         = false,
              .changed         = false,
              .validationError = *error,
            };
        }
    }

    auto it = values_.find(id);
    if (it == values_.end()) {
        values_.emplace(id, std::move(value));
        return SetResult{
          .success         = true,
          .changed         = true,
          .validationError = {},
        };
    }

    if (it->second == value) {
        return SetResult{
          .success         = true,
          .changed         = false,
          .validationError = {},
        };
    }

    it->second = std::move(value);
    return SetResult{
      .success         = true,
      .changed         = true,
      .validationError = {},
    };
}

std::optional<SettingsStore::Value>
SettingsStore::value(SettingId id) const
{
    if (const auto it = values_.find(id); it != values_.end())
        return it->second;

    return std::nullopt;
}

} // namespace settings::core
