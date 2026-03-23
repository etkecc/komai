# 🤖 MCP Server

Komai can expose a running profile as a local [Model Context Protocol](https://modelcontextprotocol.io/) (MCP) server over **stdio**.

MCP is an open protocol for connecting apps to MCP-compatible hosts, including AI assistants, editors, and local automation tools. In Komai, that means a host can discover tools, read room and account data, fetch media, and, when allowed, trigger actions in the running app.

> **Need shell scripts or quick terminal automation?** Use the [CLI commands](cli.md).
>
> **Need a classic desktop IPC API?** Use the [D-Bus API](dbus.md).
>
> **Want the big picture first?** Start with the [Automations overview](README.md).

## ✨ Quick start

1. Start Komai with the profile you want to expose.
2. Point your MCP host at `komai mcp serve`.
3. Start with `read_only`, then switch to `read_write` only if the host should send messages or change settings.

Default profile:

```bash
komai mcp serve
```

Specific profile:

```bash
komai -p work mcp serve
komai mcp serve -p work
komai mcp serve --profile work
```

Read-write mode:

```bash
komai mcp serve --access read_write
```

## 🚦 What Komai supports today

The current implementation is intentionally narrow:

- transport: stdio only
- capabilities: tools only
- backend boundary: Komai's existing local IPC transport
- default access mode: `read_only`
- supported platforms: Linux, macOS, and Windows

Not included yet:

- HTTP transport
- auth
- resources
- prompts
- tasks
- GUI settings for MCP

## 🔧 How it works

`komai mcp serve` is a thin wrapper around the Rust `komai-mcp` helper.

The wrapper:

- appears in `komai --help`
- honors Komai's normal `-p` / `--profile` semantics
- forwards the resolved profile and access mode to `komai-mcp`

The Rust helper:

- speaks MCP over stdio
- exposes Komai automation as MCP tools
- talks to the running Komai instance through the same local IPC surface used by the [CLI](cli.md)

MCP does **not** depend on D-Bus being enabled.

It also does **not** shell out to CLI subcommands like `komai rooms list`, and it does **not** call Qt/C++ internals through Rust FFI.

## 👥 Running instance required

The MCP server is only the frontend. The target Komai profile must already be running, just like the regular [CLI commands](cli.md).

Each [application profile](../application-profiles.md) has its own local automation endpoint, so `--profile` decides which running profile your MCP host talks to.

If the profile is not running:

- MCP initialization still succeeds
- `tools/list` still works
- tool calls fail with `isError: true`

## 🔐 Access modes

### `read_only` (default)

Only read tools are advertised in `tools/list`. This is the safest default for MCP hosts.

Examples:

- `app_get_version`
- `app_get_api_version`
- `rooms_list`
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

Most MCP hosts only need a command and an argument list. Use an absolute path for locally launched servers:

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

If you are not sure where to start, keep the host in `read_only`, confirm the tool list looks right, and only then opt into `read_write`.

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
- run a simple read tool such as `app_get_version` or `rooms_list`

Verify the following in Inspector:

- `initialize` succeeds
- `tools/list` shows the expected read-only catalog
- `rooms_list` returns `structuredContent` with a `rooms` field
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
