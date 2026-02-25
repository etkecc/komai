# Settings Schema Migrations

This page defines how Komai should evolve settings formats when we introduce backward-incompatible changes.

## Current Groundwork

Migration plumbing exists in:

- `src/settings/SettingsMigrations.h`
- `src/settings/SettingsMigrations.cpp`

Current behavior:

- reads `meta.settings_schema_version` from `config.yml` (`0` when missing)
- treats `1` as the current schema version
- applies a foundational `v0 -> v1` migration step (schema version stamping only)
- warns when loading a config with a newer schema version than the app supports

The schema version key is:

- `meta.settings_schema_version`

## Intended Migration Strategy

When introducing a breaking settings change:

1. increase `settings::migrations::kCurrentConfigSchemaVersion`
2. add deterministic `vN -> vN+1` migration logic in `SettingsMigrations.cpp`
3. keep migration steps small and composable
4. run migrations in order until current version is reached
5. keep existing keys readable for at least one compatibility cycle when practical

## Design Constraints

- migrations must be deterministic and idempotent
- unknown keys should be preserved whenever possible
- invalid input should fail safe (fallback to defaults, never crash)
- migration code should never require network access

## Testing Expectations

For each new migration step, add or update tests in `tests/StartupSettingsTest.cpp` to cover:

- old format -> migrated runtime values
- invalid/malformed legacy values
- schema version update behavior
