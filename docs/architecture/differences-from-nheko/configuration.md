# Configuration Differences vs nheko

This document captures configuration-architecture differences between Komai and upstream nheko.

## High-Level Differences

| Area | nheko (baseline) | Komai |
| --- | --- | --- |
| Settings backend | Qt `QSettings` (INI-style) | YAML files per profile |
| Profile storage shape | single settings store with profile-specific keys | directory per profile with split concern files |
| File layout | single settings file model | `config.yml`, `state.yml`, `session.yml`, `secrets.yml` (`file` provider only) |
| Data/cache layout | app/org-dependent Qt location shape | explicit profile-scoped layout under `~/.local/share/komai/profiles/<profile-id>/` and `~/.cache/komai/profiles/<profile-id>/` |
| Secret provider selection | not modeled as staged file-first provider key | explicit `secrets.provider` in `config.yml` |
| Load pipeline | monolithic load path | staged load (`config` -> `session` -> secrets -> `state`) |
| Runtime API | mixed implementation concerns | flat runtime API with split persistence concerns |
| Path construction | scattered per-callsite path joins | centralized in `src/Paths.h` / `src/Paths.cpp` |

## Komai-Specific Configuration Design

- Per-profile directory: `~/.config/komai/profiles/<profile-id>/` (`<profile-id>` is the `-p` profile name/identifier)
- Default profile id is `default`
- Dotted hierarchical keys in YAML (`a.b.c`)
- Durable/runtime/session/secret concerns are written to separate files
- LMDB and cache paths are profile-scoped (`data/profiles/<profile-id>/...`, `cache/profiles/<profile-id>/...`)

## Why This Matters

- Easier manual inspection and backups
- Lower chance of writing secrets into preference/state files
- Better profile isolation and deterministic secret namespacing
- Cleaner evolution path for future settings refactors

See also:

- [User Configuration Guide](../../configuration.md)
- [Configuration Architecture](../configuration.md)
- [Storage Architecture](../storage.md)
- [Storage Guide](../../storage.md)
- [Secret Storage Differences](secret-services.md)
- [Settings Name Mapping](settings-mapping.md)
- [Configuration Example (config.yml)](../configuration-examples/profile/config.yml)
- [Configuration Example (state.yml)](../configuration-examples/profile/state.yml)
- [Configuration Example (session.yml)](../configuration-examples/profile/session.yml)
- [Configuration Example (secrets.yml)](../configuration-examples/profile/secrets.yml)
