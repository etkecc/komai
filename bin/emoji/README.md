# Emoji Provider Generation 🙂

This directory builds Komai's compiled emoji provider source files from upstream Unicode data plus Komai-specific overrides; start with [`docs/maintainers/development.md`](../../docs/maintainers/development.md) and Unicode emoji data at [`unicode.org/Public/emoji`](https://unicode.org/Public/emoji/).

Quick orientation:

- Use `just emoji-generate` for normal development.
- Edit source data files, not generated C++ output.

## Why It Exists

The app needs a fast, static emoji catalog in C++ for QML/UI usage. Instead of hand-editing large generated files, we regenerate them from canonical data inputs.

## Files

- `generate.sh` - orchestrates the generation pipeline.
- `codegen.py` - parses emoji data and shortcodes, then renders C++ header/implementation output.

## Inputs and Outputs

Inputs:

- `resources/emoji-test.txt` (Unicode emoji data)
- `resources/extra_emoji.txt` (extra entries not in upstream set)
- `resources/shortcodes.txt` (shortcode overrides)
- `resources/provider-head.txt` (static C++ prelude)

Generated outputs:

- `src/emoji/Provider.cpp`
- `src/emoji/Provider.h`
- `resources/complete-emoji.txt` (intermediate merged input)

## Typical Workflow

- Run `just emoji-generate` after updating emoji inputs.
- Review generated diffs in `src/emoji/Provider.{h,cpp}`.
- Commit both input and generated output changes together.

## Dependencies

- [`python3`](https://www.python.org/)
- Python packages used by `codegen.py`:
  - [`jinja2`](https://palletsprojects.com/p/jinja/)
  - [`unidecode`](https://pypi.org/project/Unidecode/)
- Unicode test data format source: [`emoji-test.txt`](https://unicode.org/Public/emoji/)

Related docs:

- [`docs/maintainers/development.md`](../../docs/maintainers/development.md)
