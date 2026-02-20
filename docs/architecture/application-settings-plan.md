# Application Settings Persistence Rework Plan

## Scope

Rework settings persistence from a single flat profile YAML file to a per-profile directory with split files and hierarchical keys.

- Existing runtime/QML `UserSettings` API should remain flat for now.
- Hierarchy and file split are persistence concerns.

## Mapping Contract (Single Source of Truth)

All key names, target files, and persistence decisions must follow:

- `docs/architecture/application-settings-hierarchy-proposal.md`

Implementation rule:

- If code and plan diverge, update the proposal doc first, then implement.
- Do not introduce ad-hoc keys not present in the proposal table.
- Keep `docs/architecture/application-settings-inventory.md` synchronized with any persistence-surface changes.

## Target Profile Layout

Per profile directory:

- `~/.config/komai/profiles/<profile-id>/config.yml`
- `~/.config/komai/profiles/<profile-id>/state.yml`
- `~/.config/komai/profiles/<profile-id>/session.yml`
- `~/.config/komai/profiles/<profile-id>/secrets.yml` (only when `secrets.provider=file`)

`<profile-id>` should be `default` for the default profile.

## Persistence Boundaries

### `config.yml`

Durable preferences and advanced non-secret options:

- `app.*`, `ui.*`, `sidebars.*`, `timeline.*`, `composer.*`, `notifications.*`, `calls.*`, `privacy.*`, `encryption.*`
- `network.*` (for example `network.http3.enabled`, `network.tls.disable_certificate_validation`)
- `db.*` (for example `db.max_size_bytes`, `db.max_files`)
- `integrations.*`
- `secrets.provider` (`secret_service` or `file`)

### `state.yml`

Volatile/runtime UI state:

- window geometry
- sidebar widths
- collapsed communities/spaces
- hidden lists and recent reactions
- navigation cursor/state (`current_tag_id` equivalent)

### `session.yml`

Session/account metadata:

- account identity (`user_id`, `homeserver`, `device_id`)
- presence default

### `secrets.yml`

File-backed fallback secrets (only when `secrets.provider=file`):

- `auth.access_token`
- `secrets` map

## Security Policy

- Preferred mode: `secrets.provider=secret_service`.
- In preferred mode, sensitive values are read from secure backend (QtKeychain/OS secret service), not from `config.yml`/`state.yml`.
- Fallback mode: `secrets.provider=file`, secrets live in `secrets.yml`.
- `secrets.yml` must be written with strict permissions (owner read/write only).
- In preferred mode, `session.yml` keeps only non-secret session metadata.
- Secure backend key IDs are profile-scoped and deterministic:
  - `komai.<profile_hash>.settings.<key>`
  - `komai.<profile_hash>.local_crypto.<key>`
  - `komai.<profile_hash>.matrix.<key>`
  - where `profile_hash = hex(sha256(normalized_profile_id))`.
  - default profile hash convenience value: `37a8eec1ce19687d132fe29051dca629d164e2c4958ba141d5f4133a33f0688f`
- Legacy base64-profile-hash secret IDs are intentionally not used.
- In `file` mode, these same IDs are serialized under `secrets.yml` -> `secrets` (map key = secret ID).

## Implementation Strategy

### Phase 1: File Path and I/O Split

1. Replace current single `configFilePath_` semantics with profile-directory paths:
   - `configFilePath(profile)` -> `.../profiles/<profile>/config.yml`
   - new helpers for `state.yml`, `session.yml`, and `secrets.yml`
2. Keep current load/save orchestration methods but internally split into:
   - `loadConfigYaml()` / `saveConfigYaml()`
   - `loadStateYaml()` / `saveStateYaml()`
   - `loadSessionYaml()` / `saveSessionYaml()`
   - `loadSecretsYaml()` / `saveSecretsYaml()`
3. Keep all current in-memory members unchanged.

Done checklist (Phase 1):

- `UserSettings` can resolve paths for `config.yml`, `state.yml`, `session.yml`, and `secrets.yml` per profile.
- Load/save code paths are split by file concern.
- `just build` passes.

### Phase 2: Hierarchical Key Helpers

1. Introduce local YAML helpers for dotted paths (read/write nested maps):
   - `getNode(root, "a.b.c")`
   - typed readers with defaults (`getBool`, `getInt`, `getString`, list readers)
   - typed writers by dotted key.
2. Remove flat-key string literals in new save/load paths.
3. Keep key names centralized (constants table or grouped constexpr values).

Done checklist (Phase 2):

- Dotted-key read/write helpers are used for all new persistence code.
- Flat key literals are not duplicated across load/save logic.
- Key constants are centralized and reviewable.
- `just build` passes.

