# Docs Tooling 📝

Scripts in this directory help keep documentation quality high and reviewable.

## Files

- `check-links.py` - validates local Markdown links resolve to existing files/directories.

## Usage

```sh
just docs-check-links
```

## Notes

- Checks all repository `*.md` files.
- Skips external links (`https://`, `mailto:`, anchors like `#...`).
- Fails when a local target path does not exist.

Related docs:

- [`docs/maintainers/development.md`](../../docs/maintainers/development.md)
- [`docs/README.md`](../../docs/README.md)
