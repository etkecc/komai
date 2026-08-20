# 🤖 MCP Server

Komai can expose a running [application profile](../application-profiles.md) as a local [Model Context Protocol](https://modelcontextprotocol.io/) (MCP) server over **stdio**.

MCP is an open protocol for connecting apps to MCP-compatible hosts, including MCP clients like [Claude Code](#claude-code), [Codex](#codex), and others. In Komai, that means a host can discover tools, read room and account data, fetch media, and, when allowed, trigger actions in the running app.

See a [🖼️ Screenshot of an MCP host (Codex) talking to Komai](../../screenshots/automations-mcp-codex.webp).

> **Need shell scripts or quick terminal automation?** Use the [CLI commands](cli.md).
>
> **Need a classic desktop IPC API?** Use the [D-Bus API](dbus.md).
>
> **Want the big picture first?** Start with the [Automations overview](README.md).

## ✨ Quick start

1. Start Komai with the [application profile](../application-profiles.md) you want to expose.
2. Pick an [access mode](#-access-modes) — `read_only` (default) is safest; use `read_write` only if the host should send messages or change settings.
3. Register `komai mcp serve` in your MCP host — see [Claude Code](#claude-code), [Codex](#codex), or [other hosts](#other-mcp-hosts).

## 🚦 Current capabilities

- stdio transport on Linux, macOS, and Windows
- MCP tools only (no resources or prompts yet)
- default access mode: `read_only`
- does not require [D-Bus](dbus.md) to be enabled

Not supported yet: HTTP transport, auth, GUI settings for MCP.

## 👥 Running instance required

Komai must already be running with the target [application profile](../application-profiles.md), just like the [CLI commands](cli.md). The `--profile` flag decides which running instance your MCP host talks to.

If the profile is not running, MCP initialization and `tools/list` still succeed, but tool calls fail with `isError: true`.

## 🔐 Access modes

### `read_only` (default)

Only read tools are advertised in `tools/list`. This is the safest default for MCP hosts.

Examples:

- `app_get_version`
- `app_get_api_version`
- `rooms_list`
- `rooms_get_timeline`
- `rooms_get_state`
- `user_get_id`
- `user_get_homeserver_url`
- `user_get_device_id`
- `user_get_status_message`
- `settings_ui_get_theme`
- `media_fetch_image`

### `read_write`

Both read and write tools are advertised.

Examples of additional write tools:

- `rooms_create`
- `rooms_join`
- `rooms_set_state`
- `rooms_set_name`
- `rooms_set_topic`
- `rooms_set_power_level`
- `rooms_leave`
- `rooms_new_direct_chat`
- `rooms_invite`
- `rooms_kick`
- `rooms_ban`
- `rooms_unban`
- `rooms_send`
- `rooms_send_image_file`
- `rooms_send_image`
- `user_set_status_message`
- `settings_ui_set_theme`
- `media_upload_file`

`read_only` is a convenience boundary for local desktop tooling, not a hard security boundary.

## 🧩 Using it from an MCP host

Most MCP hosts only need a command and an argument list. Use an absolute path for locally launched servers.

If you are not sure where to start, keep the host in `read_only`, confirm the tool list looks right, and only then opt into `read_write`.

### `rooms_get_timeline`

Reads visible timeline events from a room, newest first. The tool keeps each event's original
Matrix `content` object and selected envelope fields such as `event_id`, `type`, `sender`,
`origin_server_ts`, and `state_key` when present.

Arguments:

- `roomIdOrAlias` -- required room ID or alias
- `limit` -- optional, default `10`, max `500`
- `beforeEventId` -- optional exclusive pagination anchor
- `includeUnsignedFields` -- optional, default `false`
- `fetchMode` -- optional, `cached_only` (the default) or `server_fetch_if_needed`.
  `cached_only` reads only what the running Komai profile already holds, so a room it has
  not opened this session can come back with no events at all. Pass `server_fetch_if_needed`
  to let it reach the homeserver for history it does not have

Structured result fields:

- `roomId`
- `events`
- `hasMore`
- `nextBeforeEventId`

### Sending messages

`rooms_send`, `rooms_send_image`, `rooms_send_image_file` and `rooms_set_state` all return the
`eventId` of the event they created. That is the real Matrix event ID, so a caller that posts a
bot command can match the reply to its own message instead of guessing by recency.

Getting it means automation sends go straight to the homeserver rather than through the offline
send queue the app itself uses. Two consequences worth knowing:

- A send that fails comes back as an error rather than being retried in the background. For a
  caller waiting on a result that is the useful behavior, but it does mean automation sends are
  not queued while you are offline.
- The message appears in the Komai window when sync returns it, rather than immediately as a
  local echo.

### `rooms_list`

Lists joined rooms. Every argument is optional, and filters combine with AND.

- `ids` -- room IDs or aliases to restrict the result to. The cheapest way to turn a handful of
  known room IDs into names.
- `query` -- case-insensitive substring matched against the room name and alias
- `isDm`, `encrypted` -- keep only rooms that are, or are not, each of those
- `tag` -- keep only rooms carrying a Matrix room tag, such as `m.favourite`
- `parentSpace` -- keep only children of a given space room ID
- `minMemberCount` -- keep only rooms with at least this many joined members
- `limit` (default `50`, max `1000`) and `offset` -- paging
- `fields` -- the keys to keep on each room. A room has 14 keys; most callers want `id` and
  `name`. An unknown key is rejected rather than silently ignored, so typos surface.

The result carries `matchCount` alongside `rooms`: how many rooms matched the filters, counted
*before* `limit` and `offset` were applied. It is the joined-room total only when you pass no
filters. When it exceeds the rooms returned you are looking at a subset, and the tool says so in
its text response.

Unlike the [CLI](cli.md), the tool applies a default `limit`. An unbounded list is large enough
on a real account to overflow an MCP host's tool-result cap.

Rooms come back in the room list's own order, which is by recent activity. Paging a busy account
is therefore a snapshot, not a stable cursor: a room can move between pages if it sees traffic
mid-walk. Filter with `ids` or `query` when you need exactness.

### `rooms_create`

Creates a room or a space and returns its room ID. Every argument is optional -- calling it with
none creates a private, unencrypted room with no name.

The straightforward ones are `name`, `topic`, `aliasLocalpart` (the local part only, so `team`,
not `#team:example.org`), `invite` (a list of user IDs), and the `isDirect` / `isEncrypted` /
`isSpace` / `isPublic` switches. `preset` accepts `private_chat` (the default), `public_chat` or
`trusted_private_chat`; the last grants invitees the creator's power level, which is how you give
someone co-equal standing in a room.

Three arguments are passed to the homeserver untouched, so any field Matrix defines for them
works without waiting on Komai:

- `powerLevelContentOverride` -- an `m.room.power_levels` content object. This is the only way to
  give a co-moderator a power level atomically with the room, rather than in a follow-up event.
- `initialState` -- a list of state events, each with a `type`, an optional `state_key` and a
  `content`. Use it for state that has to be right *before* anyone joins; history visibility is
  the usual example, since setting it afterwards has already exposed the earlier events.
- `creationContent` -- additions to `m.room.create`, such as `{"m.federate": false}`. This one
  cannot be changed after creation at all.

`roomVersion` requests a specific room version; omit it to take the server's default.

Komai does not validate the contents of the three raw fields beyond checking that they are the
right JSON shape. The homeserver rejects anything malformed, and the error comes back verbatim.

### State event tools

`rooms_get_state` and `rooms_set_state` reach any room state event, including custom types that
Matrix itself does not define. Both take `roomIdOrAlias`, an `eventType`, and an optional
`stateKey` that defaults to the empty string, which is what most state events use.

`rooms_get_state` answers `{"exists": bool, "content": {...}}`. A room that simply has no such
state comes back as `exists: false` rather than an error, since "is this set?" is a fair question.

The read always goes to the homeserver rather than Komai's local cache. Komai syncs via sliding
sync, which only fetches the state types the room list asks for, so a custom type would otherwise
read back as missing even when the room has it.

`rooms_set_state` takes a `content` object and returns the `eventId` of the state event it sent.

> **`content` replaces, it does not merge.** Whatever you send becomes the entire content of that
> state event. Read the current content first and send back a complete object, or you will drop
> the keys you left out.

That trap is sharpest for `m.room.power_levels`, where a content of
`{"users": {"@alice:example.org": 100}}` removes everyone else's power level and every custom
event threshold in the room. Use `rooms_set_power_level` instead: it reads the current levels,
changes the one user, and writes the whole thing back.

`rooms_set_name` and `rooms_set_topic` are the same idea for the two most common fields; passing
an empty string clears the field rather than failing.

### Membership tools

`rooms_invite`, `rooms_kick`, `rooms_ban` and `rooms_unban` act on another user in a room.
Each takes a required `roomIdOrAlias` and `userId`, plus an optional `reason` that is recorded
on the resulting membership event (and shown to the invitee, for an invite).

`rooms_leave` acts on your own account instead, so it only takes `roomIdOrAlias` and an optional
`reason`. It also rejects a pending invite to the room. The room is not forgotten, so it stays
visible in your left-rooms history.

All five need `read_write` access, and they succeed only if your account has enough power in the
room. `rooms_kick`, `rooms_ban` and `rooms_leave` are marked destructive, so a host that asks for
confirmation before destructive tools will prompt on those three.

Two behaviors worth knowing:

- `rooms_kick` removes a user but does not stop them rejoining. Use `rooms_ban` for that.
- `rooms_unban` lifts a ban but does not re-invite the user or bring them back into the room.

### Claude Code

[Claude Code](https://docs.anthropic.com/en/docs/claude-code) has three scopes for MCP servers:

| Scope | File | Shared? | Reach |
|-------|------|---------|-------|
| **local** (default) | `~/.claude.json` (keyed by project path) | no | this project only |
| **project** | `.mcp.json` in project root | yes, committed to git | this project, whole team |
| **user** | `~/.claude.json` | no | all projects on your machine |

Most users will want `user` scope so Komai tools are available in every project:

```bash
claude mcp add --scope user komai-default -- /absolute/path/to/komai mcp serve --profile default --access read_only
```

Or edit `~/.claude.json` directly:

```json
{
  "mcpServers": {
    "komai-default": {
      "type": "stdio",
      "command": "/absolute/path/to/komai",
      "args": ["mcp", "serve", "--profile", "default", "--access", "read_only"]
    }
  }
}
```

Run `/mcp` inside Claude Code to verify the server is connected.

### Codex

[Codex](https://github.com/openai/codex) config goes in `~/.codex/config.toml` (global) or `.codex/config.toml` in your project root:

```toml
[mcp_servers.komai-default]
command = "/absolute/path/to/komai"
args = ["mcp", "serve", "--profile", "default", "--access", "read_only"]
```

Or use the CLI:

```bash
codex mcp add komai-default -- /absolute/path/to/komai mcp serve --profile default --access read_only
```

The server name includes the profile (`komai-default`) so you can add multiple profiles side by side (e.g. `komai-work`).

See a [🖼️ Screenshot of Codex talking to Komai over MCP](../../screenshots/automations-mcp-codex.webp).

### Other MCP hosts

For hosts that accept a generic JSON configuration:

```json
{
  "mcpServers": {
    "komai-default": {
      "command": "/absolute/path/to/komai",
      "args": ["mcp", "serve", "--profile", "default", "--access", "read_only"]
    }
  }
}
```

## 🧪 Manual testing with MCP Inspector

The easiest way to inspect the server manually is the MCP Inspector. Use an absolute path to the `komai` binary:

```bash
npx -y @modelcontextprotocol/inspector /absolute/path/to/komai mcp serve --profile default --access read_only
```

Keep that terminal running. Inspector uses a browser UI, so the terminal mostly shows logs. If a browser tab does not open automatically, look for the local `http://127.0.0.1:...` or `http://localhost:...` URL in the terminal output and open it manually.

From the browser UI:

- connect to the server
- let Inspector handle MCP initialization
- open the tools view
- run a simple read tool such as `app_get_version`, `rooms_list`, or `rooms_get_timeline`

Verify the following in Inspector:

- `initialize` succeeds
- `tools/list` shows the expected read-only catalog
- `rooms_list` returns `structuredContent` with a `rooms` field, and each room includes explicit fields such as `read`, `unreadCount`, `memberCount`, `mostRecentEventTimestampMs`, `highlighted`, `categories`, and `tags`
- `rooms_get_timeline` returns `structuredContent.events` ordered newest first, plus `hasMore` and `nextBeforeEventId` for pagination
- `media_fetch_image` returns image content
- tool failures return `isError: true`

Then repeat with read-write access:

```bash
npx -y @modelcontextprotocol/inspector /absolute/path/to/komai mcp serve --profile default --access read_write
```

Verify that write tools appear in `tools/list`.

## 🔗 Choosing between automation surfaces

Komai now has three local automation surfaces:

- [🤖 MCP Server](mcp.md) - best for MCP-compatible hosts and AI tooling
- [💻 CLI Commands](cli.md) - best for terminal use, shell scripts, and `jq`
- [🔌 D-Bus API](dbus.md) - best for applications that already use D-Bus libraries

They all target the same running Komai profile, but package the automation interface differently.
