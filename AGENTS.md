# Agent Instructions for Komai

## What is Komai?

Komai (こまい, "fine/slender") is a usability-focused fork of [nheko](https://nheko.im/nheko-reborn/nheko), a Qt/C++ desktop Matrix client, by [etke.cc](https://etke.cc/).

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

- `just build` -- CMake configure + build (output: `var/build/native/komai`)
- `just test` -- Run the supported test suite (`ctest` C++ unit/integration tests in `var/build/native` plus Rust unit tests)
- `just lint` -- Run selected `prek` hooks on all files (format/lint/policy/docs drift checks)
- `just run` -- Run the compiled binary
- `just clean` -- Remove build directory
- `just appimage-build-docker` -- Build AppImage in Docker
- `just flatpak-build` -- Build Flatpak package
- `just snap-build-docker` -- Build Snap package in Docker

When making changes to C++/QML files, always run `just build` to verify the build succeeds and to prepare it for the human operator to test.
Use `just lint` as a quick local verification step (including docs-only changes; it validates Markdown links and docs drift checks).
Before committing, run `just prek-run-on-all` for full hook coverage, or rely on the installed `prek` pre-commit hook for staged checks on each commit.
If `just prek-run-on-all` has already passed for the current tree immediately before commit, `git commit --no-verify` is allowed to skip duplicate hook execution.
For C++/header/QML changes, `prek` also runs unit tests via `bin/prek/tests.sh`.

See [docs/maintainers/packaging/native.md](docs/maintainers/packaging/native.md) for build dependencies.


## Releases

To cut a release, follow [docs/maintainers/releases.md](docs/maintainers/releases.md). The CHANGELOG entry shape (categories, voice, commit-linking) is documented separately in [docs/maintainers/changelog-style.md](docs/maintainers/changelog-style.md) -- read it before drafting the entry.

**Pause for approval before tagging.** After drafting the CHANGELOG entry, show it to the maintainer and wait for their sign-off before committing, tagging, or pushing anything. The entry becomes the public GitHub Release body, and the tag push triggers the publish pipeline -- neither is easy to walk back.

**Do NOT invoke the `release-manual-*` recipes** (`release-manual-validate`, `release-manual-build`, `release-manual-publish`, `release-manual-all`) unless the user explicitly asks for a local publish. They reproduce `publish.yml`'s pipeline sequentially on one machine, take ~1h to complete, and exist only as a fallback for when CI is unavailable.


## Key Source Locations

### Settings
- `src/settings/ui/facade/UserSettingsPage.cpp` / `.h` -- User settings Qt properties and facade logic
- Settings stored per profile in `~/.config/komai/profiles/<profile-id>/` (`<profile-id>` is the `-p` profile identifier)
- Main files: `config.yml`, `state.yml`, `session.yml`, and `secrets.yml` (file-provider fallback only)
- User-facing settings docs: `docs/user-guide/settings/README.md`
- See [docs/architecture/settings/README.md](docs/architecture/settings/README.md) for details

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
- See [docs/user-guide/features/themes.md](docs/user-guide/features/themes.md) for theme format

### Icons
- `resources/icons/ui/*.svg` -- UI icon assets (mostly Fluent-derived + some Komai-authored icons)
- `resources/icons/emoji-categories/*.svg` -- Emoji category tab icons
- `resources/res.qrc` -- icon resource registration used by Qt/QML
- See [docs/architecture/icons.md](docs/architecture/icons.md) for icon sourcing/licensing/update workflow

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

Access via `Komai.theme.*`:
- `Komai.theme.red` -- Error/loud notification color
- `Komai.paddingSmall` (4px), `Komai.paddingMedium` (8px)


## Design Philosophy

- **Desktop-first UX** -- Optimized for large screens with always-visible room list
- **Readable by everyone** -- All text must be comfortably readable at default settings
- **Subtle but effective** -- Visual changes noticeable without being jarring


## Documentation

- [docs/README.md](docs/README.md) -- User/docs index
- [docs/architecture/README.md](docs/architecture/README.md) -- Technical docs index (start here, then open only task-relevant pages)
- [docs/user-guide/differences-from-nheko.md](docs/user-guide/differences-from-nheko.md) -- What makes Komai different
- [docs/maintainers/live-verification.md](docs/maintainers/live-verification.md) -- Driving a real running Komai from a shell (headless launch, CLI/IPC, seeded test homeserver) to verify changes end-to-end

## Translatable Strings Checklist

When you add, change, or remove a `qsTr(...)` / `tr(...)` string in C++, QML, or a Rust-originated translation module, the workflow is:

1. `just translations-update` -- regenerates `.ts` files via `lupdate` and re-normalizes them. New strings appear as `type="unfinished"` in every non-English language.
2. `just translations-claude-translate-all` -- AI-fills those `unfinished` entries across all 32 non-English languages.
3. Stage and commit the source change **and** the `resources/langs/` updates **together**. The pre-commit drift hook (`bin/prek/translations-drift.py`) will refuse a commit that touches translatable source without matching `.ts` updates.

The English file (`resources/langs/en/komai_en.ts`) is the reference -- it always has many `unfinished` entries by design and is excluded from the AI-fill recipe.

See [docs/maintainers/translations.md](docs/maintainers/translations.md) for the full pipeline (per-language guides, validation rules, single-language re-runs).


## Icon Change Checklist

When adding/changing icons:

- Keep icons vendored under `resources/icons/` (do not add runtime network fetches in build paths).
- Update `resources/res.qrc` for any new or removed icon file.
- Run `just icons-audit` to detect reference/qrc/file mismatches.
- Prefer existing icons over adding near-duplicates.
- For Fluent-derived icons, keep attribution/licensing aligned with `resources/icons/REUSE.toml` and `docs/architecture/icons.md`.


## Environment

- OS: Linux (primary), macOS, Windows
- Qt version: 6.x
- Compiler: GCC/Clang with C++20 support
