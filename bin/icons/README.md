# Icons Tooling

This directory provides icon maintenance helpers for Komai; start with [`docs/architecture/icons.md`](../../docs/architecture/icons.md).

Quick orientation:

- Komai vendors icons under `resources/icons/` for reproducible/offline builds.
- Sync from upstream is explicit (`just icons-sync-fluent`, `just icons-sync-fontawesome`), not part of normal build paths.
- Use `just icons-audit` to check code/qrc/files consistency.
- Fluent-mirrored files live under `resources/icons/fluent/` with upstream-relative paths.
- Font Awesome files live under `resources/icons/fontawesome/` mirroring upstream `svgs/<style>/` structure.

## Shared Files

- `audit.sh` - reports icon mismatches across code references, `resources/res.qrc`, and files on disk.
- `generate-derived.py` - regenerates local derived icons from vendored Fluent source icons (currently `ui/double-checkmark.svg` from Fluent `Checkmark`).
- `generate-list.py` - generates `docs/architecture/icons-list.md` from qrc aliases/source mappings.

## Fluent UI System Icons (`fluent/`)

Source: [`microsoft/fluentui-system-icons`](https://github.com/microsoft/fluentui-system-icons) (MIT)

- `fluent/fetch.sh` - fetches one Fluent icon by upstream path into `resources/icons/fluent/` and updates `resources/res.qrc` alias wiring.
  - default alias target is `icons/ui/<ALIAS_SVG_NAME>.svg`
  - you may pass an explicit alias path like `emoji-categories/activity.svg`
- `fluent/sync.sh` - syncs all mirrored Fluent files from pinned upstream.
- `fluent/repo.conf` - pinned upstream repository.
- `fluent/VERSION_REF` - pinned Fluent version/tag/commit reference.

## Font Awesome (`fontawesome/`)

Source: [`FortAwesome/Font-Awesome`](https://github.com/FortAwesome/Font-Awesome) (CC BY 4.0 for icons)

- `fontawesome/fetch.sh` - fetches one Font Awesome icon by upstream path into `resources/icons/fontawesome/` and updates `resources/res.qrc` alias wiring.
  - default alias target is `icons/ui/<ALIAS_SVG_NAME>.svg`
- `fontawesome/sync.sh` - syncs all mirrored Font Awesome files from pinned upstream.
- `fontawesome/repo.conf` - pinned upstream repository.
- `fontawesome/VERSION_REF` - pinned Font Awesome version/tag reference.

## Command Mapping

- `just icons-audit` -> `bin/icons/audit.sh`
- `just icons-generate-derived` -> `bin/icons/generate-derived.py`
- `just icons-generate-list` -> `bin/icons/generate-list.py`
- `just icons-fetch-fluent` -> `bin/icons/fluent/fetch.sh`
- `just icons-sync-fluent` -> `bin/icons/fluent/sync.sh`
- `just icons-fetch-fontawesome` -> `bin/icons/fontawesome/fetch.sh`
- `just icons-sync-fontawesome` -> `bin/icons/fontawesome/sync.sh`

## Notes

- Sync scripts require `curl` in the environment.
- Keep licensing metadata aligned when adding/updating third-party icons:
  - `resources/icons/REUSE.toml`
  - `LICENSES/MIT.txt` (Fluent)
  - `LICENSES/CC-BY-4.0.txt` (Font Awesome)
