# 🏗️ Architecture Docs

Technical documentation for Komai internals.

## Core Systems

- ⚙️ [Settings Architecture](settings/README.md) -- persistence model, load order, and secret-provider behavior
- 🧱 [Settings Migrations](settings/migrations.md) -- schema versioning and migration strategy
- 🧭 [Settings 3-Layer Mapping](settings/3-layer-mapping.md) -- `SettingId` ↔ runtime getter ↔ persisted key audit report
- 🧩 [Icons Architecture](icons.md) -- icon source of truth, validation, and Fluent sync workflow
- 😀 [Emoji Architecture](emojis.md) -- upstream sources, localization pipeline, overrides, and runtime loading
- 📦 [Binary Size Notes](binary-size.md) -- why local build-tree binaries are larger than packaged installs
- 💾 [Storage Architecture](storage.md) -- path helpers, storage layout, and callsites
- 🗃️ [Cache Architecture](cache/README.md) -- Matrix cache domain built on storage APIs
- 🧾 [Cache Storage Invariants](cache/storage-invariants.md) -- cache reset rules, room cleanup, read-receipt semantics, and derived-edge invariants
- 🧩 [QML/UI Structure](qml-ui.md) -- QML layering, placement rules, and loader decoupling
- ⌨️ [Keyboard Shortcuts Architecture](keyboard-shortcuts.md) -- shortcut layers, Selection mode key flow, and layout-agnostic Latin-key handling
- 🧵 [Timeline HTML Rendering](timeline-html-rendering.md) -- formatted message pipeline, sanitization, and code highlighting
- 🎨 [Themes Architecture](themes.md) -- theme sources, generation, and runtime mapping
- 🎯 [Theme Design Guide](theme-design-guide.md) -- palette role semantics, contrast targets, and authoring guardrails
- 🌐 [Translations Architecture](translations.md) -- TS update/normalize flow and tooling
- ⚡ [Performance Tracing](performance.md) -- room-switch perf markers, runtime knobs, and logging controls
- 🏘️ [Communities Sidebar Filters](communities-sidebar-filters.md) -- filter architecture, tag IDs, room-level filtering, and how to add new filters
- 🔙 [Navigation History](navigation-history.md) -- back/forward navigation via mouse buttons, in-memory history stack
- 🎬 [Media Overlay](media-overlay.md) -- full-screen image/video viewer, gallery navigation, and streaming playback
- 🎵 [Audio Playback](audio-playback.md) -- inline audio player behavior, playback-rate controls, and room-local v1 limitations
- 🦀 [Rust in Komai](rust.md) -- CXX interop pattern, directory layout, build integration, and packaging

## Differences from nheko

- 🔀 [Overview](differences-from-nheko/README.md) -- high-level architecture and behavior differences
- ⚙️ [Settings Differences](differences-from-nheko/settings.md) -- settings model deltas and migration implications
- 🔐 [Secret Storage Differences](differences-from-nheko/secret-services.md) -- secret-service and fallback behavior
- 🧭 [Settings Name Mapping](differences-from-nheko/settings-mapping.md) -- settings keys and naming transitions

## Settings Examples

- 🧾 [Profile Example: config.yml](../user-guide/settings/examples/profile/config.yml)
- 🧾 [Profile Example: state.yml](../user-guide/settings/examples/profile/state.yml)
- 🧾 [Profile Example: session.yml](../user-guide/settings/examples/profile/session.yml)
- 🧾 [Profile Example: secrets.yml](../user-guide/settings/examples/profile/secrets.yml)
