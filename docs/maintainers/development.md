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

## ✅ Minimum Verification

- Run `just prek-run-on-all` before commit/PR for full hook coverage, or rely on the installed pre-commit hook for staged checks on each commit.
- If `just prek-run-on-all` has already passed for the current tree immediately before commit, `git commit --no-verify` is acceptable to avoid running the same hooks twice.
- Use `just lint` as a faster iteration check, including docs-only edits (it checks Markdown links and docs drift).
- For C++/header/QML changes, also run `just build` (and the relevant tests) before commit/PR.

If you need a clean rebuild:

```sh
just clean
just build
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

The hook set includes formatting (`clang-format`), syntax checks (`yaml/json`), Markdown link checks, theme checks, built-in theme [WCAG AA contrast](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html) enforcement, translation normalization checks, QML linting, and C++ unit tests (run on C++/header/QML changes).

Note: QML linting expects Qt6 `qmllint`. If unavailable, it is skipped with a message.

Representative C++ test executables:

- `komai_yaml_settings_test`
- `komai_settings_storage_test`
- `komai_startup_settings_test`

## 📁 Useful Paths

- `src/` - C++ application code
- `resources/qml/` - QML UI
- `resources/themes/` - built-in themes
- `docs/architecture/` - implementation details
- `.pre-commit-config.yaml` - configured `prek` hooks
- `justfile` - development/build commands

## 🧩 Maintenance Helpers

- `just emoji-fetch` - fetch/update pinned Unicode + CLDR emoji source cache into `var/emoji/`
- `just emoji-build` - generate runtime emoji data artifacts from cache/sources
- `just emoji-check` - validate emoji lock/overrides and cache-based build reproducibility
- `just emoji-add-token <EMOJI> <LOCALE> <TOKEN>` - add a locale token override (for example `just emoji-add-token "🥃" bg "уиски"`)
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
- [Performance Tracing](../architecture/performance.md)
- [Themes](../user-guide/themes.md)
- [Translations](translations.md)
- [Packaging](packaging/README.md)
