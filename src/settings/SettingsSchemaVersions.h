// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace settings::schema_versions {

inline constexpr int kCurrentSettingsSchemaVersion = 1;
inline constexpr int kCurrentConfigSchemaVersion   = kCurrentSettingsSchemaVersion;
inline constexpr int kCurrentStateSchemaVersion    = kCurrentSettingsSchemaVersion;
inline constexpr int kCurrentSessionSchemaVersion  = kCurrentSettingsSchemaVersion;

} // namespace settings::schema_versions
