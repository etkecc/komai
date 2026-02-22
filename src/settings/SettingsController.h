// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

namespace YAML {
class Node;
}

class UserSettings;
class QString;

namespace settings {

/**
 * Orchestrates settings persistence for a profile: computes profile-scoped
 * file/secret paths, loads user settings in staged order, persists all settings
 * layers, and handles auth/session reset operations.
 *
 * This class intentionally delegates format/transport details to
 * settings::persistence and settings::storage helpers; it coordinates the
 * workflow only.
 */
class SettingsController
{
public:
    enum class SavePolicy
    {
        /**
         * Save the full settings stack: config.yml, state.yml, session.yml and secrets.
         */
        Full,
        /**
         * Save only safe startup/user-facing config values (no session/state/secrets).
         */
        ConfigOnly,
    };

    /**
     * Load profile-local settings into the provided UserSettings instance.
     */
    void load(UserSettings &settings, std::optional<QString> profile);
    void load(UserSettings &settings, std::optional<QString> profile, const YAML::Node &configRoot);
    /**
     * Persist the provided UserSettings instance to all backing stores.
     */
    void save(UserSettings &settings, SavePolicy policy = SavePolicy::Full);
    /**
     * Remove stored auth/session material for the active profile and flush the
     * affected state.
     */
    void clearAuth(UserSettings &settings);
};

} // namespace settings
