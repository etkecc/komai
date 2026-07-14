# ⚙️ Settings

Komai stores settings and session data as YAML under a per-profile directory.
Some secrets may live in your OS keychain, depending on [`secrets.provider`](#secret-storage-modes).

See a [🖼️ Screenshot of the settings page](../screenshots/settings.webp).

Quick links:

- 👥 [Application Profiles](../features/application-profiles.md)
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

- first character: ASCII letter or underscore (`A-Z`, `a-z`, `_`)
- remaining characters: ASCII letters, digits, underscore, dash (`A-Z`, `a-z`, `0-9`, `_`, `-`)

Dots and other characters (for example `.`, `/`, `\`, newlines, or non-ASCII text) are rejected.

Files in each profile directory:

- `config.yml` - durable preferences and advanced non-secret options
- `state.yml` - runtime/window/layout state
- `session.yml` - account/session metadata (non-secret)
- `secrets.yml` - file-mode fallback secrets (only when `secrets.provider=file`)

See also: [Storage Locations](../operations/storage.md#linux-paths).

## Profiles

For UI-based profile management, see [👥 Application Profiles](../features/application-profiles.md).

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

- `config.yml`: theme, fonts, desktop behavior (`desktop.notifications.*`, `desktop.attention.*`, `desktop.system_tray.*`, `desktop.window_focus_blur.*`), timeline behavior and maintenance, timeline media handling (including image/video/audio external-open preferences and the default inline playback speed), local HiddenEvents preferences (`timeline.hidden_events.global` / `timeline.hidden_events.by_room`), [communities sidebar filters](../features/communities-sidebar.md#-settings), network/db settings, `secrets.provider`, `network.presence.status_policy`
- `state.yml`: window size, sidebar widths, [hidden/collapsed sidebar state](../features/communities-sidebar.md#-hiding-sections), hidden pins/widgets, drafts, and similar runtime UI state
- `session.yml`: user id, homeserver, device id
- `secrets.yml`: `secrets` map (only when `secrets.provider=file`)
- Full example files: [settings/examples/profile/](examples/profile/)

Theme note: the selected theme is stored as `ui.theme.slug`, and the light/dark/auto mode as `ui.theme.mode`, in `config.yml`.
See [Themes](../features/themes.md#-where-your-current-theme-choice-is-stored).

## Configuration Management

Komai's settings live in plain YAML files (`config.yml`, `state.yml`, `session.yml`) under each profile directory. The format is human-editable and stable, which makes Komai amenable to external configuration management -- no proprietary format, no binary blobs, no registry.

Reference YAML files showing every setting and its default live in [`examples/profile/`](examples/profile/).

Common workflows:

- 🏠 **Dotfile sync** with [chezmoi](https://www.chezmoi.io/), [yadm](https://yadm.io/), or [GNU Stow](https://www.gnu.org/software/stow/) to version and replicate `~/.config/komai/` across machines.
- 🏢 **`/etc/skel` templates** -- drop a `~/.config/komai/profiles/default/config.yml` template into `/etc/skel/` so new user accounts on a workstation start with predefined settings.
- 🛠️ **Configuration management** with [Ansible](https://www.ansible.com/), [Puppet](https://www.puppet.com/), [Chef](https://www.chef.io/), or [Salt](https://saltproject.io/) to template and deploy YAML files per host or per user.
- ❄️ **NixOS / home-manager** modules to render `config.yml` from typed Nix expressions.

Tips:

- 👥 Use `-p <name>` to keep a managed/corporate profile separate from a personal one. Each profile has its own config directory.
- 🔐 Komai never writes secrets to `config.yml` (see [Secret Storage Modes](#secret-storage-modes)). Templated configs only need to cover non-secret preferences.
- 🪄 If `config.yml` is missing or partial, Komai fills in defaults and creates the file on first launch -- a minimal template covering only the keys you care about works fine.
- 🔌 For runtime control without editing files, see the [D-Bus integration](integrations/dbus.md).

Caveats:

- ⚠️ Komai does **not** currently enforce read-only / policy-locked settings. Users running the app can change any setting from the UI; managed setups need to re-deploy `config.yml` to keep them in sync.
- 🚫 There is no system-wide `/etc/komai/` layer -- all configuration lives under each user's `~/.config/komai/`. System-wide rollout means templating the per-user files (via `/etc/skel`, dotfiles, or config management) rather than a single global file.

## Integrations

Learn about optional integration hooks in the Integrations docs:

- [D-Bus integration](integrations/dbus.md)

## Backup and Restore

Check [Secret Storage Modes](#secret-storage-modes) first so you know what must be backed up.
Also see [Storage Locations](../operations/storage.md#secrets--providers) for on-disk vs secure-backend split.

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
| Matrix SDK state store | `~/.local/share/komai/profiles/<profile-id>/matrix-sdk/state-store/` |
| Matrix SDK runtime cache | `~/.cache/komai/profiles/<profile-id>/matrix-sdk/cache/` |
| Media cache | `~/.cache/komai/profiles/<profile-id>/media/` |
| User themes | `~/.local/share/komai/themes/` |
| Log file (file logging enabled) | `~/.cache/komai/profiles/<profile-id>/logs/komai.log` |

For complete storage details, see [storage.md](../operations/storage.md).
For theme file locations and loading rules, see [themes.md](../features/themes.md#-user-themes).

For implementation details, see [architecture/settings/README.md](../../architecture/settings/README.md) and
[architecture/storage.md](../../architecture/storage.md).
