# Emoji Data Pipeline 🙂

This directory hosts the emoji data pipeline for Komai.

Data sources are fetched into `var/emoji/` from pinned upstream refs in
[`sources.lock.yml`](./sources.lock.yml), then transformed into compact runtime
JSON embedded into the app binary via Qt resources.

## Commands

- `just emoji-fetch` -> fetch/refresh upstream cache in `var/emoji/cache/<lock-hash>/`
- `just emoji-build` -> generate runtime emoji JSON files in `var/emoji/generated/<lock-hash>/`
- `just emoji-check` -> validate lock + overrides and run an offline cache-based build check
- `just emoji-update-lock` -> re-pin the CLDR tarball sha256s after a version bump
- `just emoji-add-token "<emoji>" <locale> "<token>"` -> append a token override entry

## Files

- `pipeline.py` - fetch/build/check helper script.
- `sources.lock.yml` - pinned upstream source configuration.
- `../../resources/emoji/overrides/` - tracked local overrides (YAML).

## Upstream Sources

- Unicode emoji test data:
  - https://unicode.org/Public/emoji/
- CLDR annotations (sha256-pinned npm tarballs of https://github.com/unicode-org/cldr-json):
  - https://www.npmjs.com/package/cldr-annotations-full
  - https://www.npmjs.com/package/cldr-annotations-derived-full

Related docs:

- [`docs/maintainers/development.md`](../../docs/maintainers/development.md)
