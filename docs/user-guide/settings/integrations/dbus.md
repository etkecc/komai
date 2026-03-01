# 🔌 [D-Bus](https://www.freedesktop.org/wiki/Software/dbus/) integration

Komai can expose a local [D-Bus](https://www.freedesktop.org/wiki/Software/dbus/) API so external tools can interact with your running session.

This is controlled in **Settings → Integrations → D-Bus** with the **D-Bus access** dropdown.

- `integrations.dbus.access`:
  - `0` - `None`
  - `1` - `Read-only`
  - `2` - `Read & write`

No restart is needed when changing it.

## 🧭 What’s available

- [🛰 Read methods](#read-methods)
  - [apiVersion](#apiversion)
  - [appVersion](#appversion)
  - [rooms](#rooms)
  - [image](#image)
  - [statusMessage](#statusmessage)
- [🛠 Write methods](#write-methods)
  - [activateRoom](#activateroom)
  - [joinRoom](#joinroom)
  - [directChat](#directchat)
  - [setStatusMessage](#setstatusmessage)
  - [setTheme](#settheme)
- [🛰️ Service details](#service-details)
- [🔐 Security notes](#security-notes)

## 🛰 Read methods

### apiVersion

Returns API version as a string.

```bash
gdbus call --session --dest cc.etke.komai --object-path / --method cc.etke.komai.apiVersion
```

Example result:

```text
( "1" )
```

### appVersion

Returns Komai version string.

```bash
gdbus call --session --dest cc.etke.komai --object-path / --method cc.etke.komai.appVersion
```

Example result:

```text
( "0.9.0" )
```

### rooms

Returns a compact list of known rooms with IDs and names.

```bash
gdbus call --session --dest cc.etke.komai --object-path / --method cc.etke.komai.rooms
```

Example result:

```text
( [('!a:example.org', 'komai-dev'), ('!b:example.org', 'random')],
  '/tmp' )
```

### image

Returns an image file path or URL for a room state ID.

```bash
gdbus call --session --dest cc.etke.komai --object-path / --method cc.etke.komai.image '!a:example.org'
```

Example result:

```text
( "/home/user/.cache/komai/avatars/a.png" )
```

### statusMessage

Reads the current user presence/status text.

```bash
gdbus call --session --dest cc.etke.komai --object-path / --method cc.etke.komai.statusMessage
```

Example result:

```text
( "building D-Bus docs 🎉" )
```

## 🛠 Write methods

### activateRoom

Activates (focuses) a room in the current session.

```bash
gdbus call --session --dest cc.etke.komai --object-path / --method cc.etke.komai.activateRoom '!a:example.org'
```

Example result:

```text
()
```

### joinRoom

Joins a room by room ID or alias.

```bash
gdbus call --session --dest cc.etke.komai --object-path / --method cc.etke.komai.joinRoom '#komai:example.org'
```

Example result:

```text
()
```

### directChat

Starts or opens a one-to-one chat.

```bash
gdbus call --session --dest cc.etke.komai --object-path / --method cc.etke.komai.directChat '@alice:example.org'
```

Example result:

```text
()
```

### setStatusMessage

Sets the user status message.

```bash
gdbus call --session --dest cc.etke.komai --object-path / --method cc.etke.komai.setStatusMessage '📣 status from D-Bus'
```

Example result:

```text
()
```

### setTheme

Changes the active theme.

```bash
gdbus call --session --dest cc.etke.komai --object-path / --method cc.etke.komai.setTheme 'komai-dark'
```

Example result:

```text
()
```

`setTheme` expects a valid theme slug from Komai’s theme list (`komai-light`, `komai-dark`, `classic-light`, ...), not a display label.

## 🛰️ Service details

- **D-Bus service:** `cc.etke.komai`
- **Object path:** `/`
- **Interface:** `cc.etke.komai`

Discovery check:

```bash
gdbus introspect --session --dest cc.etke.komai --object-path /
```

## 🔐 Security notes

- Access is local-only to users that can talk to your current session bus.
- Access is controlled by **Settings → Integrations → D-Bus**.
- Keep these switches enabled only when needed for your workflow.
