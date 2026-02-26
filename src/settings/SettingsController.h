// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>

namespace YAML {
class Node;
}

class UserSettings;
class QString;

namespace spdlog {
class logger;
}

namespace settings {

struct ControllerLoggers
{
    std::shared_ptr<spdlog::logger> ui;
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
     * This path is read-only and never writes settings files.
     */
    void load(UserSettings &settings, std::optional<QString> profile);
    void load(UserSettings &settings, std::optional<QString> profile, const YAML::Node &configRoot);
    /**
     * Load settings, apply in-memory migration steps, and persist migrated roots
     * when needed (for example version bump writeback or first-file initialization).
     */
    void loadAndMigrate(UserSettings &settings, std::optional<QString> profile);
    void loadAndMigrate(UserSettings &settings,
                        std::optional<QString> profile,
                        const YAML::Node &configRoot);
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
