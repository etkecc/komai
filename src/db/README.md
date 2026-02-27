# DB Module

This directory contains Komai's backend-neutral storage layer.

Scope:

- Provides database lifecycle, store access, cursor/scan helpers, typed key/value codecs, and
  schema/migration primitives.
- Exposes reusable APIs for persistence consumers (`src/cache` today, future `src/store` modules).
- Must remain project/domain-agnostic.

Boundary rules:

- `src/db` must not depend on `src/cache`, `src/store`, UI types, or Matrix domain objects.
- Matrix-domain persistence belongs in `src/cache` (`MatrixStore`) and should consume `src/db`
  through typed helpers only.
- Backend-specific details stay inside backend implementation files (`*Backend*`, backend internals),
  not in higher-level consumers.

API-surface guidelines:

- Prefer focused public headers under `db/storage/*` instead of broad umbrella includes.
- Keep each header grouped by concern and owned by a small set of source modules.
- Avoid large cross-cutting re-export lists that hide dependencies.
- In public/internal headers, include only minimal type/lifecycle surfaces; pull heavier helper groups
  in `.cpp` files.
- Preserve behavior and persistence format when reshaping headers.

Target `db/storage/*` groups and ownership map:

- `db/storage/Core.h`
  - Owns: backend id/support checks, open/close/id/category, transaction helpers, shared storage types.
  - Backed by: `Backend.h`, `Factory.cpp`, transaction/capability helpers in `StorageApi.h`.
- `db/storage/Catalog.h`
  - Owns: logical DB naming and key naming builders.
  - Backed by: `Catalog.h/.cpp`.
- `db/storage/Open.h`
  - Owns: `open*Store` helpers and open-options policy (`openOptionsFor*`).
  - Backed by: `NamePolicy.h/.cpp`, store-open helpers in `StorageApi.h`.
- `db/storage/Scan.h`
  - Owns: key/value listing and cursor-iteration helpers.
  - Backed by: `Scan.h/.cpp`, `DupIndex.h/.cpp`.
- `db/storage/Timeline.h`
  - Owns: timeline/order/index/reference helpers.
  - Backed by: `TimelineIndex.h/.cpp`, `OrderEntry.h/.cpp`.
- `db/storage/State.h`
  - Owns: state-event index helpers.
  - Backed by: `StateIndex.h/.cpp`.
- `db/storage/SyncState.h`
  - Owns: sync-state typed accessors (`next_batch`, cache format, secrets).
  - Backed by: `SyncState.h/.cpp`.
- `db/storage/Crypto.h`
  - Owns: OLM/Megolm/read-receipt typed helpers.
  - Backed by: `OlmSessionIndex.h/.cpp`, `MegolmIndex.h/.cpp`, `ReadReceiptIndex.h/.cpp`.
- `db/storage/Serde.h`
  - Owns: typed value JSON serde helpers used across storage helpers.
  - Backed by: `Json.h`, `Serde.h`.

Compatibility:

- `db/StorageApi.h` remains the compatibility umbrella during migration.
- New/updated callsites should prefer focused `db/storage/*` headers first.
