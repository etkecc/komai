# Translation Tooling 🌐

This directory contains tooling for translation normalization and AI-assisted updates; see [`docs/translations.md`](../../docs/translations.md), [`docs/architecture/translations.md`](../../docs/architecture/translations.md), and the [`Claude CLI` docs](https://docs.anthropic.com/en/docs/claude-cli).

## Why It Exists

Qt `.ts` files are XML and easily drift in formatting. We also support incremental translation filling via Claude CLI. These scripts keep translation maintenance reproducible and reviewable.

## Files

- `check-normalized.py` - verifies staged (or all) `.ts` files are in canonical normalized XML form.
- `translate.py` - command tool with two subcommands:
  - `normalize`: canonicalize XML formatting for `.ts` files
  - `translate`: translate unfinished strings in batches via Claude CLI

## Command Mapping

- `just translations-update` -> runs `lupdate`, then normalization.
- `just translations-normalize` -> runs `translate.py normalize`.
- `just translations-claude-translate-lang <lang>` -> runs `translate.py translate <lang>`.
- `just translations-claude-translate-all` -> iterates all languages (except `en`).

`check-normalized.py` is used by the `check-ts-normalized` hook in `.pre-commit-config.yaml`.

## Workflow Notes

- Translation is incremental: each successful batch is written back immediately.
- Re-running translation processes only remaining unfinished strings.
- Plural (`numerus`) entries are intentionally skipped for now.

## Dependencies

- `python3`
- `claude` CLI (only for the `translate` subcommand)
- Qt `lupdate` tool (via the `just translations-update` workflow)

Related docs:

- [`docs/translations.md`](../../docs/translations.md)
- [`docs/architecture/translations.md`](../../docs/architecture/translations.md)
