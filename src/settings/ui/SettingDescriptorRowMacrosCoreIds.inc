// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Keep macro argument shape compatible with existing row includes while we only
// consume SettingId here. Scope/key/restart metadata comes from core definitions.
#define CORE_SETTING_WITH_ID(id, scope, key, restart) settings::core::SettingId::id

#define CORE_CONFIG_RESTART(key) CORE_SETTING_WITH_ID(Unknown, Config, key, true)
#define CORE_CONFIG_ID(id, key) CORE_SETTING_WITH_ID(id, Config, key, false)
#define CORE_CONFIG_RESTART_ID(id, key) CORE_SETTING_WITH_ID(id, Config, key, true)
#define CORE_RUNTIME_ID(id) CORE_SETTING_WITH_ID(id, Runtime, nullptr, false)
