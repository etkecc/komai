# Configuration Architecture

This document describes the internal architecture of Komai's configuration system.


## Overview

Komai uses a per-profile YAML-based configuration system, replacing nheko's Qt QSettings approach. Each profile gets its own YAML file, enabling:

- Human-readable configuration files
- Easy backup and manual editing
- Complete isolation between profiles
- No reliance on Qt's platform-specific storage backends


## File Structure

```
~/.config/komai/
  profiles/
    default.yml      # Default profile
    work.yml         # Named profile "work"
    personal.yml     # Named profile "personal"
```


## Implementation

### Core Classes

**`UserSettings`** (`src/UserSettingsPage.h`, `src/UserSettingsPage.cpp`)

Singleton class managing all application settings:

- `load(profile)` - Loads settings from the profile's YAML file
- `save()` - Writes current settings to YAML
- Property accessors for all settings (Qt property system for QML binding)
- Signals for change notifications

### YAML I/O

Uses [yaml-cpp](https://github.com/jbeder/yaml-cpp) library for parsing and emitting YAML.

**Loading** (`UserSettings::load`):
```cpp
YAML::Node config = YAML::LoadFile(configFilePath_.toStdString());
// Read with defaults:
tray_ = config["tray"] ? config["tray"].as<bool>() : false;
```

**Saving** (`UserSettings::save`):
```cpp
YAML::Emitter out;
out << YAML::BeginMap;
out << YAML::Key << "tray" << YAML::Value << tray_;
// ... more settings
out << YAML::EndMap;

std::ofstream fout(configFilePath_.toStdString());
fout << out.c_str();
```

### Profile Path Resolution

```cpp
// Helper in UserSettingsPage.cpp
static QString configFilePath(const QString &profile) {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                  + "/komai/profiles";
    QDir().mkpath(dir);
    QString name = profile.isEmpty() ? "default" : profile;
    return dir + "/" + name + ".yml";
}
```


## Settings Categories

Settings are organized into logical groups in the YAML file:

| Category | Examples |
|----------|----------|
| Window | `tray`, `start_in_tray`, `window_width`, `window_height` |
| Sidebar | `room_list_width`, `community_list_width` |
| Timeline | `bubbles`, `markdown`, `show_action_buttons` |
| Notifications | `desktop_notifications`, `alert_on_incoming_messages` |
| Appearance | `theme`, `font_family`, `font_size`, `use_circular_avatars` |
| Auth | `access_token`, `homeserver`, `user_id`, `device_id` |
| Encryption | `share_keys_with_trusted_users`, `use_online_key_backup` |
| Secrets | `secrets` map for keychain fallback storage |


## Secrets Storage

Komai stores encryption keys and other secrets in the system keychain (GNOME Keyring, KWallet, etc.) when available.

For systems without a secure keychain, secrets can be stored in the YAML config file instead by setting `run_without_secure_secrets_service: true` **before first login**:

```yaml
run_without_secure_secrets_service: true

# Secrets will be stored here (managed automatically):
secrets:
  pickle_secret: "base64-encoded-secret"
  # ... other secrets
```

This setting defaults to `false` and is not exposed in the Settings UI -- it must be set manually in the config file before logging in. Changing it after login has no effect on already-stored secrets.


## Migration Notes

### From nheko/QSettings

Komai does not migrate from nheko's QSettings format. Users starting fresh will have settings initialized with sensible defaults.

### Key Differences from nheko

| Aspect | nheko | Komai |
|--------|-------|-------|
| Format | Qt INI (QSettings) | YAML |
| Location | `~/.config/nheko/nheko.conf` | `~/.config/komai/profiles/<name>.yml` |
| Profile storage | Prefixed keys in single file | Separate file per profile |
| Library | Qt QSettings | yaml-cpp |


## Adding New Settings

1. Add member variable to `UserSettings` class
2. Add Qt property with getter/setter/signal
3. Load from YAML in `UserSettings::load()` with appropriate default
4. Save to YAML in `UserSettings::save()`
5. (Optional) Expose in settings UI via `UserSettingsModel`
