# 🤖 Automations

Komai can be scripted and controlled by external tools. All automation surfaces target a running Komai profile, but they fit different kinds of tooling.

## Pick an interface

- [🤖 MCP Server](mcp.md) -- best for MCP-compatible hosts, including AI assistants, editors, and local automation tools
- [💻 CLI Commands](cli.md) -- best for terminal workflows, shell scripts, and `jq`
- [🔌 D-Bus API](dbus.md) -- best for desktop IPC and languages with D-Bus libraries

## What they share

- They operate on a running Komai instance.
- They can target a specific [application profile](../application-profiles.md).
- They expose overlapping capabilities such as room discovery, account info, settings, and media operations.
- They are local automation surfaces, not remote hosted APIs.

## Which one should I use?

- Choose [🤖 MCP Server](mcp.md) if you want an MCP host to call Komai tools.
- Choose [💻 CLI Commands](cli.md) if you want plain commands and JSON output in a terminal.
- Choose [🔌 D-Bus API](dbus.md) if you want a session-bus API for desktop integrations.

The [D-Bus API](dbus.md) has its own access setting in Komai. The [CLI](cli.md) and [MCP server](mcp.md) use separate local channels and do not depend on D-Bus being enabled.
