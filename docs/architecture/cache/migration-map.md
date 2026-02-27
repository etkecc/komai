# Cache Path Migration Map

This map documents the first-stage move from top-level `src/Cache*` files to the structured `src/cache/*` layout.

## Prefix Mapping

- `src/CacheApiWrappers*` -> `src/cache/api/CacheApiWrappers*`
- `src/CacheCore*`, `src/Cache.h`, `src/Cache_p.h`, `src/CacheStructs.h` -> `src/cache/core/*`
- `src/CacheSetup.cpp` -> `src/cache/setup/CacheSetup.cpp`
- `src/CacheLifecycle*` -> `src/cache/lifecycle/CacheLifecycle*`
- `src/CacheTimeline*` -> `src/cache/timeline/CacheTimeline*`
- `src/CacheRoomInfo*` -> `src/cache/rooms/CacheRoomInfo*`
- `src/CacheMembership*` -> `src/cache/membership/CacheMembership*`
- `src/CacheSpaces*` -> `src/cache/spaces/CacheSpaces*`
- `src/CacheUser*` -> `src/cache/users/CacheUser*`
- `src/CacheCrypto*` -> `src/cache/crypto/CacheCrypto*`

## Transitional Include Policy

- Legacy include entry points at `src/Cache*.h` are forwarding headers during migration.
- Internal `src/cache/*` code should include `cache/*` headers directly.
