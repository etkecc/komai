# Theme System Architecture

This document describes the technical pipeline for Komai's theme system.
For theme authoring rules and contrast targets, see [Theme Design Guide](theme-design-guide.md).
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
Python theme scripts may reuse similar parsing and contrast helpers for audit
and generation tasks, but they do not define the import behavior.


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
- **`userColors`** — bubble/user color slots (timeline, member lists, profiles, etc.):
  - `self` — mapping with required `background` and optional `text`,
    `secondaryText`, `link`
  - `others` — list of mappings in the same shape (minimum 1)

When the "Adaptive" user color policy is active, `roomUserColor()` uses
the `others` slots directly: rooms with more members than `others.size()` get a
uniform first slot; smaller rooms assign a distinct slot per member from the
list. At runtime, `TimelineViewManager::userBubblePalette()` and
`TimelineViewManager::roomUserBubblePalette()` resolve the effective bubble
palette for each slot, falling back to the global theme palette when optional
slot foregrounds are absent.

Imported themes include an optional `source_base16:` section preserving the
original Base16 palette. This section is purely informational and is ignored
by both `generate.py` and `check.py`.

Auto-generated themes also get `userColors` written as final literal bubble
slot values. `komai theme tinted-import` and `komai theme create-sample`
soften bubble `background` values against `palette.base` before writing YAML,
so runtime QML can use them directly without extra tinting logic. Those
commands may also write explicit bubble `text`, `secondaryText`, and `link`
overrides when they are needed for readability on the generated bubble fills.


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

1. **highlightedText on highlight** — if contrast < 4.5, pick the closest readable
   candidate from mapped text/surface roles plus black/white. If still insufficient,
   adjust the highlight background.
2. **brightText on dark** — if contrast < 4.5, pick a closer readable candidate.
3. **buttonText on neutral surfaces** — if `window`, `base`, or `alternateBase`
   would drop below 4.5, push `buttonText` toward black (light themes) or white
   (dark themes) until it clears the floor.
4. **link on neutral surfaces** — same treatment as `buttonText`, so imported links
   stay readable on Komai's ordinary backgrounds.
5. **dark (hover bg)** — must be distinguishable from both `window` and `button`
   (contrast >= 1.5). Derived by blending `button` toward black (light themes)
   or white (dark themes). If `buttonText` is hard to read on the result, push
   further but cap at 3.0 contrast from `window` to avoid overly dramatic hovers.

These adjustments use WCAG 2.0 contrast ratios with perceptual (linear-light)
blending.


## Auto-generated userColors

Imported themes and sample themes generate `userColors` in `ThemeColorUtils.cpp`.

- `self.background` starts from `highlight` and is blended over `base`
- each `others[*].background` starts from a vivid generated accent and is
  blended over `base`
- optional `text`, `secondaryText`, and `link` fields are then populated from
  the global palette with per-bubble contrast fixes when needed

The important contract is that the stored YAML `background` value is already
the final literal bubble-fill color. Runtime code uses it directly. If a theme
needs bubble-specific foregrounds, those are authored or imported explicitly in
the slot.

After `userColors` are generated, the import/create-sample commands also re-check
`palette.link` against the neutral surfaces and generated bubble fills, so message
links remain readable without hand-tuning every imported theme.


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


## Preview gallery

For fast visual review of built-in themes, Komai also has a static preview SPA.

```
etc/tools/theme-preview/*     ← tracked HTML/CSS/JS source for the SPA
        ▲
        │
        ├────────── loads built-in themes directly from ──────────┐
        │                                                         │
        ▼                                                         ▼
browser runtime                                           resources/themes/*.yml
        ▲
        │
just theme-preview-run      ← serves the SPA root and mounts resources/themes/
```

The preview renders one frame per built-in theme, sorted light-first and then
alphabetically by slug within each variant group. Each frame includes:

- communities sidebar preview
- room list preview with idle, hover, selected, unread, and draft states
- timeline preview with all `userColors.others`, `userColors.self`, bubble text,
  bubble secondary text, and link text
- composer/footer chrome
- client-side upload/drop support for extra theme YAML files without editing the repo

Useful commands:

```sh
just theme-preview-run
```


## Bubble Slot Schema Example

```yaml
userColors:
  self:
    background: "#0b3518"
    text: "#9adca8"
    secondaryText: "#6fa17c"
    link: "#68ffc8"
  others:
    - background: "#111512"
      text: "#9adca8"
      secondaryText: "#6fa17c"
      link: "#68ffc8"
    - background: "#0d2a22"
      link: "#7ee7ff"
```

Fallbacks are intentionally simple:

- missing slot `text` -> `palette.text`
- missing slot `secondaryText` -> `palette.buttonText`
- missing slot `link` -> `palette.link`

`secondaryText` is intended for in-bubble secondary/inactive content. Outer
timeline metadata should normally use the regular timeline palette instead of
inheriting bubble-specific overrides.

`theme-preview-run` serves `etc/tools/theme-preview/` using a containerized static
web server and mounts `resources/themes/` into the served tree, so there is no
generated `themes.json` step anymore.


## Re-importing with updated heuristics

If the mapping, contrast logic, or auto-generated `userColors` logic in
`ThemeColorUtils.cpp` changes:

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
just theme-check-contrast
just theme-check-contrast-strict light-komai
just generate-themes
```

Hand-crafted themes (without `source_base16:`) must be updated manually.


## C++ import pipeline (CLI)

The C++ CLI commands (`komai theme tinted-import`, etc.) are the canonical
theme import path. They own Base16 mapping, contrast heuristics, and
auto-generated `userColors` for imported/sample themes.

```
komai theme tinted-import <slug>
        │
        ▼
src/cli/ThemeCommands.cpp           ← HTTP fetch from tinted-theming
        │
        ▼
src/cli/ThemeColorUtils.cpp         ← Base16→QPalette mapping + contrast heuristics
        │                              + softened literal userColors generation
        ▼
~/.local/share/komai/themes/*.yml   ← user themes directory
```

The Python scripts under `bin/theme/` are build/audit tools around that YAML:
header generation, validation, contrast reporting, and preview support. They
share some color/contrast helpers, but they are not the source of truth for
theme import behavior.

See [CLI Architecture](cli.md) for the subcommand dispatch design.


## Scripts

| Script | Purpose |
|--------|---------|
| `bin/theme/colors.py` | Shared module for Python-side YAML parsing and color/contrast helpers |
| `bin/theme/contrast.py` | Report practical contrast ratios for palette roles and bubble/user colors |
| `bin/theme/generate.py` | Read resolved YAMLs, generate C++ header |
| `bin/theme/check.py` | Validate theme YAML files (palette colors, userColors, hex format) |
