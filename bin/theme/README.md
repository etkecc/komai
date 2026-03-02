# Theme Pipeline 🎨

Scripts here maintain Komai's built-in theme system from YAML source files; see [`docs/user-guide/themes.md`](../../docs/user-guide/themes.md) for author workflow and [`docs/architecture/themes.md`](../../docs/architecture/themes.md) for internals.

Quick orientation:

- Theme authors work in `resources/themes/*.yml`.
- These scripts validate and transform that source into runtime C++ data.

## Why It Exists

Themes are authored as readable YAML under `resources/themes/`, but runtime code needs a C++ registry. This directory provides validation and code generation so themes stay consistent and easy to maintain.

## Files

- `check.py` - validates theme YAML schema and color fields.
- `generate.py` - generates `src/ui/ThemeDefinitions.h` from theme YAML files.
- `colors.py` - shared parsing and color-utility module used by the scripts above.

## Workflow

- `just theme-tinted-import <slug>` (via [`just`](https://github.com/casey/just)) to pull in a new external theme (wraps the C++ CLI).
- `just generate-themes` to regenerate `ThemeDefinitions.h`.
- `bin/theme/check.py` is also used by the `check-theme-yaml` hook in linting.

Theme importing is handled by the C++ CLI (`komai theme tinted-import`). The color math lives in `src/cli/ThemeColorUtils.cpp`. See [`resources/themes/README.md`](../../resources/themes/README.md) for the full developer workflow.

## Design Intent

- Keep authored theme files human-readable.
- Keep generated C++ deterministic.
- Fail fast when palette keys or values are invalid.

## Dependencies

- [`python3`](https://www.python.org/)
- Standard library only for `check.py`, `generate.py`, `colors.py`

Tool references:

- [Tinted Theming / Base16](https://github.com/tinted-theming/home)

Related docs:

- [`docs/user-guide/themes.md`](../../docs/user-guide/themes.md)
- [`docs/architecture/themes.md`](../../docs/architecture/themes.md)
