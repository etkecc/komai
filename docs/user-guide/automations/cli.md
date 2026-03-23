# CLI Commands

Komai can be controlled from the terminal. CLI commands talk to a running Komai instance over [D-Bus](dbus.md), so the target instance must be running and have D-Bus integration enabled in [Settings → Integrations → D-Bus](../settings/integrations/dbus.md).

CLI commands use `QCoreApplication` (headless) -- no display server is needed on the machine running the command.

## Output format

Read commands output **JSON** to stdout. Write commands produce no output on success (exit code 0). Errors go to stderr as plain text (exit code 1).

Use [`jq`](https://jqlang.github.io/jq/) to extract values:

```bash
# Raw value
komai app version | jq -r '.version'

# Pretty-print
komai rooms list | jq .

# Filter
komai rooms list | jq '[.[] | select(.unreadNotifications > 0)]'
```

## Profile targeting

Each [application profile](../application-profiles.md) registers its own D-Bus service. Use `-p` to target a specific profile:

```bash
komai -p work rooms list
```

If `-p` is omitted, the `default` profile is used.

## App

Instance metadata.

### version

```bash
komai app version
# {"version":"0.1.0"}
```

### api-version

```bash
komai app api-version
# {"apiVersion":"1.0.2"}
```

## Rooms

Room discovery and navigation.

### list

Returns a JSON array of joined rooms.

```bash
komai rooms list
# [{"id":"!abc:example.org","alias":"#room:example.org","name":"My Room","avatarUrl":"mxc://example.org/abc","unreadNotifications":3},...]
```

Scripting examples:

```bash
# Room names only
komai rooms list | jq -r '.[].name'

# Rooms with unread messages
komai rooms list | jq '[.[] | select(.unreadNotifications > 0)]'

# Find a room by alias
komai rooms list | jq -r '.[] | select(.alias == "#komai:example.org") | .id'
```

### activate

Activates (focuses) a room by room ID or alias.

```bash
komai rooms activate '!abc123:example.org'
komai rooms activate '#komai:example.org'
```

### join

Joins a room by room ID or alias.

```bash
komai rooms join '#komai:example.org'
```

### new-direct-chat

Starts or opens a one-to-one chat with a user.

```bash
komai rooms new-direct-chat '@alice:example.org'
```

## User

Account and presence.

### status

```bash
komai user status
# {"statusMessage":"brb"}
```

### set-status

Sets the user's status message.

```bash
komai user set-status 'brb'
```

Unquoted multi-word messages are joined automatically:

```bash
komai user set-status away for lunch
```

## Settings

### ui theme

```bash
komai settings ui theme
# {"theme":"komai-dark"}
```

### ui set-theme

Sets the active theme by slug (`komai-light`, `komai-dark`, `nheko-light`, ...).

```bash
komai settings ui set-theme komai-dark
```

## Media

Media content resolution. Resolves Matrix content URIs (`mxc://`) into image data through Komai's authenticated session.

### fetch

Fetches an image by its `mxc://` URI and writes PNG data to stdout. Useful for resolving room avatars returned by `rooms list`.

```bash
komai media fetch 'mxc://example.org/abc123' > avatar.png
```

## Offline commands

The following commands work without a running Komai instance:

| Command | Description |
|---|---|
| `komai theme list` | List all loaded themes |
| `komai theme create-sample <variant> <name>` | Create a starter theme YAML |
| `komai theme tinted-import <slug> [name]` | Import a Base16 theme from tinted-theming |
| `komai theme tinted-search [query]` | Search available Base16 themes |

## Error handling

If no Komai instance is running for the target profile, CLI commands print an error to stderr and exit with code 1:

```
Error: no running Komai instance for profile 'work'
Start Komai first: komai -p work
```
