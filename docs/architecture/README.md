# 🏗️ Architecture Docs

Technical documentation for Komai internals.

## Core Systems

- ⚙️ [Configuration Architecture](configuration.md) -- persistence model, load order, and secret-provider behavior
- 🧩 [Icons Architecture](icons.md) -- icon source of truth, validation, and Fluent sync workflow
- 💾 [Storage Architecture](storage.md) -- path helpers, storage layout, and callsites
- 🎨 [Themes Architecture](themes.md) -- theme sources, generation, and runtime mapping
- 🌐 [Translations Architecture](translations.md) -- TS update/normalize flow and tooling

## Differences from nheko

- 🔀 [Overview](differences-from-nheko/README.md) -- high-level architecture and behavior differences
- ⚙️ [Configuration Differences](differences-from-nheko/configuration.md) -- config model deltas and migration implications
- 🔐 [Secret Storage Differences](differences-from-nheko/secret-services.md) -- secret-service and fallback behavior
- 🧭 [Settings Name Mapping](differences-from-nheko/settings-mapping.md) -- settings keys and naming transitions

## Configuration Examples

- 🧾 [Profile Example: config.yml](configuration-examples/profile/config.yml)
- 🧾 [Profile Example: state.yml](configuration-examples/profile/state.yml)
- 🧾 [Profile Example: session.yml](configuration-examples/profile/session.yml)
- 🧾 [Profile Example: secrets.yml](configuration-examples/profile/secrets.yml)
