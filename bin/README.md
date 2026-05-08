# Bin Scripts Toolkit 🧰

This directory contains small operational scripts that support Komai development workflows. Start with [`docs/maintainers/development.md`](../docs/maintainers/development.md), then use [`just`](https://github.com/casey/just) + [`prek`](https://prek.j178.dev/) as the main entrypoints.

Quick orientation (humans and agents):

- Start with [`just`](https://github.com/casey/just) tasks in [`justfile`](../justfile).
- Treat `bin/*` as the implementation layer behind those tasks and hooks.
- Keep behavior here deterministic so local runs and CI match.

The big picture:

- `justfile` is the user-facing entrypoint.
- `bin/*` scripts are the implementation details behind those `just` tasks and local [`prek`](https://prek.j178.dev/) hooks.
- CI reuses the same scripts so local and CI behavior stay aligned.

## What Lives Here

- [`build/`](build/) - native build orchestrator (`native.sh`) backing `just configure` / `just build` / `just test-cpp*`.
- [`docs/`](docs/README.md) - Markdown quality checks for documentation links.
- [`emoji/`](emoji/README.md) - generate `src/emoji/Provider.{h,cpp}` from Unicode emoji data.
- [`flatpak/`](flatpak/) - vendor cargo dependencies for the offline Flatpak build (`cargo-sources.py`).
- [`icons/`](icons/README.md) - icon audit and pinned Fluent sync helpers.
- [`license/`](license/README.md) - REUSE license checks and SPDX header injection helpers.
- [`perf/`](perf/) - performance benchmarking helpers (room-switch latency reports, etc.).
- [`prek/`](prek/README.md) - project-specific hook wrappers used by `.pre-commit-config.yaml`.
- [`release/`](release/README.md) - release preparation, build, and publish helpers driven by `just release-*` recipes.
- [`serverlist/`](serverlist/) - validates `resources/serverlist/servers.yml`.
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

- [`docs/maintainers/development.md`](../docs/maintainers/development.md)
- [`docs/user-guide/features/themes.md`](../docs/user-guide/features/themes.md)
- [`docs/maintainers/translations.md`](../docs/maintainers/translations.md)
