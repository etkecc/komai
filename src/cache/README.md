# Cache Module

This directory contains Komai's Matrix-specific persistence/cache layer.

Scope:

- Reads/writes Matrix state and cache data.
- Depends on `src/db` storage APIs and helpers.
- Must not expose backend-specific details to callers.

Boundary rules:

- `src/cache` may depend on `src/db`.
- `src/db` must not depend on `src/cache` or Matrix/UI domain objects.

Public entry header:

- Include `cache/Cache.h` for application-facing cache APIs.
- Internal wrapper declaration surface lives in `cache/api/CacheApi.h`.

Architecture docs:

- [Cache Architecture](../../docs/architecture/cache/README.md)
- [Path Migration Map](../../docs/architecture/cache/migration-map.md)
- [Storage Architecture](../../docs/architecture/storage.md)
