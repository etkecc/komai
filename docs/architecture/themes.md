# Theme System Architecture

This document describes the technical pipeline for Komai's theme system.
For user-facing documentation, see [docs/themes.md](../themes.md).


## Pipeline overview

```
tinted-theming/schemes (Base16 YAML)
        │
        ▼
bin/theme/import.py          ← applies Base16→QPalette mapping + contrast heuristics
        │
        ▼
resources/themes/*.yaml      ← resolved QPalette-level colors (20 keys)
        │
        ▼
bin/theme/generate.py        ← reads colors as-is, generates C++ header
        │
        ▼
src/ui/ThemeDefinitions.h    ← compiled into the binary
```

All color derivation happens at **import time** (in `import.py`).
The build step (`generate.py`) is a straightforward YAML→C++ transcription
with no color logic.


## YAML format

Theme files contain 20 palette keys directly under `palette:`:

- **16 QPalette roles:** `window`, `windowText`, `base`, `alternateBase`, `text`,
  `brightText`, `button`, `buttonText`, `light`, `mid`, `dark`, `highlight`,
  `highlightedText`, `link`, `toolTipBase`, `toolTipText`
- **4 semantic accent colors:** `red`, `green`, `orange`, `error`

Imported themes include an optional `source_base16:` section preserving the
original Base16 palette. This section is purely informational and is ignored
by both `generate.py` and `check.py`.


## Base16 → QPalette mapping

The mapping is implemented in `bin/theme/colors.py:base16_to_palette()`.
This function is only called by `import.py` — never at build time.

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

Semantic accent colors: `red` ← base08, `green` ← base0B, `orange` ← base09,
`error` ← base08.


## Contrast heuristics

After the initial mapping, `_ensure_contrast()` adjusts colors for readability:

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


## Re-importing with updated heuristics

If the mapping or contrast logic in `colors.py` changes:

```sh
# Re-import all community themes (those with source_base16 sections)
for f in resources/themes/*.yaml; do
    if grep -q source_base16 "$f"; then
        slug=$(basename "$f" .yaml)
        python3 bin/theme/import.py "$slug" --force
    fi
done

# Verify output
python3 bin/theme/check.py
just generate-themes
```

Hand-crafted themes (without `source_base16:`) must be updated manually.


## Scripts

| Script | Purpose |
|--------|---------|
| `bin/theme/colors.py` | Shared module: YAML parser, color utilities, Base16 mapping, contrast heuristics |
| `bin/theme/import.py` | Fetch Base16 theme, apply mapping, write resolved YAML |
| `bin/theme/generate.py` | Read resolved YAMLs, generate C++ header |
| `bin/theme/check.py` | Validate theme YAML files (20 palette keys, hex format) |
