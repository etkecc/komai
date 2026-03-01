# 🏗️ Architecture Docs

Technical documentation for Komai internals.

## Core Systems

- ⚙️ [Settings Architecture](settings/README.md) -- persistence model, load order, and secret-provider behavior
- 🧱 [Settings Migrations](settings/migrations.md) -- schema versioning and migration strategy
- 🧭 [Settings 3-Layer Mapping](settings/3-layer-mapping.md) -- `SettingId` ↔ runtime getter ↔ persisted key audit report
- 🧩 [Icons Architecture](icons.md) -- icon source of truth, validation, and Fluent sync workflow
- 💾 [Storage Architecture](storage.md) -- path helpers, storage layout, and callsites
- 🗃️ [Cache Architecture](cache/README.md) -- Matrix cache domain built on storage APIs
- 🧩 [QML/UI Structure](qml-ui.md) -- QML layering, placement rules, and loader decoupling
- 🎨 [Themes Architecture](themes.md) -- theme sources, generation, and runtime mapping
- 🌐 [Translations Architecture](translations.md) -- TS update/normalize flow and tooling
- ⚡ [Performance Tracing](performance.md) -- room-switch perf markers, runtime knobs, and logging controls

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
