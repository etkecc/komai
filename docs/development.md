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
just run
```

## 🔁 Typical Workflow

```sh
# Build incrementally
just build

# Run the app
just run
```

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

The hook set includes formatting (`clang-format`), syntax checks (`yaml/json`), theme checks, translation normalization checks, and QML linting.

Note: QML linting expects Qt6 `qmllint`. If unavailable, it is skipped with a message.

## 📁 Useful Paths

- `src/` - C++ application code
- `resources/qml/` - QML UI
- `resources/themes/` - built-in themes
- `docs/architecture/` - implementation details
- `.pre-commit-config.yaml` - configured `prek` hooks
- `justfile` - development/build commands

## 📚 Related Docs

- [Architecture](architecture/)
- [Themes](themes.md)
- [Translations](translations.md)
- [Packaging](packaging/README.md)
