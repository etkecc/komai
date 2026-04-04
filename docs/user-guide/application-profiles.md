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
