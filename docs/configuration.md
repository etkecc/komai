# Configuration

Komai stores all settings in human-readable YAML files, making it easy to back up, edit, or share your configuration.


## File Location

Configuration files are stored per-profile at:

```
~/.config/komai/profiles/<profile-name>.yml
```

For example:
- Default profile: `~/.config/komai/profiles/default.yml`
- Work profile: `~/.config/komai/profiles/work.yml`


## Profiles

Profiles let you run multiple Matrix accounts with completely separate settings. Each profile has its own:

- Login credentials and session
- Theme and appearance settings
- Notification preferences
- Sidebar widths and window size
- All other preferences

### Using Profiles

Launch with a specific profile using the `-p` flag:

```bash
komai -p work      # Use the "work" profile
komai -p personal  # Use the "personal" profile
komai              # Use the default profile
```

If the profile doesn't exist, it will be created on first launch.

### Switching Profiles

To switch profiles, close Komai and relaunch with a different `-p` argument. Each profile runs independently, so you can also run multiple instances with different profiles simultaneously.


## Configuration Format

Settings are stored in YAML format. Here's an example showing some common settings:

```yaml
theme: komai-dark
font_size: 14.0
bubbles: true
desktop_notifications: true
typing_notifications: true
markdown: true

# Authentication (managed automatically)
user_id: "@alice:example.com"
homeserver: "https://example.com"
device_id: "ABCD1234"
```

Most settings are managed through the Settings page in the app. You can also edit the YAML file directly while Komai is closed.


## Backup and Restore

To back up your configuration:

```bash
cp -r ~/.config/komai/profiles ~/komai-config-backup
```

To restore:

```bash
cp -r ~/komai-config-backup/* ~/.config/komai/profiles/
```

Note: The configuration files contain your access token. Keep backups secure.


## Data Locations

| Data | Location |
|------|----------|
| Configuration | `~/.config/komai/profiles/` |
| Message database | `~/.local/share/komai/` |
| Cache | `~/.cache/komai/` |
| Logs | `~/.local/share/komai/komai.log` |
