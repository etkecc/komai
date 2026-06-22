# Arch Linux packaging (prebuilt)

PKGBUILD for installing Komai from the prebuilt AppImage published with each
GitHub release. Intended for submission to the [AUR](https://aur.archlinux.org/)
as `komai-bin`, alongside the from-source [`komai`](../komai/) package.

It exists so users on slow machines can install and update Komai without the
multi-minute source compile. The two packages `conflict` with each other and
both `provide` `komai`, so installing either satisfies a `komai` dependency.

## How it works

`package()` downloads the release AppImage for the building architecture,
extracts it (`--appimage-extract`), and ships the resulting AppDir under
`/opt/komai`. `/usr/bin/komai` symlinks to `/opt/komai/AppRun`, which sets up
the bundled Qt/QtWebEngine/GStreamer environment before exec'ing the inner
binary. The inner binary is not independently runnable (it has a relative ELF
interpreter), so AppRun is always the entry point.

### Why extract instead of run-in-place

Running the `.AppImage` in place FUSE-mounts a squashfs on every launch, which
adds a multi-second decompress tax to each startup and pulls in a `fuse2`
dependency. Extracting once at install time trades disk (the QtWebEngine bundle
is large) for near-instant launches and drops the `fuse2` requirement.

### KOMAI_EXECUTABLE_PATH

`package()` appends `KOMAI_EXECUTABLE_PATH=/usr/bin/komai` to `AppRun.env`.
Komai self-relaunches (the profile switcher and generated per-profile `.desktop`
entries) would otherwise point at the inner binary, which a menu click or fresh
shell cannot execute. `app_paths::executablePathForRelaunch()` honours the
override; it is a harmless no-op on releases that predate that helper.

## Updating

`pkgver` tracks the matching `komai` release and is bumped by
[`bin/release/prepare.py`](../../../../bin/release/prepare.py) alongside the
source package. Before pushing to AUR, download the published AppImages and
replace the `SKIP` checksums with real `sha256sums` (the in-tree copy keeps
`SKIP` for development). The AUR sync workflow mirrors the source package; see
[`../komai/README.md`](../komai/README.md).
