# Store Module (Scaffold)

This directory is reserved for future non-Matrix persisted state modules.

Design intent:

- Keep `src/cache` (`MatrixStore`) focused on Matrix-domain persistence.
- Build any app-generic or non-Matrix persisted state modules here.
- Reuse `src/db` as the low-level storage abstraction.

Boundary rules:

- `src/store` may depend on `src/db`.
- `src/db` must not depend on `src/store`.
- `src/store` must not leak backend-specific implementation details.

Current status:

- Scaffold only; no production store modules are implemented yet.
