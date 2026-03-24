# 🤖 MCP Server

Komai can expose a running [application profile](../application-profiles.md) as a local [Model Context Protocol](https://modelcontextprotocol.io/) (MCP) server over **stdio**.

MCP is an open protocol for connecting apps to MCP-compatible hosts, including MCP clients like [Claude Code](#claude-code), [Codex](#codex), and others. In Komai, that means a host can discover tools, read room and account data, fetch media, and, when allowed, trigger actions in the running app.

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
- `user_get_id`
- `user_get_homeserver_url`
- `user_get_device_id`
- `user_get_status_message`
- `settings_ui_get_theme`
- `media_fetch_image`

### `read_write`

Both read and write tools are advertised.

Examples of additional write tools:

- `rooms_activate`
- `rooms_join`
- `rooms_new_direct_chat`
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
- `limit` -- optional, default `50`, max `500`
- `beforeEventId` -- optional exclusive pagination anchor
- `includeUnsignedFields` -- optional, default `false`
- `fetchMode` -- optional, `cached_only` or `server_fetch_if_needed`

Structured result fields:

- `roomId`
- `events`
- `hasMore`
- `nextBeforeEventId`

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
- `rooms_list` returns `structuredContent` with a `rooms` field, and each room includes explicit fields such as `read`, `serverNotificationCount`, `memberCount`, `mostRecentEventTimestampMs`, `highlighted`, `categories`, and `tags`
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
