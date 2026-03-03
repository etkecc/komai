# ⚙️ Settings

Komai stores settings and session data as YAML under a per-profile directory.
Some secrets may live in your OS keychain, depending on [`secrets.provider`](#secret-storage-modes).

Quick links:

- 👥 [Application Profiles](../application-profiles.md)
- 🔐 [Secret Storage](secret-storage.md)
- 💾 [Backup and Restore](backup-and-restore.md)
- 🔌 [Integrations](integrations/README.md)

## Profile Location

Each profile lives at:

```text
~/.config/komai/profiles/<profile-id>/
```

`<profile-id>` is the profile name/identifier you pass with `-p`.

💡 If a profile is not explicitly specified, Komai opens the profile switcher unless only `default` exists.

Allowed profile-id characters:

- ASCII letters and digits: `A-Z`, `a-z`, `0-9`
- punctuation: `.`, `_`, `-`

Other characters (for example `/`, `\`, newlines, or non-ASCII text) are rejected.

Files in each profile directory:

- `config.yml` - durable preferences and advanced non-secret options
- `state.yml` - runtime/window/layout state
- `session.yml` - account/session metadata (non-secret)
- `secrets.yml` - file-mode fallback secrets (only when `secrets.provider=file`)

See also: [Storage Locations](../storage.md#linux-paths).

## Profiles

For UI-based profile management, see [👥 Application Profiles](../application-profiles.md).

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

Quick read:

- ✅ `secret_service` is preferred (more secure)
- ⚠️ `file` works everywhere but stores fallback secrets in profile files

Startup selection behavior:

- New profile creation prefers `secret_service` when secure backend is available; otherwise Komai initializes the profile with `file`.
- While the profile is still in pre-auth setup (no persisted session identity yet; Welcome/Login/Register flow), Komai re-checks secure-backend availability on each launch and may switch `secrets.provider` between `file` and `secret_service`.
- After an active session exists, Komai does not auto-switch providers. If secure backend is unavailable in a `secret_service` profile, startup/login operations fail loudly instead of silently falling back.

Sensitive values:

- `secrets` map (contains internal `__session.access_token` plus other secret values)

Security invariants:

- Secrets are never written to `config.yml`.
- Secrets are never written to `state.yml`.
- `secrets.yml` is written with owner read/write permissions.

Development/testing override:

- Set `KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY=unavailable` to force startup provider selection to treat secure backend as unavailable.
- Set `KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY=available` to force startup provider selection to treat secure backend as available.

## What Goes Where

- `config.yml`: theme, fonts, notifications, timeline behavior, network/db settings, `secrets.provider`, `network.presence.status_policy`
- `state.yml`: window size, sidebar widths, hidden/collapsed UI state, recent reactions
- `session.yml`: user id, homeserver, device id
- `secrets.yml`: `secrets` map (only when `secrets.provider=file`)
- Full example files: [settings/examples/profile/](examples/profile/)

Theme note: the currently selected theme is stored as `ui.theme.slug` in `config.yml`.
See [Themes](../themes.md#-where-your-current-theme-choice-is-stored).

## Integrations

Learn about optional integration hooks in the Integrations docs:

- [D-Bus integration](integrations/dbus.md)

## Backup and Restore

Check [Secret Storage Modes](#secret-storage-modes) first so you know what must be backed up.
Also see [Storage Locations](../storage.md#secrets--providers) for on-disk vs secure-backend split.

Backup profile files:

```bash
cp -r ~/.config/komai/profiles ~/komai-profiles-backup
```

Restore:

```bash
cp -r ~/komai-profiles-backup/* ~/.config/komai/profiles/
```

Important:

- 📄 In `file` mode, this backup includes `secrets.yml` (plaintext fallback secrets).
- 🔐 In `secret_service` mode (default and preferred), secure-backend secrets are not in YAML files; backing up only `~/.config/komai/profiles` is not a full credential backup.
- 💾 For `secret_service`, also back up the OS keychain backend used by your environment.

Common secure-backend backup targets:

- GNOME Keyring/libsecret: back up keyring files (commonly under `~/.local/share/keyrings/`) or export via keyring UI tools.
- KWallet: back up wallet files (commonly under `~/.local/share/kwalletd/`) or export with KWallet Manager.
- KeePassXC-backed setups: back up the KeePassXC database file.
- macOS/Windows: include system keychain/credential-store data in your user/system backup strategy.

## Data Locations (Linux)

| Data | Location |
| --- | --- |
| Profile settings/session/state | `~/.config/komai/profiles/` |
| Local database data (default backend: LMDB in standard builds; memory if LMDB backend is disabled at build time) | `~/.local/share/komai/profiles/<profile-id>/db/<encoded-user-id>/` |
| User themes | `~/.local/share/komai/themes/` |
| Media cache | `~/.cache/komai/profiles/<profile-id>/media_cache/` |
| Log file (file logging enabled) | `~/.cache/komai/profiles/<profile-id>/komai.log` |

For complete storage details, see [storage.md](../storage.md).
For theme file locations and loading rules, see [themes.md](../themes.md#-user-themes).

For implementation details, see [architecture/settings/README.md](../../architecture/settings/README.md) and
[architecture/storage.md](../../architecture/storage.md).
