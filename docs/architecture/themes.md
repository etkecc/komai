# Theme System Architecture

This document describes the technical pipeline for Komai's theme system.
For user-facing documentation, see [docs/user-guide/themes.md](../user-guide/themes.md).


## Pipeline overview

```
tinted-theming/schemes (Base16 YAML)
        │
        ▼
komai theme tinted-import    ← applies Base16→QPalette mapping + contrast heuristics + userColors generation
        │
        ▼
resources/themes/*.yml      ← resolved palette colors (16 Qt + 4 app) + userColors
        │
        ▼
bin/theme/generate.py        ← reads colors as-is, generates C++ header
        │
        ▼
src/ui/ThemeDefinitions.h    ← compiled into the binary
```

All color derivation happens at **import time** (in the C++ CLI).
The build step (`generate.py`) is a straightforward YAML→C++ transcription
with no color logic.


## Runtime theme loading

In addition to the build-time pipeline, Komai loads user-defined themes at
runtime from XDG data directories via `ThemeRegistry` (`src/ui/ThemeRegistry.cpp`).

```
                                        XDG data dirs
                                        ~/.local/share/komai/themes/*.yml
                                        /usr/local/share/komai/themes/*.yml
                                        /usr/share/komai/themes/*.yml
                                                │
                                                ▼
                                        ThemeRegistry::loadExternalThemes()
                                        ← parses YAML, validates palette + userColors
                                                │
tinted-theming/schemes (Base16 YAML)            │
        │                                       │
        ▼                                       │
komai theme tinted-import                       │
        │                                       │
        ▼                                       │
resources/themes/*.yml                         │
        │                                       │
        ▼                                       │
bin/theme/generate.py                           │
        │                                       │
        ▼                                       ▼
src/ui/ThemeDefinitions.h ──► ThemeRegistry (merged list at startup)
```

`ThemeRegistry` is a singleton initialized once from `main()` before
`UserSettings`. It copies all built-in themes from `themeDefinitions()`,
then scans external directories using `QStandardPaths::standardLocations(GenericDataLocation)`.
Built-in themes always take priority on slug collision. External themes
get `sortOrder = 300` so they appear after built-in themes in the UI.

All theme lookup functions (`findTheme`, `themeSlugs`, `themeNames`,
`themeVariant`, `defaultThemeSlug`) are methods on `ThemeRegistry::instance()`
rather than free functions.


## YAML format

Theme files contain 20 color keys under `palette:`, plus a required
`userColors:` section:

- **16 Qt palette roles:** `window`, `windowText`, `base`, `alternateBase`, `text`,
  `brightText`, `button`, `buttonText`, `light`, `mid`, `dark`, `highlight`,
  `highlightedText`, `link`, `toolTipBase`, `toolTipText`
- **4 app-level semantic colors:** `attention`, `success`, `warning`, `error`
- **`userColors`** — user colors (timeline, member lists, profiles, etc.):
  - `self` — `#`-prefixed hex color for the current user and their messages
  - `others` — list of `#`-prefixed hex colors for other users (minimum 1)

When the "Adaptive" user color policy is active, `roomUserColor()` uses
`others` directly: rooms with more members than `others.size()` get a uniform
color; smaller rooms assign a distinct color per member from the list.

Imported themes include an optional `source_base16:` section preserving the
original Base16 palette. This section is purely informational and is ignored
by both `generate.py` and `check.py`.


## Base16 → QPalette mapping

The mapping is implemented in `src/cli/ThemeColorUtils.cpp` and applied
at import time by `komai theme tinted-import` — never at build time.

| QPalette role    | Base16 source                |
|------------------|------------------------------|
| window           | base00                       |
| windowText       | base05                       |
| base             | base01                       |
| alternateBase    | base02                       |
| text             | base05                       |
| brightText       | base07                       |
| button           | base01                       |
| buttonText       | base04                       |
| light            | base06                       |
| mid              | base03                       |
| dark             | base01                       |
| highlight        | base0D                       |
| highlightedText  | base07 (dark) / base00 (light) |
| link             | base0D                       |
| toolTipBase      | base01                       |
| toolTipText      | base05                       |

Semantic accent colors: `attention` ← base08, `success` ← base0B, `warning` ← base09,
`error` ← base08.


## Contrast heuristics

After the initial mapping, the contrast heuristics in `ThemeColorUtils.cpp` adjust colors for readability:

1. **highlightedText on highlight** — if contrast < 3.0, pick the best candidate
   from light/dark palette slots. If still insufficient, adjust the highlight
   background.
2. **brightText on dark** — if contrast < 3.0, pick a better candidate.
3. **dark (hover bg)** — must be distinguishable from both `window` and `button`
   (contrast >= 1.5). Derived by blending `button` toward black (light themes)
   or white (dark themes). If `buttonText` is hard to read on the result, push
   further but cap at 3.0 contrast from `window` to avoid overly dramatic hovers.

These adjustments use WCAG 2.0 contrast ratios with perceptual (linear-light)
blending.


## Adding a new built-in theme

```sh
# Search for a theme (browse visually at https://tinted-theming.github.io/tinted-gallery/)
komai theme tinted-search catppuccin

# Import and relocate into the source tree
just theme-tinted-import <slug>

# Or create a starter theme for manual customisation
just theme-create-sample dark my-theme

# Rebuild — the new theme is now compiled in
just build
```

See [`resources/themes/README.md`](../../resources/themes/README.md)
for a quick reference.


## Re-importing with updated heuristics

If the mapping or contrast logic in `ThemeColorUtils.cpp` changes:

```sh
# Re-import all community themes (those with source_base16 sections)
for f in resources/themes/*.yml; do
    if grep -q source_base16 "$f"; then
        slug=$(basename "$f" .yml)
        just theme-tinted-import "$slug"
    fi
done

# Verify output
python3 bin/theme/check.py
just generate-themes
```

Hand-crafted themes (without `source_base16:`) must be updated manually.


## C++ import pipeline (CLI)

The C++ CLI commands (`komai theme tinted-import`, etc.) port the Python
color math to C++ so end users can import themes without Python or the source tree.

```
komai theme tinted-import <slug>
        │
        ▼
src/cli/ThemeCommands.cpp           ← HTTP fetch from tinted-theming
        │
        ▼
src/cli/ThemeColorUtils.cpp         ← Base16→QPalette mapping + contrast heuristics
        │                              + userColors generation (direct port of bin/theme/colors.py)
        ▼
~/.local/share/komai/themes/*.yml   ← user themes directory
```

The C++ color math in `ThemeColorUtils.cpp` is a function-for-function port of
`colors.py` and produces identical output for the same input. Both pipelines
use the same YAML format, so themes created by either path are interchangeable.

See [CLI Architecture](cli.md) for the subcommand dispatch design.


## Scripts

| Script | Purpose |
|--------|---------|
| `bin/theme/colors.py` | Shared module: YAML parser, color utilities |
| `bin/theme/generate.py` | Read resolved YAMLs, generate C++ header |
| `bin/theme/check.py` | Validate theme YAML files (palette colors, userColors, hex format) |
