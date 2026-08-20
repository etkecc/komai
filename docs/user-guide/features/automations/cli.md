# 💻 CLI Commands

Komai can be controlled from the terminal through its [command-line interface](https://en.wikipedia.org/wiki/Command-line_interface) (CLI). CLI commands talk to a running Komai instance over a local Unix socket, so the target instance must be running. No display server is needed on the machine running the command.

> **Looking for the overview?** Start with [Automations](README.md) for a quick guide to MCP, CLI, and D-Bus.

> **Need MCP host integration?** See the [MCP server guide](mcp.md). MCP uses the same running-instance IPC backend as the CLI, but exposes it through the Model Context Protocol over stdio.

> **Prefer a D-Bus library?** The same operations are also available through the [D-Bus API](dbus.md) for use from any programming language.

## 📤 Output format

Read commands output **JSON** to stdout. Write commands produce no output on success (exit code 0). Errors go to stderr as plain text (exit code 1).

Use [`jq`](https://jqlang.github.io/jq/) to extract values:

```bash
# Raw value
komai app version | jq -r '.version'

# Pretty-print
komai rooms list | jq .

# Filter
komai rooms list | jq '[.rooms[] | select(.read == false)]'
```

## 👥 Profile targeting

Each [application profile](../application-profiles.md) listens on its own socket. Use `-p` to target a specific profile:

```bash
komai -p work rooms list
```

If `-p` is omitted, the `default` profile is used.

## ⌨️ Shell completions

Komai's bash, zsh, and fish completion scripts are installed alongside the binary by its packages. On a typical distro install, open a new shell and `komai <TAB>` just works.

If tab completion isn't picked up (source build, non-standard shell dir, ...), regenerate it yourself:

```bash
komai completions bash > ~/.local/share/bash-completion/completions/komai
komai completions zsh  > ~/.local/share/zsh/site-functions/_komai
komai completions fish > ~/.config/fish/completions/komai.fish
```

## 📡 App

Instance metadata.

### version

```bash
komai app version
# {"version":"0.1.0"}
```

### api-version

```bash
komai app api-version
# {"apiVersion":"1.0.0"}
```

## 🚪 Rooms

Room discovery and navigation.

### list

Returns joined rooms as `{"rooms": [...], "matchCount": n}`, where `matchCount` is how many
rooms matched the filters, counted before `--limit` and `--offset` were applied. With no filters
that is every joined room; with filters it is the size of the match, not of your account.

Each room summary includes:

- `read` -- Komai's local room-list read state
- `unreadCount` -- locally-tracked unread message count
- `memberCount` -- joined member count from Komai's cached room metadata
- `mostRecentEventTimestampMs` -- best-known most recent room event timestamp in Unix milliseconds
- `highlighted` -- whether the room currently has a highlight
- `categories` -- derived labels such as `direct`, `person`, `bot`, `group`, `space`, `encrypted`
- `tags` -- Matrix room tags such as `m.favourite` or `m.lowpriority`
- `parentSpaces` -- parent space room IDs
- `dmUserId` -- direct-chat partner user ID when the room is a DM
- `encrypted` -- whether the room is end-to-end encrypted

Draft state is intentionally not exposed here.

Several filters are built in, so common questions do not need `jq` at all. They combine with
AND, and `--fields` trims each room down to the keys you asked for:

| Flag | Effect |
|---|---|
| `--ids <ids>` | Restrict to these room IDs or aliases, comma-separated |
| `--query <text>` | Case-insensitive substring on room name and alias |
| `--is-dm true\|false` | Keep only direct chats, or only non-direct chats |
| `--encrypted true\|false` | Keep only encrypted, or only unencrypted, rooms |
| `--tag <tag>` | Keep only rooms carrying a room tag |
| `--parent-space <room-id>` | Keep only children of a space |
| `--min-member-count <n>` | Keep only rooms with at least this many members |
| `--limit <n>`, `--offset <n>` | Paging; all rooms if `--limit` is unset |
| `--fields <keys>` | Comma-separated keys to keep on each room |

Rooms come back in the room list's own order, which is by recent activity. A paged walk is
therefore a snapshot rather than a stable cursor: a room that sees traffic mid-walk can move
between pages.

See also: [join](#join).

```bash
komai rooms list --limit 1
# {"rooms":[{"id":"!abc:example.org","alias":"#room:example.org","name":"My Room","avatarUrl":"mxc://example.org/abc","read":false,"unreadCount":3,"memberCount":42,"mostRecentEventTimestampMs":1742810400000,"highlighted":false,"categories":["group","encrypted"],"tags":["m.favourite"],"parentSpaces":["!space:example.org"],"dmUserId":"","encrypted":true}],"matchCount":37}
```

Scripting examples:

```bash
# Room names only
komai rooms list --fields name | jq -r '.rooms[].name'

# Look up names for room IDs you already have
komai rooms list --ids '!abc:example.org,!def:example.org' --fields id,name

# Find a room by alias
komai rooms list --ids '#komai:example.org' --fields id | jq -r '.rooms[].id'

# Small group rooms
komai rooms list --is-dm false | jq '[.rooms[] | select(.memberCount <= 5)]'

# Favourite bot rooms
komai rooms list --tag m.favourite | jq '[.rooms[] | select(.categories | index("bot"))]'

# Rooms with unread timeline activity
komai rooms list | jq '[.rooms[] | select(.read == false)]'

# Rooms quiet since before 2025-01-01 UTC
komai rooms list | jq '[.rooms[] | select(.mostRecentEventTimestampMs < 1735689600000)]'
```

### timeline

Returns visible timeline events from a room as JSON, newest first. Each event keeps its original
Matrix `content` object and selected envelope fields such as `event_id`, `type`, `sender`,
`origin_server_ts`, and `state_key` when present.

By default Komai reads only from the locally cached timeline. Use `--fetch-mode
server_fetch_if_needed` to back-paginate older history from the homeserver until the requested
page is filled or no older history remains.

```bash
komai rooms timeline '!abc:example.org'
komai rooms timeline '#komai:example.org' --limit 100
komai rooms timeline '!abc:example.org' --before-event-id '$older:example.org'
komai rooms timeline '!abc:example.org' --fetch-mode server_fetch_if_needed
komai rooms timeline '!abc:example.org' --include-unsigned-fields
```

Output:

```json
{"roomId":"!abc:example.org","events":[{"content":{"body":"How are you doing?","msgtype":"m.text"},"event_id":"$0Akk93vnCBLjeOoAlu2zUy9Ym7ottC0dF_9AFw1Z_4Y","origin_server_ts":1774304714130,"sender":"@test8:example.org","type":"m.room.message"}],"hasMore":true,"nextBeforeEventId":"$0Akk93vnCBLjeOoAlu2zUy9Ym7ottC0dF_9AFw1Z_4Y"}
```

| Flag | Values | Default | Description |
|---|---|---|---|
| `--limit` | `1..500` | `50` | Maximum number of events to return |
| `--before-event-id` | Matrix event ID | | Exclusive pagination anchor; returns older events |
| `--include-unsigned-fields` | flag | off | Include Matrix `unsigned` event fields |
| `--fetch-mode` | `cached_only`, `server_fetch_if_needed` | `cached_only` | Read only from local cache or fetch older history when needed |

Pagination example:

```bash
# First page
komai rooms timeline '!abc:example.org' --limit 50 | jq .

# Next older page
komai rooms timeline '!abc:example.org' \
  --before-event-id "$(komai rooms timeline '!abc:example.org' --limit 50 | jq -r '.nextBeforeEventId')" \
  --limit 50 | jq .
```

### join

Joins a room by room ID or alias. See also: [list](#list).

```bash
komai rooms join '#komai:example.org'
```

### new-direct-chat

Starts or opens a one-to-one chat with a user.

```bash
komai rooms new-direct-chat '@alice:example.org'
```

### send

Sends a text or notice message to a room. Returns the event ID as JSON.

```bash
komai rooms send '!abc:example.org' 'Hello, world!'
komai rooms send '#room:example.org' 'Hello, world!'
komai rooms send '!abc:example.org' 'Server alert' --msgtype notice
komai rooms send '!abc:example.org' '**bold** text' --format html
komai rooms send '!abc:example.org' 'plain text only' --format plain
```

Multi-word messages are joined automatically:

```bash
komai rooms send '!abc:example.org' hello world
# sends "hello world"
```

Output:

```json
{"eventId":"$abc123:example.org"}
```

| Flag | Values | Default | Description |
|---|---|---|---|
| `--msgtype` | `text`, `notice` | `text` | Message type |
| `--format` | `auto`, `plain`, `html` | `auto` | Markdown handling |

- `auto` — follows the user's Markdown setting in Komai preferences
- `plain` — sends the body as-is, no formatting
- `html` — always converts Markdown to HTML

### send-image

Uploads and sends an image to a room. Handles encryption transparently for encrypted rooms. Computes image metadata (dimensions, blurhash, thumbnail) automatically.

```bash
# Upload from disk and send (recommended; works with encrypted rooms)
komai rooms send-image '!abc:example.org' /path/to/photo.jpg
komai rooms send-image '!abc:example.org' /path/to/photo.jpg --caption "Look at this"

# From a previously-uploaded mxc:// URI (unencrypted rooms only)
komai rooms send-image '!abc:example.org' mxc://hs/abc --filename photo.jpg --caption "Look"
```

The CLI detects whether the second argument is a file path or `mxc://` URI:
- Starts with `mxc://` -- uses the pre-uploaded URI (unencrypted rooms only)
- Otherwise -- treats as file path, uploads and encrypts if needed

Output:

```json
{"eventId":"$img789:example.org"}
```

| Flag | Description |
|---|---|
| `--caption <text>` | Image caption (defaults to filename) |
| `--filename <name>` | Filename (required with `mxc://` URI) |

## 👤 User

Account and presence.

### id

Returns the logged-in user's Matrix ID.

```bash
komai user id
# {"userId":"@alice:example.org"}
```

### homeserver-url

Returns the homeserver URL.

```bash
komai user homeserver-url
# {"homeserverUrl":"https://example.org"}
```

### device-id

Returns the device ID for the current session.

```bash
komai user device-id
# {"deviceId":"ABCDEF1234"}
```

### status / set-status

Reads or sets the user's presence status text.

```bash
# Get
komai user status
# {"statusMessage":"brb"}

# Set
komai user set-status 'brb'
```

Unquoted multi-word messages are joined automatically:

```bash
komai user set-status away for lunch
```

## ⚙️ Settings

Appearance settings.

### ui theme / ui set-theme

Reads or sets the active theme. `set-theme` expects a valid theme slug (`light-komai`, `dark-komai`, `light-nheko`, ...), not a display label.

```bash
# Get
komai settings ui theme
# {"theme":"dark-komai"}

# Set
komai settings ui set-theme dark-komai
```

## 🖼️ Media

Media content resolution. Resolves Matrix content URIs (`mxc://`) into image data through Komai's authenticated session.

### fetch

Fetches an image by its `mxc://` URI and writes PNG data to stdout. Useful for resolving room avatars returned by [`rooms list`](#list).

```bash
komai media fetch 'mxc://example.org/abc123' > avatar.png
```

### upload

Uploads a file to the homeserver and returns its `mxc://` URI. Uploads are not end-to-end encrypted. To send media to an encrypted room, use [`rooms send-image`](#send-image) with a file path instead.

```bash
komai media upload /path/to/photo.png
komai media upload /path/to/file.pdf --filename report.pdf --content-type application/pdf
cat photo.png | komai media upload --stdin --filename photo.png --content-type image/png
```

Output:

```json
{"mxcUri":"mxc://example.org/abc123","contentType":"image/png","filename":"photo.png","size":204800}
```

| Flag | Description |
|---|---|
| `--filename <name>` | Override the filename (required with `--stdin`) |
| `--content-type <mime>` | Override the MIME type |
| `--stdin` | Read from stdin instead of a file path |

## 🔧 Offline commands

The following commands work without a running Komai instance:

| Command | Description |
|---|---|
| `komai theme list` | List all available themes |
| `komai theme create-sample <variant> <name>` | Create a starter theme YAML |
| `komai theme tinted-import <slug> [name]` | Import a Base16 theme from tinted-theming |
| `komai theme tinted-search [query]` | Search available Base16 themes |

## ⚠️ Error handling

If no Komai instance is running for the target profile, CLI commands print an error to stderr and exit with code 1:

```
Error: no running Komai instance for profile 'work'
Start Komai first: komai -p work
```

## 🔐 Security

CLI commands use a per-profile Unix socket and do not require enabling [D-Bus access](../../settings/integrations/dbus.md) in Settings. The socket's filesystem permissions restrict access to the owning user, which is the same trust boundary as running the Komai binary itself. If you can execute `komai`, you already have full access to the user's session, config files, and credentials.

This is distinct from the [D-Bus API](dbus.md), which can be called by any process on the session bus. The CLI channel is not exposed on D-Bus and is not affected by the [D-Bus access setting](../../settings/integrations/dbus.md).
