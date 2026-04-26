# Settings Architecture

This document describes Komai's current settings and secret persistence architecture.

## Overview

Komai keeps a runtime `UserSettings` API (Qt properties/signals) and persists it across multiple profile-scoped stores (`config.yml`, `state.yml`, `session.yml`, `secrets.yml`).

Responsibility split:

- `UserSettings` owns the in-memory settings model and exposes QML properties/signals.
- `settings::SettingsController` owns profile orchestration and delegates load/save/clear operations to the resident Rust settings profile handle.
- `settings::storage` owns low-level file/keyring/text operations and profile path resolution.
- `settings::profile`, `settings::config`, `settings::session`, `settings::state`, and `settings::secrets`
  on the Rust side own schema-versioned load/migrate/save behavior for profile-scoped settings files.
- live schema-versioned migration logic for `config.yml`, `state.yml`, and `session.yml` now lives in
  the Rust settings loaders; the remaining C++ migration helpers are test-support only.
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
- `src/settings/SettingsStorage.*`
  - Profile pathing and direct file/secure-store I/O primitives.
- `src/rust/src/settings/profile/*`, `src/rust/src/settings/config/*`,
  `src/rust/src/settings/session/*`, `src/rust/src/settings/state/*`,
  `src/rust/src/settings/secrets/*`
  - Live settings schema-version load/migration/save entry points and profile-handle ownership.
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
  - `main.cpp` reads a typed startup snapshot via `settings::startup::readStartupConfig(...)` for
    pre-Qt bootstrap values only; full settings load still happens later through
    `settings::SettingsController`.
  - `ThemeRegistry::initialize()` and logging are set up before UI creation.
  - `UserSettings::load(profile)` calls `settings::SettingsController::loadAndMigrate(...)`
    (explicit migration writeback path).
  - Controller opens a Rust profile handle, prepares the loaded profile bundle, applies config/session/secrets/state, then applies theme.
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
  - Top-level read/write ordering, Rust profile-handle coordination, and event notifications.
- `src/settings/SettingsStorage.h/.cpp`
  - Profile path helpers (`configFilePathForProfile`, etc.).
  - Text file IO (`readTextFile`, `writeTextFile`).
  - Keychain wrappers (`readSecureValue`, `writeSecureValue`, `deleteSecureValue`).
- `src/settings/SettingKeys.h`
  - Canonical settings keys for all persisted scopes (config/state/session/secrets/runtime).
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

Schema version key for versioned settings files:

- `meta.settings_schema_version` (currently `1` for `config.yml`, `state.yml`, `session.yml`)

Profile id normalization:

- empty profile id -> `default`
- `default` -> `default`
- any other profile id -> unchanged
- valid non-empty profile ids are restricted to ASCII `[A-Za-z_][A-Za-z0-9_-]*`
- this keeps profile ids usable directly in D-Bus service names and Linux desktop-entry IDs

Examples:

- `komai` -> `default`
- `komai -p etke` -> `etke`

Reference examples:

- [Profile config.yml](../../user-guide/settings/examples/profile/config.yml)
- [Profile state.yml](../../user-guide/settings/examples/profile/state.yml)
- [Profile session.yml](../../user-guide/settings/examples/profile/session.yml)
- [Profile secrets.yml](../../user-guide/settings/examples/profile/secrets.yml)

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
- `src/settings/SettingsStorage.cpp`
- `src/settings/SettingsStorage.h`
- `src/settings/core/SettingDefinition.h`
- `src/settings/core/SettingsDefinitions.h`
- `src/settings/core/SettingsConstraints.h`

Persistence entry points:

- `settings::startup::readStartupConfig(...)`
- `settings::SettingsController::load(...)`
- `settings::SettingsController::loadAndMigrate(...)`
- `settings::SettingsController::save(...)`
- `settings::SettingsController::clearAuth(...)`

YAML key hierarchy is nested/dotted (for example `timeline.messages.style`).

## Load Order

Load order is still intentionally sequenced, but that sequencing now lives in the Rust profile loader/handle rather than a separate C++ staged-load plan:

