# Built-in Themes

This directory contains the built-in theme YAML files that are compiled into the Komai binary at build time.

Each `.yml` file defines 16 Qt palette colors and 4 app-level semantic colors, plus a required `userColors` section for bubble/user colors. Each `userColors` slot is a mapping with required `background` and optional `text`, `secondaryText`, and `link`. `secondaryText` is for in-bubble secondary/inactive content, not outer timeline metadata. At build time, these files are embedded into the Rust binary via `include_str!` and parsed at startup.

To import a tinted-theming theme: `just theme-tinted-import <slug>` — see [Architecture: Themes](../../docs/architecture/themes.md#adding-a-new-built-in-theme) for details.

To create a starter theme: `just theme-create-sample dark my-theme` — generates a YAML skeleton here for manual customisation.

To hand-craft a theme: create a `.yml` with all 20 palette keys and a `userColors` section (see [Theme YAML format](../../docs/user-guide/themes.md#-theme-yaml-format)) and place it here, then `just build`.

## Further reading

- [User guide: Themes](../../docs/user-guide/themes.md) — end-user documentation, YAML format reference
- [Architecture: Themes](../../docs/architecture/themes.md) — import pipeline, Base16 mapping, contrast heuristics
- [Architecture: CLI](../../docs/architecture/cli.md) — CLI subcommand dispatch design
