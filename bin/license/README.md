# License Tooling ⚖️

This directory contains [`REUSE`](https://reuse.software/)-oriented helpers for source license metadata; operational context is in [`docs/development.md`](../../docs/development.md).

Quick orientation:

- Use `just license-check` in normal workflows.
- Use `just license-inject` when you intentionally want to add missing SPDX headers.

## Why It Exists

We want a practical split between:

- checking compliance (`license-check`), and
- applying missing SPDX headers (`license-inject`).

This keeps everyday linting predictable and keeps intentional source churn explicit.

## Files

- `check.sh` - runs REUSE checks for `src/`, `resources/qml/`, and icon licensing metadata under `resources/icons/`.
- `inject.sh` - adds SPDX headers to files that are missing them.
- `reuse-annotate.sh` - compatibility wrapper that forwards to `inject.sh`.

## Command Mapping

- `just license-check` -> `bin/license/check.sh`
- `just license-inject` -> `bin/license/inject.sh`
- `just license` -> alias to `just license-check`

The `license-check` hook is also used in `.pre-commit-config.yaml` and therefore in `just lint`.

## Behavior Notes

- `check.sh` skips cleanly if `reuse` is unavailable in the current environment.
- `check.sh` requires `LICENSES/GPL-3.0-or-later.txt` to exist.
- `check.sh` requires `LICENSES/MIT.txt` for third-party icon licensing.
- `inject.sh` uses `--skip-existing`, so it only injects missing headers (no broad reorder churn).
- `inject.sh` exits non-zero if it changed files (via `git diff --exit-code`), so changes are visible and reviewable.

## Dependencies

- [`REUSE`](https://reuse.software/) CLI (`reuse-tool`):
  - tool repo: <https://github.com/fsfe/reuse-tool>

Related docs:

- [`docs/development.md`](../../docs/development.md)
