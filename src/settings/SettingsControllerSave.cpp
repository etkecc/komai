// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsController.h"
#include "SettingsControllerInternal.h"
#include "komai-rust-cxxbridge/ffi.h"

#include <utility>

#include "logging/Logging.h"

#include "profile/Paths.h"
#include "settings/SettingsSerializer.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {

::rust::Vec<::komai::rust::SettingsStringMapEntry>
toRustStringMapEntries(const QMap<QString, QString> &entries)
{
    ::rust::Vec<::komai::rust::SettingsStringMapEntry> rustEntries;
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        rustEntries.push_back({
          .key   = it.key().toStdString(),
          .value = it.value().toStdString(),
        });
    }
    return rustEntries;
}

::komai::rust::SettingsProfileHandle *
ensureRustSettingsProfileHandle(UserSettings &settings, bool includeSession)
{
    if (auto *handle = settings.rustSettingsProfileHandle(); handle != nullptr)
        return handle;

    auto handle = ::komai::rust::settings_open_profile_handle_for_profile(
      settings.profileId().toStdString(), includeSession);
    settings.setRustSettingsProfileHandle(std::move(handle));
    return settings.rustSettingsProfileHandle();
}

void
logFlushOutcome(QStringView profileId, const char *label, bool attempted, bool saved)
{
    if (!attempted)
        return;

    if (saved) {
        settings::activeLoggers().ui->debug(
          "Saved {} for profile '{}'",
          label,
          app_paths::normalizedProfileId(profileId).toStdString());
        return;
    }

    settings::activeLoggers().ui->warn("Failed to save {} for profile '{}'",
                                       label,
                                       app_paths::normalizedProfileId(profileId).toStdString());
}

void
logFlushOutcome(QStringView profileId, const ::komai::rust::SettingsProfileFlushResult &result)
{
    logFlushOutcome(profileId, "config", result.config_attempted, result.config_saved);
    logFlushOutcome(profileId, "session", result.session_attempted, result.session_saved);
    logFlushOutcome(profileId, "secrets", result.secrets_attempted, result.secrets_saved);
    logFlushOutcome(profileId, "state", result.state_attempted, result.state_saved);
}

} // namespace

void
settings::SettingsController::save(UserSettings &settings, SavePolicy policy)
{
    if (!settings.hasResolvedProfilePaths()) {
        settings.applyProfilePathState(settings.profileId());
    }

    syncCoreStoreFromSettings(settings);

    if (policy == SavePolicy::ConfigOnly) {
        auto *profileHandle = ensureRustSettingsProfileHandle(settings, false);
        settings::serializer::stageConfig(
          settings, settings.usesFileSecretsProvider(), *profileHandle);
        logFlushOutcome(
          settings.profileId(),
          ::komai::rust::settings_profile_flush(*profileHandle, true, false, false, false));
    } else if (policy == SavePolicy::StateOnly) {
        auto *profileHandle = ensureRustSettingsProfileHandle(settings, false);
        settings::serializer::stageState(settings, *profileHandle);
        logFlushOutcome(
          settings.profileId(),
          ::komai::rust::settings_profile_flush(*profileHandle, false, false, false, true));
    } else if (policy == SavePolicy::Full) {
        auto *profileHandle = ensureRustSettingsProfileHandle(settings, true);
        settings::serializer::stageConfig(
          settings, settings.usesFileSecretsProvider(), *profileHandle);
        settings::serializer::stageSession(settings, *profileHandle);
        ::komai::rust::settings_profile_replace_secrets_payload(
          *profileHandle,
          settings.accessToken().toStdString(),
          toRustStringMapEntries(settings.secretsMap()));
        settings::serializer::stageState(settings, *profileHandle);
        logFlushOutcome(
          settings.profileId(),
          ::komai::rust::settings_profile_flush(*profileHandle, true, true, true, true));
    }
    syncCoreStoreFromSettings(settings);
}

void
settings::SettingsController::clearAuth(UserSettings &settings)
{
    activeLoggers().ui->info("Clearing persisted session auth/identity for profile '{}'",
                             app_paths::normalizedProfileId(settings.profileId()).toStdString());

    settings.clearAuthInMemory();
    auto *profileHandle = ensureRustSettingsProfileHandle(settings, false);
    if (!::komai::rust::settings_profile_clear_auth(*profileHandle)) {
        activeLoggers().ui->warn(
          "Failed to clear persisted session auth for profile '{}', keeping data to avoid "
          "accidental data loss",
          app_paths::normalizedProfileId(settings.profileId()).toStdString());
    }
    settings::serializer::stageState(settings, *profileHandle);
    logFlushOutcome(
      settings.profileId(),
      ::komai::rust::settings_profile_flush(*profileHandle, false, false, false, true));
    syncCoreStoreFromSettings(settings);
}
