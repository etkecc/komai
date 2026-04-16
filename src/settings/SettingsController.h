// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>

class UserSettings;
class QString;

namespace komai::logging {
class Logger;
}

namespace settings {

struct ControllerLoggers
{
    std::shared_ptr<komai::logging::Logger> ui;
};

void
setLoggers(ControllerLoggers loggers);
const ControllerLoggers &
activeLoggers();

/**
 * Orchestrates settings persistence for a profile: computes profile-scoped
 * file/secret paths, loads user settings in staged order, persists all settings
 * layers, and handles auth/session reset operations.
 *
 * This class intentionally delegates format/persistence details to the Rust
 * settings owner plus the narrow C++ storage/keychain adapters; it coordinates
 * the workflow only.
 */
class SettingsController
{
public:
    enum class LoadPolicy
    {
        /**
         * Load full profile state: config/session/secrets/state.
         */
        Full,
        /**
         * Load startup UI/runtime state only (config + state), skipping
         * session/secrets/auth material.
         */
        ConfigAndStateOnly,
    };

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
        /**
         * Save runtime state only (state.yml), without touching config/session/secrets.
         */
        StateOnly,
    };

    /**
     * Load profile-local settings into the provided UserSettings instance.
     * This path is read-only and never writes settings files.
     */
    void load(UserSettings &settings,
              std::optional<QString> profile,
              LoadPolicy policy = LoadPolicy::Full);
    /**
     * Load settings, apply in-memory migration steps, and persist migrated roots
     * when needed (for example version bump writeback or first-file initialization).
     */
    void loadAndMigrate(UserSettings &settings,
                        std::optional<QString> profile,
                        LoadPolicy policy = LoadPolicy::Full);
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
