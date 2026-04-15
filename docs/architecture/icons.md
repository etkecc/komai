# Icons Architecture

This document describes how Komai stores, references, and maintains icon assets.

## Quick Links

- 🎨 Visual catalog: [Icon Catalog](icons-list.md) (generated from `resources/res.qrc`)

## Locations

- `resources/icons/ui/*.svg`: main UI icons.
- `resources/icons/emoji-categories/*.svg`: emoji picker category icons.
- `resources/icons/fluent/**`: mirrored Fluent upstream paths (selected subset).
- `resources/res.qrc`: Qt resource registration for icons (compiled into the app).
- `bin/icons/`: shared icon tooling (`audit.sh`, `generate-derived.py`, `generate-list.py`).
- `resources/icons/fontawesome/**`: mirrored Font Awesome upstream paths (selected subset).
- `bin/icons/fluent/`: Fluent fetch/sync scripts and version pin.
- `bin/icons/fontawesome/`: Font Awesome fetch/sync scripts and version pin.

## Runtime Usage Model

Icons are loaded via Qt resource paths, typically:

- `qrc:/icons/icons/ui/<name>.svg`
- `:/icons/icons/ui/<name>.svg`
- `image://colorimage/:/icons/icons/ui/<name>.svg?<color>`

Because resources are compiled in, missing files or stale qrc entries are build/runtime defects.

## Source of Truth and Provenance

- Most modern UI icons were introduced by the Fluent switch (`8f50e891`).
- Some icons are Komai-authored custom assets (for example `plus-circle.svg`, `toggles.svg`, `state-event.svg`, `sidebar.svg`, `integrations.svg`).
- Provenance/licensing metadata is tracked in:
  - `resources/icons/REUSE.toml`

## Current Policy (Komai)

- Keep icons vendored in-repo under `resources/icons/` for reproducible/offline builds.
- Do not fetch icons dynamically during `cmake`/`just build`.
- Prefer a small curated icon set over mass import.
- Use pinned upstream references for any sync operation.

## Tooling

- `just icons-audit`: report mismatches between code refs, qrc entries, and files on disk.
- `just icons-generate-list`: regenerate icon catalog table used for visual review in docs.
- `just icons-generate-derived`: regenerate local derived icons from vendored Fluent sources.
- `just icons-fetch-fluent <REL_PATH> <ALIAS_SVG_NAME>`: fetch one Fluent icon into `resources/icons/fluent/` and add/update qrc alias (`icons/ui/*.svg` by default).
- `just icons-sync-fluent`: sync mirrored Fluent icons from pinned upstream ref.
- `just icons-fetch-fontawesome <REL_PATH> <ALIAS_SVG_NAME>`: fetch one Font Awesome icon into `resources/icons/fontawesome/` and add/update qrc alias.
- `just icons-sync-fontawesome`: sync mirrored Font Awesome icons from pinned upstream ref.

Generated catalog:

- `docs/architecture/icons-list.md`

Sync config files:

- `bin/icons/fluent/repo.conf`:
  - `FLUENT_REPO` (for example `microsoft/fluentui-system-icons`)
- `bin/icons/fluent/VERSION_REF`:
  - pinned upstream version/tag/commit ref
  - value is used as-is (for example `1.1.319`)
- `bin/icons/fontawesome/repo.conf`:
  - `FA_REPO` (for example `FortAwesome/Font-Awesome`)
- `bin/icons/fontawesome/VERSION_REF`:
  - pinned upstream version/tag (for example `7.2.0`)

Sync scope:

- `just icons-sync-fluent` updates every `*.svg` under `resources/icons/fluent/`.
- For each local file, the path relative to `resources/icons/fluent/` is treated as the upstream path.
- `just icons-sync-fluent` also regenerates derived local icons (currently `resources/icons/ui/double-checkmark.svg`) from pinned Fluent sources.
- `just icons-sync-fontawesome` updates every `*.svg` under `resources/icons/fontawesome/`.

## Add/Update Workflow

1. Pin/update ref in the relevant `VERSION_REF` file (`bin/icons/fluent/` or `bin/icons/fontawesome/`).
2. Place mirrored files under `resources/icons/fluent/` or `resources/icons/fontawesome/` (upstream-relative paths).
3. Run `just icons-sync-fluent` or `just icons-sync-fontawesome` (append `--dry-run` to preview).
4. Add/remove corresponding entries in `resources/res.qrc`.
5. Update QML/C++ references if path/name changed.
6. Run lint/build checks:
   - `just lint`
   - `just build`
7. Verify icon consistency:
   - `just icons-audit`

## Remove Workflow

1. Confirm icon is unused in `resources/qml` and `src`.
2. Remove references from QML/C++.
3. Remove `resources/res.qrc` entry.
4. Delete SVG file.
5. Run `just icons-audit`, `just lint`, and `just build`.

## Licensing and Attribution

- Fluent UI System Icons are MIT licensed; using MIT assets in this GPL project is allowed.
- Font Awesome Free icons are CC BY 4.0 licensed; compatible with GPL, requires attribution.
- We must preserve proper license attribution/metadata for third-party assets.
- License metadata lives in `resources/icons/REUSE.toml`.
- Keep `resources/icons/REUSE.toml` and this document in sync when icon sources change.
