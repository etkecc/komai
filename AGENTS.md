# Agent Instructions for Komai

## What is Komai?

Komai (細い, "fine/slender") is a usability-focused fork of [nheko](https://nheko.im/nheko-reborn/nheko), a Qt/C++ desktop Matrix client, by [etke.cc](https://etke.cc/).

This is a **real git fork** -- the full source code is committed to this repository and we develop directly on it.


## Project Structure

```
komai/
  src/                    # C++ source files
  resources/
    qml/                  # QML UI files
    themes/               # Theme definition YAML files
  docs/                   # User-facing documentation
    architecture/         # Developer/technical documentation
    packaging/            # Build and packaging guides
  etc/packaging/          # Packaging configs (flatpak, appimage, etc.)
  CMakeLists.txt          # CMake build configuration
  justfile                # Build commands (use `just` command runner)
```


## Build System

Use `just` as the build entry point:

- `just build` -- CMake configure + build (output: `build/komai`)
- `just run` -- Run the compiled binary
- `just clean` -- Remove build directory
- `just appimage-build-docker` -- Build AppImage in Docker
- `just flatpak-build` -- Build Flatpak package

When making changes to C++/QML files, always run `just build` to verify the build succeeds and to prepare it for the human operator to test.

See [docs/packaging/native.md](docs/packaging/native.md) for build dependencies.


## Key Source Locations

### Settings and Configuration
- `src/UserSettingsPage.cpp` / `.h` -- All user settings with Qt properties
- Settings stored as YAML in `~/.config/komai/profiles/<name>.yml`
- See [docs/architecture/configuration.md](docs/architecture/configuration.md) for details

### Room List (Sidebar)
- `resources/qml/RoomList.qml` -- Room list UI
- `src/timeline/RoomlistModel.cpp` / `.h` -- Room list data model

### Message Timeline
- `resources/qml/TimelineBubbleMessageStyle.qml` -- Bubble message style (default)
- `resources/qml/TimelineDefaultMessageStyle.qml` -- Classic message style
- `src/timeline/TimelineModel.cpp` / `.h` -- Timeline data model

### Themes
- `resources/themes/*.yml` -- Theme definitions (colors, styles)
- `src/ui/Theme.cpp` / `.h` -- Theme loading and application
- See [docs/themes.md](docs/themes.md) for theme format

### QML Components
- `resources/qml/components/` -- Reusable UI components
- `resources/qml/dialogs/` -- Dialog windows
- `resources/qml/delegates/` -- Message type delegates


## Theme Colors (for styling)

Access via `palette.*` in QML:
- `palette.window` -- Normal background
- `palette.dark` -- Hovered background
- `palette.highlight` -- Accent color
- `palette.text` -- Primary text
- `palette.buttonText` -- Secondary/dimmed text

Access via `Nheko.theme.*`:
- `Nheko.theme.red` -- Error/loud notification color
- `Nheko.paddingSmall` (4px), `Nheko.paddingMedium` (8px)


## Design Philosophy

- **Desktop-first UX** -- Optimized for large screens with always-visible room list
- **Readable by everyone** -- All text must be comfortably readable at default settings
- **Subtle but effective** -- Visual changes noticeable without being jarring


## Documentation

- [docs/README.md](docs/README.md) -- Documentation index
- [docs/architecture/](docs/architecture/) -- Technical/developer documentation
- [docs/differences-from-nheko.md](docs/differences-from-nheko.md) -- What makes Komai different


## Environment

- OS: Linux (primary), macOS, Windows
- Qt version: 6.x
- Compiler: GCC/Clang with C++20 support
