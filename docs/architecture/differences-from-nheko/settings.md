# Settings Differences vs nheko

This document captures settings-architecture differences between Komai and upstream nheko.

## High-Level Differences

| Area | nheko (baseline) | Komai |
| --- | --- | --- |
| Settings backend | Qt `QSettings` (INI-style) | YAML files per profile |
| Profile storage shape | single settings store with profile-specific keys | directory per profile with split concern files |
| File layout | single settings file model | `config.yml`, `state.yml`, `session.yml`, `secrets.yml` (`file` provider only) |
| Data/cache layout | app/org-dependent Qt location shape | explicit profile-scoped layout under `~/.local/share/komai/profiles/<profile-id>/` and `~/.cache/komai/profiles/<profile-id>/` |
| Secret provider selection | not modeled as staged file-first provider key | explicit `secrets.provider` in `config.yml` |
| Load pipeline | monolithic load path | staged load (`config` -> `session` -> secrets -> `state`) |
| Runtime API | mixed implementation concerns | flat runtime API with split persistence concerns and semantics-aligned setting names |
| Path construction | scattered per-callsite path joins | centralized in `src/Paths.h` / `src/Paths.cpp` |

## Komai-Specific Settings Design

- Per-profile directory: `~/.config/komai/profiles/<profile-id>/` (`<profile-id>` is the `-p` profile name/identifier)
- Default profile id is `default`
- Dotted hierarchical keys in YAML (`a.b.c`)
- Some keys are intentionally renamed from upstream nheko flat keys (for example, nheko `presence` maps to Komai `network.presence.status_policy`, and nheko `tray`/`start_in_tray` map to `desktop.system_tray.enabled`/`desktop.system_tray.autostart`). Some upstream settings, such as `mobile_mode` and `enable_swipe_gestures`, are not carried forward.
- Durable/runtime/session/secret concerns are written to separate files
- Database and cache paths are profile-scoped (`data/profiles/<profile-id>/...`, `cache/profiles/<profile-id>/...`)

## Why This Matters

- Easier manual inspection and backups
- Lower chance of writing secrets into preference/state files
- Better profile isolation and deterministic secret namespacing
- Cleaner evolution path for future settings refactors

See also:

- [User Settings Guide](../../user-guide/settings/README.md)
- [Settings Architecture](../settings/README.md)
- [Storage Architecture](../storage.md)
- [Storage Guide](../../user-guide/operations/storage.md)
- [Secret Storage Differences](secret-services.md)
- [Settings Name Mapping](settings-mapping.md)
- [Settings Example (config.yml)](../../user-guide/settings/examples/profile/config.yml)
- [Settings Example (state.yml)](../../user-guide/settings/examples/profile/state.yml)
- [Settings Example (session.yml)](../../user-guide/settings/examples/profile/session.yml)
- [Settings Example (secrets.yml)](../../user-guide/settings/examples/profile/secrets.yml)
