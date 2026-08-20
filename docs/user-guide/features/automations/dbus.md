# 🔌 D-Bus API

Komai exposes a local [D-Bus](https://www.freedesktop.org/wiki/Software/dbus/) API so external tools can interact with your running session -- query rooms, switch themes, open chats, and more.

D-Bus is the standard inter-process communication (IPC) bus on Linux desktops: applications register named services and other local processes -- scripts, panel widgets, desktop shells, language bindings -- call methods or listen for signals without writing transport code. See [Wikipedia: D-Bus](https://en.wikipedia.org/wiki/D-Bus) for background.

> **Looking for the overview?** Start with [Automations](README.md) for a quick guide to MCP, CLI, and D-Bus.

> **Need MCP host integration?** See the [MCP server guide](mcp.md). MCP is stdio-based, uses the same running-instance automation surface, and does not require D-Bus to be enabled.

> **Before you start:** D-Bus integration must be enabled in [Settings → Integrations → D-Bus](../../settings/integrations/dbus.md).

> **Just need a quick command?** The [CLI](cli.md) provides the same operations as ready-made commands with human & agent-friendly JSON output -- no D-Bus library needed. CLI commands are always available regardless of the D-Bus access setting.

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

> **Note:** unlike the [CLI](cli.md) and [MCP](mcp.md) room listings, this returns every joined
> room with every field. The filtering added there relies on projecting away struct members, which
> a fixed D-Bus signature cannot express -- and a D-Bus client can filter the typed array locally
> at no cost. Nothing is unreachable here; there is just no server-side narrowing.


Returns all joined rooms with explicit local read state, notification/highlight state, derived categories, Matrix tags, parent-space IDs, DM partner metadata, and encryption state. Draft state is intentionally not exposed.

Returned struct fields are ordered as:
`id`, `alias`, `name`, `avatarUrl`, `read`, `unreadCount`, `memberCount`, `mostRecentEventTimestampMs`, `highlighted`, `categories`, `tags`, `parentSpaces`, `dmUserId`, `encrypted`.

See also: [join](#join).

> Required D-Bus access level: 👁️ read

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms list
```

### timeline

Returns a JSON string containing visible timeline events from a room, newest first. The payload
matches the CLI and MCP result shape:

- `roomId`
- `events`
- `hasMore`
- `nextBeforeEventId`

Each event keeps its original Matrix `content` object and selected envelope fields such as
`event_id`, `type`, `sender`, `origin_server_ts`, and `state_key` when present. `unsigned` fields
are omitted unless requested.

> Required D-Bus access level: 👁️ read

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms timeline sisbs \
  '!abc:example.org' 50 '' false 'cached_only'
```

| Parameter | Type | Default | Description |
|---|---|---|---|
| `roomIdOrAlias` | string | *(required)* | Room ID or alias |
| `limit` | int32 | `50` | Maximum number of events to return (`1..500`) |
| `beforeEventId` | string | empty | Exclusive pagination anchor; returns older events |
| `includeUnsignedFields` | boolean | `false` | Include Matrix `unsigned` event fields |
| `fetchMode` | string | `cached_only` | `cached_only` or `server_fetch_if_needed` |

`server_fetch_if_needed` starts from the local cache, then back-paginates older history from the
homeserver only if the cached page is too short.

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

### send

Sends a text or notice message to a room. Returns the event ID string.

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms send ssss '!abc:example.org' 'Hello' 'm.text' 'auto'
```

| Parameter | Type | Default | Description |
|---|---|---|---|
| `roomIdOrAlias` | string | *(required)* | Room ID or alias |
| `body` | string | *(required)* | Message text |
| `msgtype` | string | `m.text` | `m.text` or `m.notice` |
| `format` | string | `auto` | `auto`, `plain`, or `html` |

### sendImageFile

Uploads an image from disk and sends it to a room in one step. Handles encryption transparently for encrypted rooms.

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms sendImageFile sss '!abc:example.org' '/path/to/photo.jpg' ''
```

| Parameter | Type | Default | Description |
|---|---|---|---|
| `roomIdOrAlias` | string | *(required)* | Room ID or alias |
| `filePath` | string | *(required)* | Absolute path to the image |
| `body` | string | *(auto)* | Caption (defaults to filename) |

### sendImage

Sends an image using an already-uploaded `mxc://` URI. Only works for unencrypted rooms. Returns an error if the target room is encrypted.

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms sendImage ssss '!abc:example.org' 'mxc://hs/abc' '' 'photo.jpg'
```

| Parameter | Type | Default | Description |
|---|---|---|---|
| `roomIdOrAlias` | string | *(required)* | Room ID or alias |
| `mxcUri` | string | *(required)* | The `mxc://` URI from `media.upload` |
| `body` | string | *(auto)* | Caption (defaults to filename) |
| `filename` | string | | Original filename |

### invite / kick / ban / unban

Membership operations targeting another user. All four take the same arguments; `reason` may be
empty. `kick` and `ban` are, obviously, not reversible by re-running them -- use `unban` to lift a
ban, which does not re-invite the user.

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms invite sss '!abc:example.org' '@alice:example.org' 'joining the review'
```

| Parameter | Type | Default | Description |
|---|---|---|---|
| `roomIdOrAlias` | string | *(required)* | Room ID or alias |
| `userId` | string | *(required)* | Matrix user ID to act on |
| `reason` | string | | Recorded on the membership event |

### leave

Leaves a room, or rejects a pending invite to it. The room is not forgotten.

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms leave ss '!abc:example.org' ''
```

### create

Creates a room or space and returns its room ID.

The options arrive as a **JSON object string** rather than a dozen typed arguments. Matrix keeps
adding fields to `createRoom`, and a typed signature could not grow without a breaking API bump.
Every key is optional; the accepted set is the same as the
[`rooms_create` MCP tool](mcp.md#rooms_create).

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms create s \
  '{"name":"Review","preset":"trusted_private_chat","invite":["@alice:example.org"]}'
```

### getState / setState

Reads or writes one room state event, including custom event types. `getState` returns
`{"exists": bool, "content": {...}}` as a JSON string; `setState` returns the event ID it sent.
`stateKey` is usually empty.

> Required D-Bus access level: 👀 read for `getState`, ✏️ write for `setState`

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms getState sss '!abc:example.org' 'm.room.topic' ''
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms setState ssss '!abc:example.org' 'm.room.topic' '' '{"topic":"Hello"}'
```

> **`setState` replaces the state event, it does not merge into it.** For `m.room.power_levels`
> that means an object naming one user drops every other level in the room. Use `setPowerLevel`.

### setName / setTopic / setPowerLevel

Named wrappers for the three most common changes. An empty `name` or `topic` clears the field.
`setPowerLevel` reads the current levels, changes the one user, and writes them all back.

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms setTopic ss '!abc:example.org' 'Release planning'
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms setPowerLevel ssi '!abc:example.org' '@alice:example.org' 50
```

### redact

Redacts an event. Returns the **redaction's own event ID**, which is a different event from the
one being redacted.

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms redact sss '!abc:example.org' '$event:example.org' 'spam'
```

### markRead / markUnread

`markRead` marks a room read up to `eventId`, or up to its latest event when `eventId` is empty.
`receipt` is `public`, `private`, or empty to follow your own read-receipt setting for that room.

`markUnread` sets or clears the manual "leave this for later" flag, which is separate from having
unread messages.

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms markRead sss '!abc:example.org' '' ''
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms markUnread sb '!abc:example.org' true
```

### readReceipts

Lists who has read at or past an event, as a JSON string. Your own receipt is never included, so
an empty list means nobody else has read that far.

> Required D-Bus access level: 👀 read

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Rooms readReceipts ss '!abc:example.org' '$event:example.org'
```

## 👤 User

Account and presence.

### userId

Returns the logged-in user's Matrix ID.

> Required D-Bus access level: 👁️ read

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.User userId
```

### homeserverUrl

Returns the homeserver URL.

> Required D-Bus access level: 👁️ read

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.User homeserverUrl
```

### deviceId

Returns the device ID for the current session.

> Required D-Bus access level: 👁️ read

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.User deviceId
```

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

Reads or sets the active theme. `setTheme` expects a valid theme slug (`light-komai`, `dark-komai`, `light-nheko`, ...), not a display label.

> Required D-Bus access level: 👁️ read (get) / ✏️ write (set)

```bash
# Get
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Settings.UI theme

# Set
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Settings.UI setTheme s 'dark-komai'
```

## 🖼️ Media

Media content resolution. Resolves Matrix content URIs (`mxc://`) into image data through Komai's authenticated session.

### fetch

Fetches an image by its `mxc://` URI. Useful for resolving room avatars returned by [Rooms.list](#list).

> Required D-Bus access level: 👁️ read

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Media fetch s 'mxc://example.org/abc123'
```

### upload

Uploads a file (unencrypted) to the homeserver and returns its `mxc://` URI. Uploads are not end-to-end encrypted. To send media to an encrypted room, use [Rooms.sendImageFile](#sendimagefile) with a file path instead.

> Required D-Bus access level: ✏️ write

```bash
busctl --user call cc.etke.komai.profile.default / cc.etke.komai.Media upload sss '/path/to/photo.png' '' ''
```

| Parameter | Type | Default | Description |
|---|---|---|---|
| `filePath` | string | *(required)* | Absolute path to the file |
| `filename` | string | *(auto)* | Override the display filename |
| `contentType` | string | *(auto)* | Override the MIME type |

## 🔐 Security notes

- Access is local-only to users that can talk to your current session bus.
- Access is controlled per profile by **Settings → Integrations → D-Bus** ([details](../../settings/integrations/dbus.md)).
- Keep D-Bus access enabled only when needed for your workflow.
