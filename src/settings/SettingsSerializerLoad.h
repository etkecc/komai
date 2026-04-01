// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "komai-rust-cxxbridge/lib.h"

class UserSettings;

namespace settings::serializer {

/**
 * Rust-backed load helpers used by the staged settings controller.
 *
 * These remain separate from the broader serializer facade so config-load
 * bridge details do not leak into unrelated startup/application code.
 */
void
loadConfig(UserSettings &settings, const ::komai::rust::SettingsLoadedConfig &snapshot);
} // namespace settings::serializer