### Phase 3: Map Members to Target Files

1. Apply mapping from `application-settings-hierarchy-proposal.md`.
2. Move non-UI persisted variables into `config.yml`, `state.yml`, `session.yml`, or `secrets.yml` per proposal.
3. Explicitly handle special conversions:
   - `run_without_secure_secrets_service` -> `secrets.provider`
   - enum storage as readable strings where applicable.

Done checklist (Phase 3):

- Every currently persisted variable is mapped to exactly one target file or secure backend.
- `network.*` and `db.*` keys are implemented per proposal.
- `run_without_secure_secrets_service` conversion behavior is implemented and documented in code comments.
- `just build` passes.

### Phase 4: Secrets Source Logic

1. Load `config.yml` first.
2. Resolve provider:
   - `secret_service` -> read access token + secrets from secure backend.
   - `file` -> read from `secrets.yml`.
3. Save path mirrors provider decision.
4. Ensure secrets never leak into `config.yml` or `state.yml`.

Done checklist (Phase 4):

- `access_token` is absent from `config.yml` and `state.yml`.
- In `secret_service` mode, token/secrets are read from secure backend only.
- In `file` mode, token/secrets are read/written in `secrets.yml` only.
- `secrets.yml` permissions are restricted for file-backed secrets.
- `just build` passes.

### Phase 5: Session Decoupling Cleanup (Optional but Recommended)

1. Keep `UserSettings` flat initially.
2. If desired later, extract session persistence into a dedicated helper class without changing QML property surface.

Done checklist (Phase 5):

- Optional helper abstraction does not change QML-visible API.
- Persistence behavior remains identical to Phases 1-4.
- `just build` passes.

## Exact C++ Touchpoints

Primary implementation files:

- `src/UserSettingsPage.cpp`
- `src/UserSettingsPage.h`

Functions/areas to modify:

- `configDir()` and path helpers near file top.
- Existing `configFilePath(const QString &profile)` (replace semantics to profile directory + add file-specific helpers).
- `UserSettings::load(std::optional<QString> profile)` (split into file-specific loading).
- `UserSettings::save()` (split into file-specific saving).
- YAML helper lambdas currently scoped inside `load()`/`save()` (replace with reusable dotted-key helpers).
- Secrets fallback handling (`runWithoutSecureSecretsService_`, `secrets_`, access token read/write logic).
- Any member(s) currently representing single-file path state (`configFilePath_`) to support multi-file paths.

Secondary references to keep consistent:

- `docs/architecture/application-settings-hierarchy-proposal.md`
- `docs/architecture/application-settings-inventory.md`
- `docs/architecture/configuration-examples/profile/config.yml`
- `docs/architecture/configuration-examples/profile/state.yml`
- `docs/architecture/configuration-examples/profile/session.yml`
- `docs/architecture/configuration-examples/profile/secrets.yml`

## Documentation Tasks

1. Keep `docs/architecture/application-settings-inventory.md` updated with:
   - UI table entries
   - non-UI persisted variables
2. Keep `docs/architecture/application-settings-hierarchy-proposal.md` updated with:
   - proposed hierarchical keys
   - target file (`config.yml`, `state.yml`, `session.yml`, `secrets.yml`, secure backend)
3. Maintain sample files under:
   - `docs/architecture/configuration-examples/profile/config.yml`
   - `docs/architecture/configuration-examples/profile/state.yml`
   - `docs/architecture/configuration-examples/profile/session.yml`
   - `docs/architecture/configuration-examples/profile/secrets.yml`

## Validation and Acceptance Criteria

### Functional

- App starts with empty profile directory and writes valid `config.yml`, `state.yml`, `session.yml`; `secrets.yml` only when `secrets.provider=file`.
- User preference changes write only to the intended file.
- Runtime-only changes update only `state.yml`.
- Session identity updates write to `session.yml`.
- Access token handling follows `secrets.provider`.

### Security

- `access_token` and secret map are absent from `config.yml` and `state.yml`.
- In `file` mode, `secrets.yml` permissions are restricted.

### Code Quality

- No behavior regressions in current Settings UI.
- `just build` passes after C++ changes.
- Key mappings are centralized and testable.

## Suggested Test Matrix

1. Fresh start with default profile.
2. Fresh start with named profile.
3. Toggle each major settings group and verify target file writes.
4. Switch `secrets.provider` between `secret_service` and `file` and verify token source behavior.
5. Verify secure backend key IDs use the profile-first hex format and expected namespaces.
6. Restart app and verify state restoration (window size, sidebar widths, collapsed spaces).

## Out of Scope (for this iteration)

- Full runtime refactor of `UserSettings` into nested QObject groups.
