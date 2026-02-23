// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/core/SettingsStore.h"

namespace settings::core {

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
