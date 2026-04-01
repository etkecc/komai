# Settings Schema Migrations

This page defines how Komai should evolve settings formats when we introduce backward-incompatible changes.

## Current Groundwork

Migration plumbing exists in:

- `src/rust/src/settings/config/bridge.rs`
- `src/rust/src/settings/session.rs`
- `src/rust/src/settings/state.rs`

Current behavior:

- reads `meta.settings_schema_version` from `config.yml`, `state.yml`, and `session.yml` (`0` when missing)
- treats `1` as the current schema version for all settings YAML scopes
- runs an explicit step chain (`v0 -> v1`, then next steps as added over time)
- applies a foundational `v0 -> v1` migration step (schema version stamping only)
- exposes two load paths:
  - `SettingsController::load(...)`: read-only load (no file writes)
  - `SettingsController::loadAndMigrate(...)`: load + migration writeback when needed
- stamps brand-new profile `config.yml` on initial `loadAndMigrate(...)` with the current schema version
- stamps brand-new `state.yml` / `session.yml` on first creation during full persistence saves
- writes back migrated `config.yml` / `state.yml` / `session.yml` during `loadAndMigrate(...)` when existing files are older than the current schema
- warns when loading a settings file with a newer schema version than the app supports
- warns when a migration path is unsupported between known versions

The schema version key is:

- `meta.settings_schema_version`

`secrets.yml` is intentionally excluded for now because secrets persistence remains provider-dependent (`secret_service` vs `file`) and uses compatibility parsing rules.

## Intended Migration Strategy

When introducing a breaking settings change:

1. increase the current schema-version constant in the corresponding Rust settings loader
2. add deterministic `vN -> vN+1` migration logic in that Rust loader
3. keep migration steps small and composable
4. run migrations in order until current version is reached
5. keep existing keys readable for at least one compatibility cycle when practical

## Design Constraints

- migrations must be deterministic and idempotent
- unknown keys should be preserved whenever possible
- invalid input should fail safe (fallback to defaults, never crash)
- migration code should never require network access

## Testing Expectations

For each new migration step, add or update tests in the corresponding Rust settings module, and
extend `tests/StartupSettingsTest.cpp` only when the C++ startup/settings seam still needs direct
coverage, to cover:

- old format -> migrated runtime values
- invalid/malformed legacy values
- schema version update behavior
