# Configuration

Komai stores settings and session data as YAML under a per-profile directory.

## Profile Location

Each profile lives at:

```text
~/.config/komai/profiles/<profile-id>/
```

`<profile-id>` is the profile name/identifier you pass with `-p`.

Files in each profile directory:

- `config.yml` - durable preferences and advanced non-secret options
- `state.yml` - runtime/window/layout state
- `session.yml` - account/session metadata (non-secret)
- `secrets.yml` - file-mode fallback secrets (only when `secrets.provider=file`)

Default profile id is `default`.

## Profiles

Use `-p` to run a named profile:

```bash
komai                # default profile
komai -p work        # profile "work"
komai -p personal    # profile "personal"
```

If the profile does not exist, Komai creates it on first launch.

## Secret Storage Modes

Configure secret storage with `secrets.provider` in `config.yml`:

- `secret_service` (default): uses [QtKeychain](https://github.com/frankosterfeld/qtkeychain) with the platform secret backend (for example [KWallet](https://api.kde.org/frameworks/kwallet/html/index.html) or [GNOME Keyring](https://gitlab.gnome.org/GNOME/gnome-keyring))
- `file`: stores sensitive values in `secrets.yml`

Sensitive values:

- `auth.access_token`
- `secrets` map

Security invariants:

- Secrets are never written to `config.yml`.
- Secrets are never written to `state.yml`.
- `secrets.yml` is written with owner read/write permissions.

## What Goes Where

- `config.yml`: theme, fonts, notifications, timeline behavior, network/db settings, `secrets.provider`, `network.presence.default`
- `state.yml`: window size, sidebar widths, hidden/collapsed UI state, recent reactions
- `session.yml`: user id, homeserver, device id
- `secrets.yml`: `auth.access_token` and `secrets` map (only when `secrets.provider=file`)
- Full example files: [architecture/configuration-examples/profile/](architecture/configuration-examples/profile/)

## Backup and Restore

Backup profile files:

```bash
cp -r ~/.config/komai/profiles ~/komai-profiles-backup
```

Restore:

```bash
cp -r ~/komai-profiles-backup/* ~/.config/komai/profiles/
```

Important:

- In `file` mode, this backup includes `secrets.yml` (plaintext fallback secrets).
- In `secret_service` mode, secure-backend secrets are not in YAML files; backing up only `~/.config/komai/profiles` is not a full credential backup.

## Data Locations (Linux)

| Data | Location |
| --- | --- |
| Profile settings/session/state | `~/.config/komai/profiles/` |
| Local database data (default backend: LMDB in standard builds; memory if LMDB backend is disabled at build time) | `~/.local/share/komai/profiles/<profile-id>/db/<hash>/` |
| Media cache | `~/.cache/komai/profiles/<profile-id>/media_cache/` |
| Log file (file logging enabled) | `~/.cache/komai/profiles/<profile-id>/komai.log` |

For complete storage details, see [storage.md](storage.md).

For implementation details, see [architecture/configuration.md](architecture/configuration.md) and
[architecture/storage.md](architecture/storage.md).
