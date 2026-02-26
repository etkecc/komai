# 🛠️ Development

This guide covers day-to-day development on Komai.

For build dependencies and distro package names, see [Packaging: Native build](packaging/native.md).

## ✅ Prerequisites

- Linux/macOS/Windows development environment
- [`just`](https://github.com/casey/just) (command runner used by this project)
- Build dependencies from [packaging/native.md](packaging/native.md)
- (Recommended) [`mise`](https://mise.jdx.dev/) for tool management used by `prek`

## 🚀 Quick Start

```sh
just build
just test
just test-integration
just run
just lint
```

## 🔁 Typical Workflow

```sh
# Build incrementally
just build

# Run unit tests
just test-unit

# Run integration tests
just test-integration

# Run all tests
just test

# Run the app
just run
```

If you need a clean rebuild:

```sh
just clean
just build
```

For a local build that enables both LMDB and RocksDB backends at once:

```sh
just configure-all-backends
just build-all-backends
just test-all-backends
```

To explicitly select RocksDB at runtime (when compiled in), set:

```sh
KOMAI_DB_BACKEND=rocksdb just run-all-backends
```

## 🧹 Pre-commit Checks (prek)

Komai uses [prek](https://prek.j178.dev/) (pre-commit compatible) for fast local checks.

Install git hook once and you can forget about it:

```sh
just prek-install-git-pre-commit-hook
```

Run checks manually:

```sh
# What runs during commit (staged files)
just prek-run-on-staged

# Full repository check
just prek-run-on-all
```

The hook set includes formatting (`clang-format`), syntax checks (`yaml/json`), Markdown link checks, theme checks, translation normalization checks, QML linting, and C++ unit tests (run on C++/header/QML changes).

Note: QML linting expects Qt6 `qmllint`. If unavailable, it is skipped with a message.

Current C++ test executables:

- `komai_yaml_settings_test`
- `komai_db_backend_test`

Notes:

- `komai_db_backend_test` includes in-memory backend contract checks and, when LMDB or RocksDB backend support is enabled, adapter checks for those backends using temporary filesystem directories (integration-style coverage).

## 📁 Useful Paths

- `src/` - C++ application code
- `resources/qml/` - QML UI
- `resources/themes/` - built-in themes
- `docs/architecture/` - implementation details
- `.pre-commit-config.yaml` - configured `prek` hooks
- `justfile` - development/build commands

## 🧩 Maintenance Helpers

- `just emoji-generate` - regenerate `src/emoji/Provider.{h,cpp}` from emoji data files
- `just icons-audit` - check icon reference/qrc/files consistency
- `just icons-generate-list` - regenerate `docs/architecture/icons-list.md` icon catalog
- `just icons-generate-derived` - regenerate derived local icons from Fluent sources (for example `ui/double-checkmark.svg`)
- `just icons-fetch <REL_PATH> <ALIAS_SVG_NAME>` - fetch one Fluent icon into `resources/icons/fluent/` and wire qrc alias (`ui/` by default)
- `just icons-sync [--dry-run]` - sync mirrored Fluent icons from pinned upstream ref
- `just settings-3-layer-mapping-generate` - regenerate `docs/architecture/settings/3-layer-mapping.md` (`SettingId` ↔ runtime getter ↔ persisted key audit)
- `just settings-3-layer-mapping-check` - check drift for that report without rewriting it
- `just docs-check-links` - verify Markdown links point to existing local targets
- `just license-check` - run REUSE compliance lint (skips when `reuse` is unavailable)
- `just license-inject` - add SPDX headers to source files that currently lack them

## 📚 Related Docs

- [Architecture](../architecture/)
- [Themes](../user-guide/themes.md)
- [Translations](translations.md)
- [Packaging](packaging/README.md)
