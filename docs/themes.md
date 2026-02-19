# 🎨 Themes

Komai uses a data-driven theme system. Built-in themes are defined as YAML files in `resources/themes/` and compiled into the binary at build time. Each YAML file contains resolved QPalette-level colors directly — what you see in the YAML is what the app uses.


## 🧰 Built-in themes

Komai ships with several built-in themes (see `resources/themes/` for the current list), including Komai light/dark, nheko light/dark, and popular community themes like Catppuccin, Dracula, Gruvbox, Nord, Solarized, and Tokyo Night.

A **System** option is also available, which uses your OS palette instead of a built-in theme.


## 🗂️ User themes

In addition to built-in themes, Komai loads custom theme YAML files at startup from XDG data directories. Drop a `.yml` file into one of these paths and restart Komai:

1. `~/.local/share/komai/themes/` — user themes (highest priority)
2. `/usr/local/share/komai/themes/` — locally-installed system themes
3. `/usr/share/komai/themes/` — distro-packaged themes

The filename (without the extension) becomes the theme slug. User themes appear in the Settings dropdown alongside built-in themes.

**Priority rules:**
- Built-in themes always win over external themes with the same slug
- If the same slug exists in multiple directories, the first occurrence wins (user dir beats system dir)
- External themes are sorted after all built-in themes in the dropdown

**Format:** Use the same YAML format as built-in themes (see [Theme YAML format](#theme-yaml-format) below). The `author` and `source_base16` fields are optional and ignored at runtime.

**No hot-reload:** Themes are loaded once at startup. Restart Komai to pick up new or changed theme files.

**Graceful handling:** If a theme file has missing keys, invalid hex values, or other errors, it is skipped with a log warning. Other themes still load normally. If a previously-selected custom theme file is removed, Komai falls back to the system palette.


## ✨ Adding a new theme

### 🌈 Importing from tinted-theming

The `import-theme` command fetches a [Base16](https://github.com/tinted-theming/home) theme and automatically converts it to QPalette colors:

```sh
# Import from the tinted-theming/schemes collection
just import-theme rose-pine

# List available themes to import
just import-theme --list

# Overwrite an existing imported theme
just import-theme rose-pine --force

# Rebuild — the new theme appears in the Settings dropdown
just build
```

### ✍️ Hand-crafted themes

Drop a `.yml` file into `resources/themes/` with all 20 palette keys and rebuild. The theme appears in Settings automatically.


## 🧩 Theme YAML format

Each theme defines exactly 20 palette colors. Unknown keys are rejected by the validator to catch typos.

```yaml
name: "My Theme"
author: "Your Name"
variant: "light"   # or "dark"
palette:
  window: "ffffff"          # main window background
  windowText: "334258"      # main window text
  base: "ffffff"            # input field / list background
  alternateBase: "eeeeee"   # alternating row background
  text: "334258"            # input field / list text
  brightText: "f2f5f8"      # bright text (hover states)
  button: "ffffff"          # button background
  buttonText: "555459"      # button text
  light: "efefef"           # lighter than button (3D border)
  mid: "dcdcdc"             # between button and dark (3D border)
  dark: "334258"            # darker than button (hover bg, 3D border)
  highlight: "f49300"       # selection / focus highlight
  highlightedText: "ffffff" # text on highlight
  link: "b56e00"            # hyperlinks
  toolTipBase: "ffffff"     # tooltip background
  toolTipText: "334258"     # tooltip text
  red: "a82353"             # error / destructive accents
  green: "008000"           # success accents
  orange: "f49300"          # warning accents
  error: "dd3d3d"           # error messages
```

Imported themes also include an optional `source_base16:` section with the original Base16 palette for reference. This section is ignored by the build.

See [docs/architecture/themes.md](architecture/themes.md) for the technical pipeline details.


## ⚙️ How it works

At build time, CMake runs `bin/theme/generate.py` which reads all `resources/themes/*.yml` files and generates `src/ui/ThemeDefinitions.h` — a C++ header containing a registry of all theme palettes with inline lookup functions. This header is gitignored as a build artifact.

The `just generate-themes` recipe can also be used to regenerate the header manually.
