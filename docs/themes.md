# 🎨 Themes

Komai uses a data-driven theme system based on the [Base16](https://github.com/tinted-theming/home) specification. Themes are defined as YAML files in `resources/themes/` and compiled into the binary at build time.


## Built-in themes

Komai ships with several built-in themes (see `resources/themes/` for the current list), including Komai light/dark, nheko light/dark, and popular community themes like Catppuccin, Dracula, Gruvbox, Nord, Solarized, and Tokyo Night.

A **System** option is also available, which uses your OS palette instead of a built-in theme.


## Adding a new theme

Drop a `.yaml` file into `resources/themes/` and rebuild. The theme appears in Settings automatically.

```sh
# Import from the tinted-theming/schemes collection
just import-theme rose-pine

# List available themes to import
just import-theme --list

# Rebuild — the new theme appears in the Settings dropdown
just build
```

Any theme from the [tinted-theming/schemes](https://github.com/tinted-theming/schemes) repository works out of the box.


## Theme YAML format

Each theme follows the Base16 spec: 16 hex colors (`base00`–`base0F`) with fixed semantic meanings.

```yaml
system: "base16"
name: "My Theme"
author: "Your Name"
variant: "light"   # or "dark"
palette:
  base00: "ffffff"  # default background
  base01: "eeeeee"  # lighter background
  base02: "dcdcdc"  # selection background
  base03: "555459"  # comments, invisibles
  base04: "555459"  # dark foreground
  base05: "334258"  # default foreground
  base06: "f2f5f8"  # light foreground
  base07: "ffffff"  # lightest foreground
  base08: "a82353"  # red (errors, deletions)
  base09: "f49300"  # orange (integers, bold)
  base0A: "fcbe05"  # yellow (classes, search bg)
  base0B: "008000"  # green (strings, success)
  base0C: "38a3d8"  # cyan (support, regex)
  base0D: "b56e00"  # blue (functions, headings)
  base0E: "334258"  # purple (keywords, tags)
  base0F: "dd3d3d"  # dark red (deprecated)
```

### Custom overrides

For themes that need exact QPalette role values instead of the automatic Base16 mapping, add an `overrides:` section:

```yaml
overrides:
  highlight: "f49300"   # use orange accent instead of base0D
  link: "b56e00"
  error: "dd3d3d"
```

Available override keys: `window`, `windowText`, `base`, `alternateBase`, `text`, `brightText`, `button`, `buttonText`, `light`, `mid`, `dark`, `highlight`, `highlightedText`, `link`, `toolTipBase`, `toolTipText`, `red`, `green`, `orange`, `error`.


## How it works

At build time, CMake runs `bin/generate-themes.py` which reads all `resources/themes/*.yaml` files and generates `src/ui/ThemeDefinitions.h` — a C++ header containing a registry of all theme palettes with inline lookup functions. This header is gitignored as a build artifact.

The `just generate-themes` recipe can also be used to regenerate the header manually.
