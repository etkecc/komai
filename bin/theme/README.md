# Theme Pipeline 🎨

Scripts here maintain Komai's built-in theme system from YAML source files; see [`docs/user-guide/themes.md`](../../docs/user-guide/themes.md) for author workflow and [`docs/architecture/themes.md`](../../docs/architecture/themes.md) for internals.

Quick orientation:

- Theme authors work in `resources/themes/*.yml`.
- These scripts validate and transform that source into runtime C++ data.

## Why It Exists

Themes are authored as readable YAML under `resources/themes/`, but runtime code needs a C++ registry. This directory provides validation and code generation so themes stay consistent and easy to maintain.

## Files

- `check.py` - validates theme YAML schema and color fields.
- `contrast.py` - reports practical contrast ratios for palette roles and bubble/user colors.
- `generate.py` - generates `src/ui/ThemeDefinitions.h` from theme YAML files.
- `colors.py` - shared parsing and color-utility module used by the scripts above.

## Workflow

- `just theme-tinted-import <slug>` (via [`just`](https://github.com/casey/just)) to pull in a new external theme (wraps the C++ CLI).
- `just theme-check-contrast [slug ...]` to audit theme contrast ratios.
- `just theme-check-contrast-strict [slug ...]` to fail on hard AA-style misses.
- `just generate-themes` to regenerate `ThemeDefinitions.h`.
- `bin/theme/check.py` is also used by the `check-theme-yaml` hook in linting.

Theme importing is handled by the C++ CLI (`komai theme tinted-import`). The color math lives in `src/cli/ThemeColorUtils.cpp`. See [`resources/themes/README.md`](../../resources/themes/README.md) for the full developer workflow.

## Design Intent

- Keep authored theme files human-readable.
- Keep generated C++ deterministic.
- Fail fast when palette keys or values are invalid.

## Dependencies

- [`python3`](https://www.python.org/)
- Standard library only for `check.py`, `contrast.py`, `generate.py`, `colors.py`

Tool references:

- [Tinted Gallery (Base16 preview)](https://tinted-theming.github.io/tinted-gallery/)
- [Tinted schemes (Base16 identifiers)](https://github.com/tinted-theming/schemes/tree/spec-0.11/base16)

Related docs:

- [`docs/user-guide/themes.md`](../../docs/user-guide/themes.md)
- [`docs/architecture/themes.md`](../../docs/architecture/themes.md)
