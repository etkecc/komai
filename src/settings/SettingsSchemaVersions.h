// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace settings::schema_versions {

// Per-file current schema versions. Keep these in lockstep with the
// matching `CURRENT_*_SCHEMA_VERSION` constants on the Rust side
// (`src/rust/src/settings/{config,state,session}/`).
inline constexpr int kCurrentConfigSchemaVersion  = 3;
inline constexpr int kCurrentStateSchemaVersion   = 1;
inline constexpr int kCurrentSessionSchemaVersion = 1;

} // namespace settings::schema_versions
