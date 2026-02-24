# Bin Scripts Toolkit 🧰

This directory contains small operational scripts that support Komai development workflows. Start with [`docs/development.md`](../docs/development.md), then use [`just`](https://github.com/casey/just) + [`prek`](https://prek.j178.dev/) as the main entrypoints.

Quick orientation (humans and agents):

- Start with [`just`](https://github.com/casey/just) tasks in [`justfile`](../justfile).
- Treat `bin/*` as the implementation layer behind those tasks and hooks.
- Keep behavior here deterministic so local runs and CI match.

The big picture:

- `justfile` is the user-facing entrypoint.
- `bin/*` scripts are the implementation details behind those `just` tasks and local [`prek`](https://prek.j178.dev/) hooks.
- CI reuses the same scripts so local and CI behavior stay aligned.

## What Lives Here

- [`emoji/`](emoji/README.md) - generate `src/emoji/Provider.{h,cpp}` from Unicode emoji data.
- [`license/`](license/README.md) - REUSE license checks and SPDX header injection helpers.
- [`prek/`](prek/README.md) - project-specific hook wrappers used by `.pre-commit-config.yaml`.
- [`theme/`](theme/README.md) - theme validation, import, and C++ header generation.
- [`translations/`](translations/README.md) - translation normalization and Claude-assisted translation tooling.

## Why This Exists

- Keeps the `justfile` readable.
- Keeps hook logic testable and versioned in one place.
- Avoids hidden CI-only behavior.

External tools referenced here:

- [`just`](https://github.com/casey/just)
- [`prek`](https://prek.j178.dev/)
- [`pre-commit` hooks format compatibility](https://pre-commit.com/)

Related docs:

- [`docs/development.md`](../docs/development.md)
- [`docs/themes.md`](../docs/themes.md)
- [`docs/translations.md`](../docs/translations.md)