1. `config.yml` (resolve `secrets.provider`)
2. `session.yml` (account metadata)
3. secrets source
   - `secret_service`: read secrets from secure backend
   - `file`: read secrets from `secrets.yml`
4. `state.yml` (runtime/layout)

This prevents secret-source ambiguity and allows provider selection before secret reads.

Settings migration note:

- after loading `config.yml`, `state.yml`, and `session.yml`, the matching Rust snapshot loaders run
  migration stamping/normalization before values are applied.
- future schema versions are loaded best-effort with a warning.
- unsupported migration paths are also warned and treated as partial migrations.

Startup nuance:

- Before `QApplication` is created, Komai reads `ui.scale.factor` from profile `config.yml` and sets
  `QT_SCALE_FACTOR` only if the environment variable is not already set.

## Secret Providers

`secrets.provider` values:

- `secret_service` (default)
- `file`

Startup provider policy:

- On first profile creation, Komai probes secure backend availability and writes the chosen provider to `config.yml` (`secret_service` when available, otherwise `file`).
- If the profile is still pre-auth (no persisted session identity), startup re-evaluates secure backend availability and can switch providers before authentication.
- Once a profile has an active session, startup keeps the configured provider and does not auto-fallback between providers.

Behavior:

- `secret_service`
  - `session.secrets` is stored in secure backend
  - access token is embedded in that secrets payload under internal key `__session.access_token`
  - `session.yml` stores non-secret session metadata
  - `secrets.yml` is absent/unused
- `file`
  - `secrets` map is stored in `secrets.yml`
  - access token is embedded under internal key `__session.access_token`
  - cache-side secret storage also uses this fallback map via `UserSettings::secret/setSecret/removeSecret`

`secrets.yml` write permissions are restricted to owner read/write.

## Secure Backend Key IDs

Key generation is centralized in:

- `src/ProfileSecrets.h`
- `src/ProfileSecrets.cpp`

Profile id:

- `normalized_profile_id`: empty/default -> `default`, otherwise profile id string

Environment tag:

`<env-tag>` isolates secrets across packaging formats depending on the filesystem
config paths they use, so that each unique filesystem path produces a unique keyring prefix.

- `native` — standard (non-sandboxed) builds on any platform, including AppImage (config root ends in `/.config/komai`, `/Library/Preferences/komai`, or `/AppData/Local/komai`)
- `flatpak` — config root ends in `/.var/app/cc.etke.komai/config/komai`
- `snap` — config root contains `/snap/` and ends in `/.config/komai`
- 6-char hex hash — any other config root path

Namespaces:

- settings secrets: `komai.<env-tag>.<profile-id>.settings.<key>`
- local crypto secrets: `komai.<env-tag>.<profile-id>.local_crypto.<key>`
- matrix secrets: `komai.<env-tag>.<profile-id>.matrix.<key>`

Examples:

- `komai.native.default.settings.session.secrets`
- `komai.flatpak.default.local_crypto.pickle_secret`
- `komai.snap.work.matrix.cross_signing_master`

## File-Provider Secret Format

When `secrets.provider=file`, `secrets.yml` includes:

- `secrets:` map with:
  - internal `__session.access_token` for session auth
  - full secret IDs (`komai.<env-tag>.<profile-id>.<scope>.<name>`) for regular secrets

This keeps fallback and secure-backend key identity consistent.

## Cache/Crypto Integration

`src/matrix/backend/MatrixSessionSecrets.cpp` uses the same key-id helper as
`UserSettings`.

- Local pickle secret is stored under `local_crypto` scope.
- Matrix secret names are stored under `matrix` scope.
- In file mode, these values are stored in `secrets.yml` under the `secrets` map.

## Notes

- Canonical references are:
  - [User Settings Guide](../../user-guide/settings/README.md)
  - [Storage Guide](../../user-guide/operations/storage.md)
  - [this architecture document](README.md)
  - [settings migration playbook](migrations.md)
  - [Storage Architecture](../storage.md)
  - [settings examples](../../user-guide/settings/examples/profile/)
  - [nheko to Komai settings mapping](../differences-from-nheko/settings-mapping.md)
