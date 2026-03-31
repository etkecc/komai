# Store Module (Scaffold)

This directory is reserved for future app-owned persisted state modules.

Design intent:

- Build app-owned or non-Matrix persisted state modules here.
- Reuse `src/db` as the low-level storage abstraction.

Boundary rules:

- `src/store` may depend on `src/db`.
- `src/db` must not depend on `src/store`.
- `src/store` must not leak backend-specific implementation details.

Current status:

- Scaffold only; no production store modules are implemented yet.
