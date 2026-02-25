# Settings Tooling 🧭

Start with [`docs/architecture/configuration.md`](../../docs/architecture/configuration.md) to understand the settings layers, then use this directory for architecture audits.

## Purpose

Scripts here help keep settings naming and wiring understandable across 3 layers:

- `SettingId` (core schema)
- runtime getter expressions (`UserSettings` bridge)
- persisted config keys (`SettingKey` constants and dotted keys)

This is mainly for refactoring safety and review clarity.

## Scripts

- `generate-3-layer-mapping.sh`
  - Generates `var/plans/settings-3-layer-mapping.md`
  - Supports `--check` mode for drift detection without overwriting.

Examples:

```sh
just settings-generate-3-layer-mapping
just settings-check-3-layer-mapping
```

## Why Not A Default Pre-commit Hook?

By default, this report is written under `var/plans/`, which is intentionally not tracked in git. Running a strict hook for an untracked artifact would add noise without protecting repository state.

If we later move this report to a tracked location (for example `docs/architecture/`), wiring `--check` into `prek` becomes useful and straightforward.
