# 🔌 D-Bus integration setting

Komai can expose a local [D-Bus](https://www.freedesktop.org/wiki/Software/dbus/) API so external tools can interact with your running session.

This is controlled in **Settings → Integrations → D-Bus** with the **D-Bus access** dropdown.

## Access levels

- `integrations.dbus.access`:
  - `none` *(default)* -- D-Bus API is not registered
  - `read_only` -- external tools can query rooms, status, version, etc.
  - `read_write` -- external tools can also switch rooms, join rooms, change themes, etc.

No restart is needed when changing it.

> **Note:** This setting only affects the D-Bus API. The [CLI](../../automations/cli.md) and [MCP server](../../automations/mcp.md) use separate local channels and are not controlled by this setting. For the full automation overview, see [Automations](../../automations/README.md).

## What's exposed

Each [application profile](../../application-profiles.md) registers its own D-Bus service (e.g. `cc.etke.komai.profile.default`), so multiple profiles can be targeted independently.

For the full list of available methods, examples, and multi-profile usage, see the [D-Bus API guide](../../automations/dbus.md).
