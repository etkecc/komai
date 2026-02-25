# Settings Architecture

This document describes Komai's current settings and secret persistence architecture.

## Overview

Komai keeps a runtime `UserSettings` API (Qt properties/signals) and persists it across multiple profile-scoped stores (`config.yml`, `state.yml`, `session.yml`, `secrets.yml`).

Responsibility split:

- `UserSettings` owns the in-memory settings model and exposes QML properties/signals.
- `settings::SettingsController` owns profile orchestration: path setup, staged loading, save orchestration, and auth/session clearing.
- `settings::staged_load_plan` defines the load stages and how secrets-provider selection affects them.
- `settings::persistence` handles secret-provider plumbing and serializing secret payloads.
- `settings::storage` owns low-level file/keyring/YAML operations and profile path resolution.
- `settings::migrations` owns schema-versioned config migration logic.
- `settings::startup` owns startup-only reads that must happen before `Q(Core)Application` is created.

Current ownership map:

- `src/settings/ui/facade/UserSettingsPage.h/.cpp`
  - `UserSettings` singleton façade (`Q_PROPERTY`, `Q_INVOKABLE`, `load()`, `save()`, `initialize()`).
  - Maintains runtime settings state and delegates persistence orchestration to `settings::SettingsController`.
- `src/settings/ui/UserSettingsModel.h/.cpp` (`UserSettingsModel`)
  - QML list-model adapter and settings schema metadata mapping (`SettingMeta` rows, roles, and delegate types).
- `src/settings/ui/SettingDescriptor.h/.cpp`
  - Descriptor primitives and shared SettingId-to-row lookup cache.
- `src/settings/ui/SettingDescriptorTable.cpp`
  - Descriptor table construction (`settingsTable`) and row/callback registration.
- `src/settings/ui/SettingDescriptorValueAccessors.h`
  - Shared typed value getter/setter templates used by descriptor rows.
- `src/settings/ui/SettingDescriptorRowMacros.inc`
  - Shared row-construction macros used by settings row include files.
- `src/settings/ui/UserSettingsModelConnections.cpp`
  - Dedicated wiring for settings-row data-change signals (keeps `UserSettingsModel` focused on role/data logic).
- `src/settings/ui/SettingInputValidation.h/.cpp`
  - Centralized model-input validation for settings edits and SettingId-backed role updates.
- `src/settings/ui/SettingRoleData.h/.cpp`
  - SettingId-backed special-role adapters (for example theme variant roles and encryption key-status roles).
- `src/settings/ui/SessionKeyActions.h/.cpp`
  - Session import/export and cross-signing action handlers used by settings-row buttons.
- `src/settings/ui/facade/UserSettingsSetters*.cpp`
  - Mutator behavior for setting updates, including inline validation/normalization and immediate side effects.
- `src/settings/ui/facade/UserSettingsTheme.cpp`
  - Theme switching and runtime theme application.
- `src/settings/SettingsSerializer*`
  - Settings YAML serialization/deserialization and normalized value conversion.
  - Enum token conversion and adapter registry are split between
    `SettingsSerializerConfigConverters.cpp` and `SettingsSerializerConfigEnumTokenAdapters.cpp`.
- `src/settings/ui/facade/UserSettingsSessionSettings.cpp`
  - Session/auth/session-file related helpers.
- `src/settings/SettingsController.*`
  - Profile orchestration and persistence pipeline orchestration.
- `src/settings/SettingsPersistence.*`
  - Secret provider strategy plus secret payload load/save/cleanup behavior.
- `src/settings/SettingsStorage.*`
  - Profile pathing and direct file/secure-store I/O primitives.
- `src/settings/SettingsMigrations.*`
  - Config schema-version migration entry point and migration step chain.
- `src/settings/StartupSettings.*`, `src/settings/core/StartupConfig.*`
  - Bootstrap profile config preloading for startup-time scale-factor handling.
- `src/settings/core/SettingDefinition.h`, `src/settings/core/SettingsDefinitions.h`, `src/settings/core/SettingsConstraints.h`
  - Canonical persisted setting schema (`SettingId`, scope, key, restart policy) and schema-level value constraints.

### Responsibility map

- `src/settings/ui/facade/UserSettingsPage.h/.cpp` (`UserSettings`)
  - Runtime settings API: `Q_PROPERTY`, signals, and getters/setters for UI consumption.
  - In-memory defaults and current values.
  - Delegates profile load/save/clear orchestration to `settings::SettingsController`.

Settings flow:

- startup:
  - `main.cpp` computes profile from args, reads pre-UI scale factor from `config.yml`, and initializes
    `UserSettings` before constructing visual UI (`MainWindow`).
  - `main.cpp` reads a startup snapshot via `settings::startup::readStartupConfig(...)` and passes the
    loaded `config.yml` node into `settings::SettingsController` to avoid duplicate config parsing.
  - `ThemeRegistry::initialize()` and logging are set up before UI creation.
  - `UserSettings::load(profile)` calls `settings::SettingsController::load(...)`.
  - Controller resolves profile paths, runs staged config/session/secret/state load, then applies theme.
