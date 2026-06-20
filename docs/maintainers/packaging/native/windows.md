# Native build on Windows

Build and run Komai on Windows from source.

> **Status:** verified end-to-end on Windows 10 Pro 22H2 (May 2026)
> with **MSVC 2022 + Qt 6.10**. Build, deploy, login (incl. SSO), and
> main UI all work. VOIP is **not** covered -- see [Limits](#limits).

For shared reference (dependencies, CMake flags, CPM), see
[native.md](../native.md).

## Tested environment

- Windows 10 Pro 22H2, 64-bit
- PowerShell 5.1 (the in-box version)

## Quick install (Chocolatey)

[Chocolatey](https://chocolatey.org/) installs most prerequisites in
two commands. **Run from an Administrator PowerShell**:

```powershell
choco install -y `
    git cmake python ninja rustup.install `
    pkgconfiglite

choco install -y visualstudio2022buildtools `
    --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

Visual Studio Build Tools is the long pole; the rest are quick.

Three things still need to happen by hand:

1. [Allow PowerShell to run local scripts](#3-allow-powershell-to-run-local-scripts).
2. [Open an x64 Developer PowerShell](#4-open-an-x64-developer-powershell).
3. [Install Qt via aqtinstall](#5-install-qt-via-aqtinstall).

Then jump to [Configure](#7-configure-with-cmake) and
[Build](#8-build).

If you don't use Chocolatey, follow the walkthrough in order -- each
step lists a manual-install alternative.

## Get the source

```powershell
git clone https://github.com/etkecc/komai C:\src\komai
cd C:\src\komai
```

Keep the path short -- CPM nests dependencies and the Windows
260-character path limit will bite in deeper locations.

## Walkthrough

### 1. Prerequisites available via package manager

| Tool | Purpose | Chocolatey | Manual install |
|------|---------|------------|----------------|
| Git | Source control | `git` | [git-scm.com](https://git-scm.com/download/win) |
| CMake | Build system | `cmake` | [cmake.org](https://cmake.org/download/) |
| Python 3 | Theme / emoji generation | `python` | [python.org](https://www.python.org/downloads/) |
| Ninja | Faster CMake generator (recommended) | `ninja` | [ninja-build releases](https://github.com/ninja-build/ninja/releases) |
| Rustup | Manages the Rust toolchain | `rustup.install` | [rustup.rs](https://rustup.rs/) |

`just` is **not used on Windows** (recipes shell out to bash and use
Unix-only build-lock primitives) -- invoke `cmake` directly as shown in
[Configure](#7-configure-with-cmake) and [Build](#8-build). `rustup`
only needs to be installed; Corrosion will install the version pinned
in [`rust-toolchain.toml`](../../../../rust-toolchain.toml) on first
configure.

### 2. MSVC Build Tools 2022

**Chocolatey** (Administrator PowerShell):

```powershell
choco install visualstudio2022buildtools -y `
    --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

`VCTools` selects the "Desktop development with C++" workload;
`--includeRecommended` adds the Windows SDK and supporting
components. Without `--includeRecommended` you get a bare compiler
that can't link Win32 apps. Several GB on disk.

**Manual:** download the [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
installer and pick "Desktop development with C++". Full Visual Studio
2022 Community works too.

### 3. Allow PowerShell to run local scripts

The VS Developer PowerShell launcher is a `.ps1` script, blocked by
the default `Restricted` policy:

```powershell
Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
```

`CurrentUser` scope avoids machine-wide changes and doesn't need
admin. `RemoteSigned` is Microsoft's recommended developer baseline:
local scripts run, downloaded scripts must be signed.

### 4. Open an x64 Developer PowerShell

The default "Developer PowerShell for VS 2022" Start-menu shortcut
opens an **x86** environment, which can't link against x64 Qt. Force
x64:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64
```

After running this, `cl` should report "for x64". For repeatable use,
make a desktop shortcut:

- Target:
  ```
  powershell.exe -NoExit -Command "& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64"
  ```
- Start in: `C:\src\komai`

If you installed full Visual Studio 2022 (not Build Tools), replace
`BuildTools` in the path with `Community` (or `Professional` /
`Enterprise`).

### 5. Install Qt via aqtinstall

Chocolatey's Qt packages are stale or only install Qt Creator, so
use [aqtinstall](https://github.com/miurahr/aqtinstall) -- a Python
tool that downloads Qt from Qt's official CDN. No Qt account
required.

```powershell
pip install aqtinstall
```

**Pick a version.** Last verified against **6.10.3**; any **6.10.x**
(or newer 6.x) should work. List what's available:

```powershell
python -m aqt list-qt windows desktop --arch win64_msvc2022_64
```

Output is grouped by minor version; pick the highest patch in the
latest 6.x line (pipe through `Select-String '6.10'` to filter).
Substitute your chosen version for `<QT_VER>` below.

```powershell
python -m aqt install-qt windows desktop <QT_VER> win64_msvc2022_64 -O C:\Qt `
    -m qtmultimedia qtimageformats qtshadertools `
    qtwebengine qtwebchannel qtpositioning
```

`qtwebengine`, `qtwebchannel`, and `qtpositioning` back Element Call
(`ELEMENT_CALL` is ON by default): qtwebchannel provides
`Qt::WebChannelQuick`, qtpositioning is a QtWebEngine dependency. Drop
all three if you build with `-DELEMENT_CALL=OFF`.

**Do not** pass `qtdeclarative`, `qtsvg`, or `qttools` to `-m` -- on
Qt 6.10 they're part of the base install and aqt errors out if listed
as add-ons. To see what modules a specific version offers:
`python -m aqt list-qt windows desktop --modules <QT_VER> win64_msvc2022_64`.
Installs to `C:\Qt\<QT_VER>\msvc2022_64\`.

**Manual:** the [Qt online installer](https://www.qt.io/download-qt-installer)
works but needs a free Qt account and a GUI walkthrough.

### 6. pkg-config

**Chocolatey** (Administrator PowerShell):

```powershell
choco install pkgconfiglite -y
```

`pkgconfiglite` is only required because
[`CMakeLists.txt`](../../../../CMakeLists.txt) calls
`find_package(PkgConfig REQUIRED)` unconditionally; no
`pkg_check_modules` calls fire with `-DVOIP=OFF`, so the install just
satisfies the configure check.

**Manual:** pkg-config has no standalone Windows installer; use
Chocolatey or `vcpkg install pkgconf`.

Open a new shell so PATH picks up pkg-config, then verify:

```powershell
pkg-config --version
```

### 7. Configure with CMake

From the x64 Developer PowerShell, in the repo root:

```powershell
cmake -S . -B var\build\native -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DVOIP=OFF `
    -DBUILD_TESTING=OFF `
    -DCMAKE_PREFIX_PATH=C:\Qt\<QT_VER>\msvc2022_64
```

- `-G Ninja` -- faster than MSBuild, cleaner output. Optional; drop
  it for the Visual Studio generator (`-G "Visual Studio 17 2022"`).
- `-DVOIP=OFF` -- skip GStreamer; see [Limits](#limits).
- `-DBUILD_TESTING=OFF` -- skip tests; the hardened test env uses
  Linux-only `/proc` paths.
- `X11` is auto-disabled on Windows; no flag needed.

The first configure downloads CPM dependencies (qt6keychain,
kdsingleapplication, WinToast, Corrosion, litehtml) and triggers
`rustup toolchain install` for the pin in
[`rust-toolchain.toml`](../../../../rust-toolchain.toml).

### 8. Build

From the x64 Developer PowerShell:

```powershell
cmake --build var\build\native --parallel
```

The first build is slow because Corrosion compiles matrix-sdk and
its dependencies from scratch. Subsequent builds are fast: cargo's
incremental cache under `var\build\native\cargo\` is preserved across
rebuilds. Wiping `var\build\native\` invalidates the cache and the
next build pays the full Rust cost again.

### 9. Deploy Qt DLLs and plugins with `windeployqt`

`komai.exe` isn't runnable as-is: Qt DLLs, platform plugins, and QML
modules need to sit alongside it. `windeployqt` ships with Qt and
copies them all in:

```powershell
C:\Qt\<QT_VER>\msvc2022_64\bin\windeployqt.exe --qmldir resources\qml --release var\build\native\komai.exe
```

It inspects the binary for Qt module deps, scans `resources\qml\`
for QML imports, and copies the matching DLLs / plugins / `.qml`
files into `var\build\native\`. `--release` skips debug-variant DLLs
(without it you get both `Qt6Core.dll` and `Qt6Cored.dll` and the
output doubles in size). Adds a few hundred MB.

### 10. Run

```powershell
.\var\build\native\komai.exe
```

The login screen should appear shortly. SSO login flow, main UI, room
list, and message rendering all work.

## Limits

What this build doesn't cover:

- **VOIP.** `-DVOIP=OFF`. A working VOIP build would need a
  GStreamer 1.20+ MSVC build (e.g. from
  [gstreamer.freedesktop.org](https://gstreamer.freedesktop.org/download/#windows))
  with `gst-plugins-base`, `-good`, `-bad`, `-nice`, and a Qt6 GL
  video sink -- untried.
- **C++ tests.** `-DBUILD_TESTING=OFF`. The hardened test env uses
  Linux-only `/proc/...` paths.
- **Distribution.** The `var\build\native\` tree is runnable in place.
  Installers (MSI / NSIS), code-signing, and Microsoft Store
  publishing are separate efforts.

## Troubleshooting

**`komai.exe` exits silently with no window.** Check `$LASTEXITCODE`.
A value of `-1073741515` (`0xC0000135`, `STATUS_DLL_NOT_FOUND`) means
a Qt DLL is missing -- re-run `windeployqt` (step 9).

**A window appears but stays blank/white.** That's a QML import error.
Run from `cmd.exe` with logging enabled to see what's failing:

```cmd
set KOMAI_LOG_LEVEL=debug
set KOMAI_LOG_TYPE=stderr
var\build\native\komai.exe > komai.log 2>&1
type komai.log
```

The log will show every QML import the engine tries to resolve; a
"module ... is not installed" line points at the missing piece.
