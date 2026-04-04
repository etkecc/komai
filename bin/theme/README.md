# Theme Pipeline

Scripts here maintain Komai's built-in theme system from YAML source files; see [`docs/user-guide/themes.md`](../../docs/user-guide/themes.md) for author workflow and [`docs/architecture/themes.md`](../../docs/architecture/themes.md) for internals.

Quick orientation:

- Theme authors work in `resources/themes/*.yml`.
- These scripts validate and audit that source data.
- Built-in themes are embedded into the Rust binary via `include_str!` and parsed at startup.

## Why It Exists

Themes are authored as readable YAML under `resources/themes/`. This directory provides validation and contrast auditing so themes stay consistent and easy to maintain.

## Files

- `check.py` - validates theme YAML schema and color fields.
- `contrast.py` - reports practical contrast ratios for palette roles and bubble/user colors.
- `colors.py` - shared parsing and color-utility module used by the Python build/audit scripts above.

## Workflow

- `just theme-tinted-import <slug>` (via [`just`](https://github.com/casey/just)) to pull in a new external theme (wraps the C++ CLI).
- `just theme-check-contrast [slug ...]` to audit theme contrast ratios.
- `just theme-check-contrast-strict [slug ...]` to fail on hard AA-style misses.
- `just theme-preview-run` to serve the static preview SPA in `etc/tools/theme-preview/` with `resources/themes/` mounted live.
- `bin/theme/check.py` is also used by the `check-theme-yaml` hook in linting.

Theme importing is handled by the C++ CLI (`komai theme tinted-import`). The import-time color math and auto-generated `userColors` logic live in `src/cli/ThemeColorUtils.cpp`. That includes generating explicit bubble slots with required `background` and optional `text`, `secondaryText`, and `link` when needed for readability. The Python scripts here are for validation, contrast auditing, and preview support. See [`resources/themes/README.md`](../../resources/themes/README.md) for the full developer workflow.

## Design Intent

- Keep authored theme files human-readable.
- Fail fast when palette keys or values are invalid.

## Dependencies

- [`python3`](https://www.python.org/)
- Standard library only for `check.py`, `contrast.py`, `colors.py`

Tool references:

- [Tinted Gallery (Base16 preview)](https://tinted-theming.github.io/tinted-gallery/)
- [Tinted schemes (Base16 identifiers)](https://github.com/tinted-theming/schemes/tree/spec-0.11/base16)

Related docs:

- [`docs/user-guide/themes.md`](../../docs/user-guide/themes.md)
- [`docs/architecture/themes.md`](../../docs/architecture/themes.md)
