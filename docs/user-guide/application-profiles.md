# 👥 Application Profiles

Application Profiles let you run multiple independent Komai instances on the same machine.

Each profile has its own:

- login/session
- settings
- local database/cache
- secret storage entries

Use this for setups like `work` and `personal` without mixing data.

## Open the Profile UI

You can manage profiles from:

- **Application Settings** -> **Application Profiles**
- room list user menu (right-click your avatar/settings button) -> **Open Profile Switcher**
- running `komai` (or `komai -p ""`) without an explicit profile opens the switcher whenever profiles are not just `default`

## Create and Launch

In **Application Profiles**:

- click **Create new**
- enter a profile name (for example `work`, `personal`, `dev_build`)
- on native Linux, leave **Create desktop launcher** enabled for non-default profiles if you want reliable app badges and launcher grouping
- click **Create and Launch**

Komai launches a separate app instance with that profile.

## Launch Existing Profiles

In the profile card list, click a card to launch that profile in a new window.

The standalone switcher window closes after a successful launch.

## Delete Profiles

Each profile card has a delete action with confirmation.

Deleting a profile removes its profile-scoped data, including:

- profile config/session/state files under `~/.config/komai/profiles/<profile-id>/`
- local profile database under `~/.local/share/komai/profiles/<profile-id>/`
- profile secrets for the active secret provider

## CLI Behavior

```bash
komai             # opens switcher unless only `default` exists
komai -p work     # always launch profile "work" directly
komai -p ""       # same selector-mode behavior as bare `komai`
```

Profile names follow the same validation rules as `-p` names documented in [Settings -> Profile Location](settings/README.md#profile-location). In practice, that means names like `work`, `personal`, `dev_build`, or `work-2` are valid, while names containing `.` are not.

## Reliable App Badges With Multiple Profiles

On native Linux, the packaged `default` profile launcher already has a stable
app/launcher identity. Non-default profiles only get reliable app-icon/taskbar
badges when they are launched from their own desktop launcher.

The **Create Application Profile** dialog offers this by default for new
non-default profiles. If you already have a profile, create its launcher
explicitly:

```bash
komai profiles launcher create work
```

This writes:

```text
~/.local/share/applications/cc.etke.komai.profile.work.desktop
```

Remove it later with:

```bash
komai profiles launcher remove work
```

After creating the launcher, Komai can use that profile-specific desktop/app
identity even when you start it with `komai -p work`, because startup checks
for an installed matching launcher entry. Starting from the created desktop
entry is still convenient, but not required for badge targeting.

Manual desktop-entry example:

```ini
[Desktop Entry]
Version=1.5
Type=Application
Name=Komai (work)
Comment=Desktop client for Matrix
Exec=/absolute/path/to/komai -p work %u
Icon=cc.etke.komai
Categories=Network;InstantMessaging;Qt;
Terminal=false
X-GNOME-UsesNotifications=true
```

Keep the filename aligned with the profile desktop ID:

```text
cc.etke.komai.profile.work.desktop
```
