# Release Tooling 🚀

This directory contains scripts that prepare, validate, build, and publish a Komai release. Shared infrastructure: `publish.yml`'s release job calls `publish.py` directly, and the local fallback is exposed via `just release-prepare` and `just release-manual-*` recipes. See [`docs/maintainers/releases.md`](../../docs/maintainers/releases.md) for the maintainer-facing flow.

## Why It Exists

The canonical release path is the [`publish.yml`](../../.github/workflows/publish.yml) GitHub Actions workflow. The same release-creation logic (CHANGELOG extraction, `gh release create`) needs to run locally too — for debugging packaging recipes or recovering when CI is unavailable — so it lives here as Python scripts that both invocation paths share.

## Files

- `prepare.py` - bumps `VERSION.txt` and propagates the change to `PKGBUILD`, `CHANGELOG.md`, and `appdata.xml.in` for a new release.
- `validate.py` - runs eight independent prerequisite gates before publishing (clean tree, tag, CHANGELOG, drift, `gh` auth, etc.).
- `build.py` - drives the three packaging recipes (`appimage-build-docker` / `flatpak-build` / `snap-build-docker`) and verifies output paths.
- `publish.py` - extracts the CHANGELOG section as release notes and runs `gh release create`. Hard-fails on existing release; no `--force`.
- `all.py` - orchestrator: validate → build → publish, with `--dry-run` propagation.
- `_lib.py` - shared helpers (`REPO_ROOT`, version reading, expected artefact paths, `check_tools`, CHANGELOG section extractor).

## Command Mapping

- `just release-prepare [VERSION]` -> `prepare.py`
- `just release-validate` -> `validate.py`
- `just release-build` -> `build.py`
- `just release-publish [--dry-run]` -> `publish.py`
- `just release-all [--dry-run]` -> `all.py`

## Dependencies

- `python3`
- `git`, `gh` (authenticated with `repo` scope on the target repo)
- `just`, `docker`, `flatpak-builder` (for `build.py` only)

Related docs:

- [`docs/maintainers/releases.md`](../../docs/maintainers/releases.md)
- [`docs/architecture/ci.md`](../../docs/architecture/ci.md)
