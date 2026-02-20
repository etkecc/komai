# Configuration Architecture

This document describes Komai's current configuration and secret persistence architecture.

## Overview

Komai keeps a flat runtime `UserSettings` API (Qt properties/signals) but persists data by concern into separate per-profile files.

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

- [Profile config.yml](configuration-examples/profile/config.yml)
- [Profile state.yml](configuration-examples/profile/state.yml)
- [Profile session.yml](configuration-examples/profile/session.yml)
- [Profile secrets.yml](configuration-examples/profile/secrets.yml)

## Persistence Model

Primary implementation:

- `src/UserSettingsPage.cpp`
- `src/UserSettingsPage.h`

Persistence split:

- `UserSettings::loadConfigYaml(...)` / `saveConfigYaml()`
- `UserSettings::loadSessionYaml(...)` / `saveSessionYaml()`
- `UserSettings::loadSecretsYaml(...)` / `saveSecretsYaml()`
- `UserSettings::loadStateYaml(...)` / `saveStateYaml()`

YAML key hierarchy is nested/dotted (for example `timeline.messages.layout.bubbles`).

## Staged Load Order

Load order is intentionally staged:

1. `config.yml` (resolve `secrets.provider`)
2. `session.yml` (account metadata)
3. secrets source
   - `secret_service`: read secrets from secure backend
   - `file`: read secrets from `secrets.yml`
4. `state.yml` (runtime/layout)

This prevents secret-source ambiguity and allows provider selection before secret reads.

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

## Notes

- Canonical references are:
  - [User Configuration Guide](../configuration.md)
  - [Storage Guide](../storage.md)
  - [this architecture document](configuration.md)
  - [storage architecture](storage.md)
  - [configuration examples](configuration-examples/profile/)
  - [nheko to Komai settings mapping](differences-from-nheko/settings-mapping.md)
