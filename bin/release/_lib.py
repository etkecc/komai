# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Shared helpers for ``bin/release/*`` scripts.

Imported by the per-phase entry points (``validate.py``, ``build.py``,
``publish.py``, ``all.py``). Each script is invoked directly via
``python3 bin/release/<name>.py``; CPython prepends ``bin/release/`` to
``sys.path``, so a plain ``import _lib`` resolves here.
"""

from __future__ import annotations

import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

VERSION_FILE = REPO_ROOT / "VERSION.txt"
CHANGELOG_FILE = REPO_ROOT / "CHANGELOG.md"
APPIMAGE_DIR = REPO_ROOT / "var/build/appimage"
FLATPAK_DIR = REPO_ROOT / "var/build/flatpak"
SNAP_DIR = REPO_ROOT / "var/build/snap"
WINDOWS_DIR = REPO_ROOT / "var/build/windows"
MACOS_DIR = REPO_ROOT / "var/build/macos"

CALVER_RE = re.compile(r"^\d{4}\.\d{2}\.\d{2}\.\d+$")


def fail(msg: str) -> "None":
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def info(msg: str) -> None:
    print(msg, file=sys.stderr)


def check_tools(*tools: str) -> None:
    """Verify each tool is on ``PATH``; ``fail()`` if any are missing."""
    missing = [t for t in tools if shutil.which(t) is None]
    if missing:
        fail(
            "missing required tool(s) on PATH: "
            + ", ".join(missing)
            + " — install and retry."
        )


def read_version() -> str:
    """Return the validated CalVer string from ``VERSION.txt``."""
    if not VERSION_FILE.is_file():
        fail(f"VERSION.txt not found at {VERSION_FILE}")
    text = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not CALVER_RE.match(text):
        fail(f"VERSION.txt content {text!r} is not valid CalVer (YYYY.MM.DD.N)")
    return text


def tag_for(version: str) -> str:
    return f"v{version}"


# The Linux artefacts ship for both amd64 and arm64. AppImage/Flatpak name
# the arch the Linux-uname way (x86_64 / aarch64); Snap uses the Debian
# arch (amd64 / arm64). The defaults keep the historical amd64 filenames.
def appimage_path(version: str, arch: str = "x86_64") -> Path:
    return APPIMAGE_DIR / f"komai-{version}-{arch}.AppImage"


def flatpak_path(version: str, arch: str = "x86_64") -> Path:
    return FLATPAK_DIR / f"komai-{version}-{arch}.flatpak"


def snap_path(version: str, arch: str = "amd64") -> Path:
    return SNAP_DIR / f"komai_{version}_{arch}.snap"


def windows_zip_path(version: str) -> Path:
    return WINDOWS_DIR / f"komai-{version}-windows-x64-no-installer.zip"


def macos_dmg_path(version: str) -> Path:
    return MACOS_DIR / f"komai-{version}-macos-arm64.dmg"


def expected_artefacts(version: str) -> list[Path]:
    """Every artefact a full (CI) release attaches, across all arches.

    This is the set ``publish.py`` verifies and uploads; the build jobs in
    ``publish.yml`` produce them on their respective runners. Local
    single-machine builds only produce the host-arch Linux subset -- see
    ``host_linux_artefacts``.
    """
    return [
        appimage_path(version, "x86_64"),
        appimage_path(version, "aarch64"),
        flatpak_path(version, "x86_64"),
        flatpak_path(version, "aarch64"),
        snap_path(version, "amd64"),
        snap_path(version, "arm64"),
        windows_zip_path(version),
        macos_dmg_path(version),
    ]


def host_linux_artefacts(version: str) -> list[Path]:
    """The three Linux artefacts ``release-manual-build`` produces locally.

    Only the host architecture is built (the Docker/flatpak recipes target
    the host), so this returns the AppImage/Flatpak/Snap paths for the
    machine's own arch rather than the full cross-arch publish set.
    """
    machine = platform.machine()
    if machine in ("x86_64", "AMD64"):
        return [
            appimage_path(version, "x86_64"),
            flatpak_path(version, "x86_64"),
            snap_path(version, "amd64"),
        ]
    if machine in ("aarch64", "arm64"):
        return [
            appimage_path(version, "aarch64"),
            flatpak_path(version, "aarch64"),
            snap_path(version, "arm64"),
        ]
    fail(f"unsupported host architecture for local release build: {machine!r}")
    return []  # unreachable; fail() exits


def extract_changelog_section(version: str) -> str:
    """Return the body under ``## <version>`` in CHANGELOG.md, or ``""``.

    Matches the same heading pattern the ``version-drift`` hook accepts:
    end-of-line or a single space after the version (so ``## 2026.05.07.0``
    and ``## 2026.05.07.0 - 2026-05-07`` both work).
    """
    text = CHANGELOG_FILE.read_text(encoding="utf-8")
    head_re = re.compile(rf"^## {re.escape(version)}(?:$| .*$)", re.MULTILINE)
    m = head_re.search(text)
    if not m:
        return ""
    start = m.end()
    next_re = re.compile(r"^## ", re.MULTILINE)
    n = next_re.search(text, pos=start)
    end = n.start() if n else len(text)
    return text[start:end].strip("\n")


def run(cmd: list[str], **kwargs: object) -> subprocess.CompletedProcess:
    """Thin ``subprocess.run`` wrapper that defaults ``cwd`` to ``REPO_ROOT``."""
    kwargs.setdefault("cwd", REPO_ROOT)
    return subprocess.run(cmd, **kwargs)
