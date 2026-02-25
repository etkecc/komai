# Secret Storage Differences vs nheko

This document summarizes Komai's secret storage behavior and differences from nheko.

## Komai Secret Provider Model

Komai uses explicit provider selection via `secrets.provider` in `config.yml`:

- `secret_service` (default): secure backend via [QtKeychain](https://github.com/frankosterfeld/qtkeychain) (for example [KWallet](https://api.kde.org/frameworks/kwallet/html/index.html) or [GNOME Keyring](https://gitlab.gnome.org/GNOME/gnome-keyring))
- `file`: fallback to `secrets.yml`

In both modes, secret identity names are profile-scoped and deterministic.

## Secret ID Format

Profile hash:

- `profile_hash = hex(sha256(normalized_profile_id))`
- `normalized_profile_id`: empty/default -> `default`

Namespaces:

- `komai.<profile_hash>.settings.<key>`
- `komai.<profile_hash>.local_crypto.<key>`
- `komai.<profile_hash>.matrix.<key>`

Default profile hash convenience value:

- `37a8eec1ce19687d132fe29051dca629d164e2c4958ba141d5f4133a33f0688f`

## Behavior by Provider

### `secret_service`

- Access token and secrets map are stored in secure backend.
- `session.yml` does not contain token/secrets.

### `file`

- `auth.access_token` is stored in `secrets.yml`.
- `secrets` stores all fallback secret values.
- Keys in `secrets` are full secret IDs (same IDs used in secure backend mode).

## Key-ID Notes

Secret IDs use the profile-first hex hash format and namespaces shown above.

See also:

- [Settings Architecture](../settings/README.md)
- [Settings Example (config.yml)](../settings/examples/profile/config.yml)
- [Settings Example (secrets.yml)](../settings/examples/profile/secrets.yml)
