# Settings Tooling 🧭

Start with [`docs/architecture/settings/README.md`](../../docs/architecture/settings/README.md) to understand the settings layers, then use this directory for architecture audits.

## Purpose

Scripts here help keep settings naming and wiring understandable across 3 layers:

- `SettingId` (core schema)
- runtime getter expressions (`UserSettings` bridge)
- persisted config keys (`SettingKey` constants and dotted keys)

This is mainly for refactoring safety and review clarity.

## Scripts

- `settings-3-layer-mapping.sh`
  - Generates `docs/architecture/settings/3-layer-mapping.md`
  - Supports `--check` mode for drift detection without overwriting.

Examples:

```sh
just settings-3-layer-mapping-generate
just settings-3-layer-mapping-check
```

## Drift Checks

This report is now a tracked docs artifact. A `prek` check should enforce `--check` mode to prevent drift.
