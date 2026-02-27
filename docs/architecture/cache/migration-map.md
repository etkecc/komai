# Cache Path Migration Map

This map documents the first-stage move from top-level `src/Cache*` files to the structured `src/cache/*` layout.

## Prefix Mapping

- `src/CacheApiWrappers*` -> `src/cache/api/CacheApi*`
- `src/CacheCore*`, `src/Cache.h`, `src/Cache_p.h`, `src/CacheStructs.h` -> `src/cache/core/*`
- `src/CacheSetup.cpp` -> `src/cache/setup/CacheSetup.cpp`
- `src/CacheLifecycle*` -> `src/cache/lifecycle/CacheLifecycle*`
- `src/CacheTimeline*` -> `src/cache/timeline/CacheTimeline*`
- `src/CacheRoomInfo*` -> `src/cache/rooms/CacheRoomInfo*`
- `src/CacheMembership*` -> `src/cache/membership/CacheMembership*`
- `src/CacheSpaces*` -> `src/cache/spaces/CacheSpaces*`
- `src/CacheUser*` -> `src/cache/users/CacheUser*`
- `src/CacheCrypto*` -> `src/cache/crypto/CacheCrypto*`

## Include Policy

- Application-facing call sites should include `cache/Cache.h`.
- Cache-module internals may include narrower `cache/*` submodule headers where needed.