- runtime mutation:
  - QML delegates mutate through `UserSettingsModel` -> `UserSettings`.
  - `UserSettings` updates in-memory values and persists via controller orchestration in `save()`.
- pre-login settings access:
  - settings page can be opened even without an active session.
  - session-dependent account details are guarded by `UserSettings::hasActiveSession`.
  - the Account tab is disabled while signed out, with a signed-out fallback view.
- exit/logout:
  - `UserSettings::clearAuth()` delegates to controller to erase auth/session data and secrets.

- `src/settings/SettingsController.h/.cpp` (`settings::SettingsController`)
  - Profile lifecycle orchestration (`load`, `save`, `clearAuth`).
  - Profile path resolution and stage sequencing.
  - Top-level read/write ordering and event notifications.
- `src/settings/SettingsPersistence.h/.cpp`
  - Secret provider strategy and secrets payload handling (`secret_service` vs `file`).
  - Payload load/save/cleanup for auth and per-profile secret maps.
- `src/settings/SettingsStorage.h/.cpp`
  - Profile path helpers (`configFilePathForProfile`, etc.).
  - YAML file IO (`loadYamlFile`, `writeYamlFile`).
  - Keychain wrappers (`readSecureValue`, `writeSecureValue`, `deleteSecureValue`).
- `src/settings/SettingKeys.h`
  - Canonical settings keys for all persisted scopes (config/state/session/secrets/runtime).
- `src/settings/StagedLoadPlan.h`
  - Startup stage ordering and secrets-provider dispatch plan.
- `src/settings/ui/UserSettingsModel.cpp` and `src/settings/ui/rows/UserSettingsModel*.inc` (`UserSettingsModel`)
  - UI adapter that maps setting metadata to rows, roles, and tab-filtered models.
- `src/settings/ui/SettingDescriptor.h/.cpp`
  - Descriptor metadata contract (`SettingMeta`) and SettingId row lookup helper.
- `src/settings/ui/SettingDescriptorTable.cpp`
  - Central descriptor table definition and callback/value bindings.
- `src/settings/ui/SettingDescriptorValueAccessors.h`
  - Shared value conversion helpers for descriptor row bindings.
- `src/settings/ui/SettingDescriptorRowMacros.inc`
  - Shared row-construction macros for settings row includes.
- `src/settings/ui/UserSettingsModelConnections.cpp`
  - Row-to-signal binding setup (`dataChanged` emission policy for settings dependencies).
- `src/settings/ui/SessionKeyActions.cpp`
  - Session key import/export and cross-signing request/download action handling.
- `src/settings/ui/SettingDescriptorCallbacks*.inc`
  - Descriptor-bound value helpers used by `settingsTable` row entries.
- `src/settings/ui/SettingRoleData.cpp`
  - SettingId-based special role handlers (theme variant and key-status `good` role values).

Profile directory:

```text
~/.config/komai/profiles/<profile-id>/
```

`<profile-id>` is the profile name/identifier passed via `-p`.

Files:

- `config.yml` - durable preferences and non-secret advanced settings
- `state.yml` - runtime/layout/window state
- `session.yml` - account/session metadata (non-secret)
- `secrets.yml` - file-provider fallback secrets (only when `secrets.provider=file`)

Default profile id: `default`.

Reference examples:

- [Profile config.yml](examples/profile/config.yml)
- [Profile state.yml](examples/profile/state.yml)
- [Profile session.yml](examples/profile/session.yml)
- [Profile secrets.yml](examples/profile/secrets.yml)

## Persistence Model

Primary implementation files:

- `src/settings/ui/facade/UserSettingsPage.cpp`
- `src/settings/ui/facade/UserSettingsPage.h`
- `src/settings/ui/UserSettingsModel.cpp`
- `src/settings/ui/UserSettingsModelConnections.cpp`
- `src/settings/ui/SettingDescriptor.h`
- `src/settings/ui/SettingDescriptor.cpp`
- `src/settings/ui/SettingDescriptorTable.cpp`
- `src/settings/ui/SettingDescriptorValueAccessors.h`
- `src/settings/ui/SettingDescriptorRowMacros.inc`
- `src/settings/ui/SettingDescriptorRowMacrosUndef.inc`
- `src/settings/ui/rows/UserSettingsModel*.inc`
- `src/settings/ui/SettingInputValidation.h`
- `src/settings/ui/SettingInputValidation.cpp`
- `src/settings/ui/SettingRoleData.h`
- `src/settings/ui/SettingRoleData.cpp`
- `src/settings/ui/SessionKeyActions.h`
- `src/settings/ui/SessionKeyActions.cpp`
- `src/settings/ui/facade/UserSettingsSettersCore.cpp`
- `src/settings/ui/facade/UserSettingsSettersLayout.cpp`
- `src/settings/ui/facade/UserSettingsSettersMisc.cpp`
- `src/settings/ui/facade/UserSettingsSettersUi.cpp`
- `src/settings/SettingsSerializerConfig.cpp`
- `src/settings/SettingsSerializerConfigEnumTokenAdapters.cpp`
- `src/settings/SettingsSerializerSession.cpp`
- `src/settings/SettingsSerializerState.cpp`
- `src/settings/SettingsSerializer.h`
- `src/settings/ui/facade/UserSettingsSessionSettings.cpp`
- `src/settings/ui/facade/UserSettingsTheme.cpp`
- `src/settings/SettingsController.cpp`
- `src/settings/SettingsController.h`
- `src/settings/SettingsPersistence.cpp`
- `src/settings/SettingsPersistence.h`
- `src/settings/SettingsStorage.cpp`
- `src/settings/SettingsStorage.h`
- `src/settings/core/SettingDefinition.h`
- `src/settings/core/SettingsDefinitions.h`
- `src/settings/core/SettingsConstraints.h`

