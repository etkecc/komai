# Secret Storage Differences vs nheko

This document summarizes Komai's secret storage behavior and differences from nheko.

## Komai Secret Provider Model

Komai uses explicit provider selection via `secrets.provider` in `config.yml`:

- `secret_service` (default): secure backend via [QtKeychain](https://github.com/frankosterfeld/qtkeychain) (for example [KWallet](https://api.kde.org/frameworks/kwallet/html/index.html) or [GNOME Keyring](https://gitlab.gnome.org/GNOME/gnome-keyring))
- `file`: fallback to `secrets.yml`

Provider selection is startup-aware:

- first profile launch chooses the best available provider and writes it to config
- pre-auth launches (no persisted session identity) can re-evaluate and switch providers
- post-auth launches keep the configured provider and fail loudly on secure-backend issues

In both modes, secret identity names are profile-scoped and deterministic.

## Secret ID Format

Environment tag:

`<env-tag>` isolates secrets across packaging formats depending on the filesystem
config paths they use, so that each unique filesystem path produces a unique keyring prefix.

- `native` — standard (non-sandboxed) builds on any platform, including AppImage (config root ends in `/.config/komai`, `/Library/Preferences/komai`, or `/AppData/Local/komai`)
- `flatpak` — config root ends in `/.var/app/cc.etke.komai/config/komai`
- `snap` — config root contains `/snap/` and ends in `/.config/komai`
- 6-char hex hash — any other config root path

Profile id:

- `normalized_profile_id`: empty/default -> `default`
- all other profile IDs are unchanged
- valid non-empty profile IDs are ASCII `[A-Za-z_][A-Za-z0-9_-]*`

Namespaces:

- `komai.<env-tag>.<profile-id>.settings.<key>`
- `komai.<env-tag>.<profile-id>.local_crypto.<key>`
- `komai.<env-tag>.<profile-id>.matrix.<key>`

## Behavior by Provider

### `secret_service`

- Secrets map is stored in secure backend (`session.secrets`).
- Access token is embedded in that map under internal `__session.access_token`.
- `session.yml` does not contain token/secrets.

### `file`

- `secrets` in `secrets.yml` stores all fallback secret values.
- Access token is embedded in that map under internal `__session.access_token`.
- Keys in `secrets` are full secret IDs (same IDs used in secure backend mode).

## Key-ID Notes

Secret IDs use the normalized profile id and namespaces shown above.

See also:

- [Settings Architecture](../settings/README.md)
- [Settings Example (config.yml)](../../user-guide/settings/examples/profile/config.yml)
- [Settings Example (secrets.yml)](../../user-guide/settings/examples/profile/secrets.yml)
