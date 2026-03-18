# Windows Packaging

Windows packaging is currently inactive.

These files are kept as dormant templates from the earlier Windows packaging
flow so the work can be revived later without recovering them from history.

When Windows packaging returns:

- treat this directory as packaging input, not runtime resources
- stage generated logo assets from `var/build/<build-name>/logo-assets/`
  next to `AppxManifest.xml`
- use `komai.png`, `komai-44.png`, and `komai-150.png` from that generated
  asset directory for the AppX/MSIX package
- use the generated `komai.ico` for executable metadata on Windows builds

`resources/` is intentionally reserved for runtime/source assets now, with
`resources/komai.svg` as the only checked-in logo source of truth.
