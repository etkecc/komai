# 🔌 D-Bus API

Komai exposes a local [D-Bus](https://www.freedesktop.org/wiki/Software/dbus/) API so external tools can interact with your running session -- query rooms, switch themes, open chats, and more.

> **Before you start:** D-Bus integration must be enabled in [Settings → Integrations → D-Bus](../settings/integrations/dbus.md).

## 🛰️ Service details

Each [application profile](../application-profiles.md) registers its own D-Bus service:

- **Service name format:** `cc.etke.komai.profile.<profile-id>`
- **Object path:** `/`
- **Interface:** `cc.etke.komai`

| Profile | D-Bus service name |
|---|---|
| *(default)* | `cc.etke.komai.profile.default` |
| `work` | `cc.etke.komai.profile.work` |
| `personal` | `cc.etke.komai.profile.personal` |

Introspect a running instance:

```bash
gdbus introspect --session --dest cc.etke.komai.profile.default --object-path /
```

## 👥 Multiple profiles

When running multiple profiles (`komai -p work`, `komai -p personal`), each registers its own service. Target any profile by name.

### Discovering running profiles

```bash
busctl --user list | grep cc.etke.komai.profile
```

### Targeting a specific profile

Replace `default` with the profile name in any `gdbus` command:

```bash
# Get rooms from the "work" profile
gdbus call --session --dest cc.etke.komai.profile.work --object-path / --method cc.etke.komai.rooms

# Set theme on the "personal" profile
gdbus call --session --dest cc.etke.komai.profile.personal --object-path / --method cc.etke.komai.setTheme 'komai-dark'
```

## 🛰 Read methods

### apiVersion

Returns API version as a string.

```bash
gdbus call --session --dest cc.etke.komai.profile.default --object-path / --method cc.etke.komai.apiVersion
```

```text
( "1" )
```

### appVersion

Returns Komai version string.

```bash
gdbus call --session --dest cc.etke.komai.profile.default --object-path / --method cc.etke.komai.appVersion
```

```text
( "0.9.0" )
```

### rooms

Returns a compact list of known rooms with IDs and names.

```bash
gdbus call --session --dest cc.etke.komai.profile.default --object-path / --method cc.etke.komai.rooms
```

```text
( [('!a:example.org', 'komai-dev'), ('!b:example.org', 'random')],
  '/tmp' )
```

### image

Returns an image for a matrix content URI.

```bash
gdbus call --session --dest cc.etke.komai.profile.default --object-path / --method cc.etke.komai.image 'mxc://example.org/abc123'
```

### statusMessage

Reads the current user presence/status text.

```bash
gdbus call --session --dest cc.etke.komai.profile.default --object-path / --method cc.etke.komai.statusMessage
```

```text
( "building D-Bus docs 🎉" )
```

## 🛠 Write methods

### activateRoom

Activates (focuses) a room in the current session.

```bash
gdbus call --session --dest cc.etke.komai.profile.default --object-path / --method cc.etke.komai.activateRoom '!a:example.org'
```

### joinRoom

Joins a room by room ID or alias.

```bash
gdbus call --session --dest cc.etke.komai.profile.default --object-path / --method cc.etke.komai.joinRoom '#komai:example.org'
```

### directChat

Starts or opens a one-to-one chat.

```bash
gdbus call --session --dest cc.etke.komai.profile.default --object-path / --method cc.etke.komai.directChat '@alice:example.org'
```

### setStatusMessage

Sets the user status message.

```bash
gdbus call --session --dest cc.etke.komai.profile.default --object-path / --method cc.etke.komai.setStatusMessage '📣 status from D-Bus'
```

### setTheme

Changes the active theme. Expects a valid theme slug (`komai-light`, `komai-dark`, `nheko-light`, ...), not a display label.

```bash
gdbus call --session --dest cc.etke.komai.profile.default --object-path / --method cc.etke.komai.setTheme 'komai-dark'
```

## 🔐 Security notes

- Access is local-only to users that can talk to your current session bus.
- Access is controlled per profile by **Settings → Integrations → D-Bus** ([details](../settings/integrations/dbus.md)).
- Keep D-Bus access enabled only when needed for your workflow.
