# Translation Tooling 🌐

This directory contains tooling for translation normalization and AI-assisted updates; see [`docs/maintainers/translations.md`](../../docs/maintainers/translations.md) and [`docs/architecture/translations.md`](../../docs/architecture/translations.md). The current LLM backend is the [`Claude CLI`](https://docs.anthropic.com/en/docs/claude-cli) — swap `call_claude()` in `translate.py` to use a different provider.

## Why It Exists

Qt `.ts` files are XML and easily drift in formatting. We also support incremental translation filling via an LLM. These scripts keep translation maintenance reproducible and reviewable.

## Files

- `check-normalized.py` - verifies staged (or all) `.ts` files are in canonical normalized XML form.
- `translate.py` - command tool with two subcommands:
  - `normalize`: canonicalize XML formatting for `.ts` files
  - `translate`: translate unfinished strings in batches via an LLM

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
- An LLM CLI configured in `translate.py` (currently the `claude` CLI)
- Qt `lupdate` tool (via the `just translations-update` workflow)

Related docs:

- [`docs/maintainers/translations.md`](../../docs/maintainers/translations.md)
- [`docs/architecture/translations.md`](../../docs/architecture/translations.md)
