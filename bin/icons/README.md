# Icons Tooling 🧩

This directory provides icon maintenance helpers for Komai; start with [`docs/architecture/icons.md`](../../docs/architecture/icons.md) and Fluent upstream [`fluentui-system-icons`](https://github.com/microsoft/fluentui-system-icons).

Quick orientation:

- Komai vendors icons under `resources/icons/` for reproducible/offline builds.
- Sync from upstream is explicit (`just icons-sync`), not part of normal build paths.
- Use `just icons-audit` to check code/qrc/files consistency.
- Fluent-mirrored files live under `resources/icons/fluent/` with upstream-relative paths.

## Files

- `audit.sh` - reports icon mismatches across code references, `resources/res.qrc`, and files on disk.
- `generate-derived.py` - regenerates local derived icons from vendored Fluent source icons (currently `ui/double-checkmark.svg` from Fluent `Checkmark`).
- `generate-list.py` - generates `docs/architecture/icons-list.md` from qrc aliases/source mappings.
- `fetch.sh` - fetches one Fluent icon by upstream path into `resources/icons/fluent/` and updates `resources/res.qrc` alias wiring.
  - default alias target is `icons/ui/<ALIAS_SVG_NAME>.svg`
  - you may pass an explicit alias path like `emoji-categories/activity.svg`
  - both upstream path and alias target are validated to end with `.svg` (alias may omit extension)
- `sync-fluent.sh` - syncs all mirrored Fluent files from pinned upstream.
- `fluent-icons.lock` - pinned upstream repository.
- `FLUENT_VERSION_REF` - pinned Fluent version/tag/commit reference.

## Command Mapping

- `just icons-audit` -> `bin/icons/audit.sh`
- `just icons-generate-derived` -> `bin/icons/generate-derived.py`
- `just icons-generate-list` -> `bin/icons/generate-list.py`
- `just icons-fetch` -> `bin/icons/fetch.sh`
- `just icons-sync` -> `bin/icons/sync-fluent.sh`

## Notes

- `sync-fluent.sh` requires:
  - pinned ref in `FLUENT_VERSION_REF`
  - `curl` in the environment
- `sync-fluent.sh` reads all `*.svg` files already present under `resources/icons/fluent/`.
- Keep licensing metadata aligned when adding/updating third-party icons:
  - `resources/icons/REUSE.toml`
  - `LICENSES/MIT.txt`
