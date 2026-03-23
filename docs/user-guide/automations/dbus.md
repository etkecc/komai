# 🔌 D-Bus API

Komai exposes a local [D-Bus](https://www.freedesktop.org/wiki/Software/dbus/) API so external tools can interact with your running session -- query rooms, switch themes, open chats, and more.

> **Before you start:** D-Bus integration must be enabled in [Settings → Integrations → D-Bus](../settings/integrations/dbus.md).

## 🛰️ Service details

Each [application profile](../application-profiles.md) registers its own D-Bus service:

- **Service name format:** `cc.etke.komai.profile.<profile-id>`
- **Object path:** `/`

| Profile | D-Bus service name |
|---|---|
| *(default)* | `cc.etke.komai.profile.default` |
| `work` | `cc.etke.komai.profile.work` |
| `personal` | `cc.etke.komai.profile.personal` |

Methods are organized into **interfaces**:

| Interface | Purpose |
|---|---|
| `cc.etke.komai.App` | Instance metadata |
| `cc.etke.komai.Rooms` | Room discovery and navigation |
| `cc.etke.komai.User` | Account and presence |
| `cc.etke.komai.Settings.UI` | Appearance settings |
| `cc.etke.komai.Media` | Media content resolution |

Introspect a running instance to see all interfaces:

```bash
busctl --user introspect cc.etke.komai.profile.default /
```

## 👥 Multiple profiles

When running multiple profiles (`komai -p work`, `komai -p personal`), each registers its own service. Target any profile by name.

### Discovering running profiles

```bash
busctl --user list | grep cc.etke.komai.profile
```

### Targeting a specific profile

Replace `default` with the profile name:

```bash
busctl --user call cc.etke.komai.profile.work / cc.etke.komai.Rooms list
```

## 🔧 How to use

The `busctl` examples below are for **quick testing from a terminal**. For actual scripting and automation, use a D-Bus library in your language of choice -- for example [dbus-python](https://dbus.freedesktop.org/doc/dbus-python/), [zbus](https://docs.rs/zbus/) (Rust), [godbus](https://github.com/godbus/dbus) (Go), or [dbus-next](https://github.com/altdesktop/python-dbus-next) (Python, asyncio).

## 📡 App

Instance metadata.

### apiVersion

Returns the D-Bus API version string.

> Required D-Bus access level: 👁️ any (always available)

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.App apiVersion
```

### appVersion

Returns the Komai version string.

> Required D-Bus access level: 👁️ any (always available)

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.App appVersion
```

## 🚪 Rooms

Room discovery and navigation.

### list

Returns all joined rooms with IDs, aliases, names, avatar URLs, and unread notification counts. See also: [activate](#activate), [join](#join).

> Required D-Bus access level: 👁️ read

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms list
```

### activate

Activates (focuses) a room by room ID or alias. See also: [list](#list).

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms activate s '!a:example.org'
```

### join

Joins a room by room ID or alias. See also: [list](#list).

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms join s '#komai:example.org'
```

### newDirectChat

Starts or opens a one-to-one chat with a user.

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms newDirectChat s '@alice:example.org'
```

## 👤 User

Account and presence.

### statusMessage / setStatusMessage

Reads or sets the user's presence status text.

> Required D-Bus access level: 👁️ read (get) / ✏️ write (set)

```bash
# Get
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.User statusMessage

# Set
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.User setStatusMessage s 'brb'
```

## ⚙️ Settings.UI

Appearance settings.

### theme / setTheme

Reads or sets the active theme. `setTheme` expects a valid theme slug (`komai-light`, `komai-dark`, `nheko-light`, ...), not a display label.

> Required D-Bus access level: 👁️ read (get) / ✏️ write (set)

```bash
# Get
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Settings.UI theme

# Set
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Settings.UI setTheme s 'komai-dark'
```

## 🖼️ Media

Media content resolution. Resolves Matrix content URIs (`mxc://`) into image data through Komai's authenticated session.

### fetch

Fetches an image by its `mxc://` URI. Useful for resolving room avatars returned by [Rooms.list](#list).

> Required D-Bus access level: 👁️ read

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Media fetch s 'mxc://example.org/abc123'
```

## 🔐 Security notes

- Access is local-only to users that can talk to your current session bus.
- Access is controlled per profile by **Settings → Integrations → D-Bus** ([details](../settings/integrations/dbus.md)).
- Keep D-Bus access enabled only when needed for your workflow.
