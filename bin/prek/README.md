# Prek Local Hook Wrappers ✅

This directory holds local wrapper scripts used by [`prek`](https://prek.j178.dev/) hooks; see [`docs/development.md`](../../docs/development.md) for the day-to-day lint/test flow.

Quick orientation:

- [`prek`](https://prek.j178.dev/) is the hook runner Komai uses (compatible with the [`pre-commit`](https://pre-commit.com/) model).
- Purpose: run fast automated checks before commit and in CI so style, policy, and test regressions are caught early, which is especially important when coding with AI agents.
- These wrappers add Komai-specific behavior on top of generic hooks (Qt6-aware QML linting, `QSettings` policy enforcement, and test execution).

## Why It Exists

Some checks need project-specific logic that is too complex for a plain pre-commit entry. Wrappers keep that logic versioned, testable, and shared between local runs and CI.

## Files

- `qmllint.sh` - finds a Qt6 `qmllint`, runs it, and treats warnings as failures.
- `no-qsettings.sh` - blocks direct `QSettings` usage in `src/` to enforce Komai's YAML-backed settings architecture.
- `tests.sh` - delegates to `just test` for the C++ test suite.

## How It Fits Together

- `.pre-commit-config.yaml` registers these as local hooks.
- `just lint` (via [`just`](https://github.com/casey/just)) runs selected hooks using `just prek-run-on-all ...`.
- `.github/workflows/prek.yml` runs the same hook set in CI.

## Dependencies

- `qmllint.sh`: Qt6 tooling (`qmake6` and/or `qmllint` linked against Qt6)
- `no-qsettings.sh`: `rg` preferred (falls back to `grep`)
- `tests.sh`: `just`, build dependencies for test targets

Tool references:

- [`prek`](https://prek.j178.dev/)
- [`pre-commit`](https://pre-commit.com/) (hook format compatibility)
- [`qmllint` (Qt docs)](https://doc.qt.io/qt-6/qtqml-tooling-qmllint.html)

Related docs:

- [`docs/development.md`](../../docs/development.md)
- [`docs/architecture/settings/README.md`](../../docs/architecture/settings/README.md)