Persistence entry points:

- `UserSettings::load(...)` delegates to `settings::SettingsController`.
- `UserSettings::loadConfigYaml(...)` / `saveConfigYaml()`
- `UserSettings::loadSessionYaml(...)` / `saveSessionYaml()`
- `UserSettings::saveSecretsYaml()`
- `UserSettings::loadStateYaml(...)` / `saveStateYaml()`
- `settings::SettingsController::save(...)`
- `settings::SettingsController::clearAuth(...)`

YAML key hierarchy is nested/dotted (for example `timeline.messages.layout.style`).

## Staged Load Order

Load order is intentionally staged:

1. `config.yml` (resolve `secrets.provider`)
2. `session.yml` (account metadata)
3. secrets source
   - `secret_service`: read secrets from secure backend
   - `file`: read secrets from `secrets.yml`
4. `state.yml` (runtime/layout)

This prevents secret-source ambiguity and allows provider selection before secret reads.

Config migration note:

- after loading `config.yml`, `settings::migrations::migrateConfigRoot(...)` runs before config values are applied.
- future schema versions are loaded best-effort with a warning.
- unsupported migration paths are also warned and treated as partial migrations.

Startup nuance:

- Before `QApplication` is created, Komai reads `ui.scale.factor` from profile `config.yml` and sets
  `QT_SCALE_FACTOR` only if the environment variable is not already set.

## Secret Providers

`secrets.provider` values:

- `secret_service` (default)
- `file`

Behavior:

- `secret_service`
  - `session.auth.access_token` and `session.secrets` are stored in secure backend only
  - `session.yml` stores non-secret session metadata
  - `secrets.yml` is absent/unused
- `file`
  - `auth.access_token` and `secrets` are stored in `secrets.yml`
  - cache-side secret storage also uses this fallback map via `UserSettings::secret/setSecret/removeSecret`

`secrets.yml` write permissions are restricted to owner read/write.

## Secure Backend Key IDs

Key generation is centralized in:

- `src/ProfileSecrets.h`
- `src/ProfileSecrets.cpp`

Profile hash:

- `profile_hash = hex(sha256(normalized_profile_id))`
- normalized profile id: empty/default -> `default`, otherwise profile id string
- default profile hash convenience value:
  - `37a8eec1ce19687d132fe29051dca629d164e2c4958ba141d5f4133a33f0688f`

Namespaces:

- settings secrets: `komai.<profile_hash>.settings.<key>`
- local crypto secrets: `komai.<profile_hash>.local_crypto.<key>`
- matrix secrets: `komai.<profile_hash>.matrix.<key>`

Examples:

- `komai.<profile_hash>.settings.session.auth.access_token`
- `komai.<profile_hash>.settings.session.secrets`
- `komai.<profile_hash>.local_crypto.pickle_secret`

Legacy Base64 profile-hash IDs are intentionally not used.

## File-Provider Secret Format

When `secrets.provider=file`, `secrets.yml` includes:

- `auth.access_token: <token>`
- `secrets:` map where keys are full secret IDs (`komai.<profile_hash>.<scope>.<name>`)

This keeps fallback and secure-backend key identity consistent.

## Cache/Crypto Integration

`src/Cache.cpp` uses the same key-id helper as `UserSettings`.

- Local pickle secret is stored under `local_crypto` scope.
- Matrix secret names are stored under `matrix` scope.
- In file mode, these values are stored in `secrets.yml` under the `secrets` map.

## 3-Layer Naming Audit

To audit naming alignment across persisted settings definitions, runtime getters, and persisted keys:

- run `just settings-3-layer-mapping-generate`
- run `just settings-3-layer-mapping-check` for non-mutating drift checks
- inspect [`3-layer-mapping.md`](3-layer-mapping.md)

The generated report includes:

- full mapping rows (`SettingId` <-> runtime getter expression <-> `SettingKey` <-> dotted key)
- a heuristic mismatch summary (`lcfirst(SettingId)` vs runtime getter name)

## Notes

- Canonical references are:
  - [User Settings Guide](../../settings.md)
  - [Storage Guide](../../storage.md)
  - [this architecture document](README.md)
  - [settings migration playbook](migrations.md)
  - [Storage Architecture](../storage.md)
  - [settings examples](examples/profile/)
  - [nheko to Komai settings mapping](../differences-from-nheko/settings-mapping.md)
