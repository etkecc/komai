#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Fetch and unpack the pinned @element-hq/element-call-embedded bundle.

Reads the pin from bin/element-call/sources.lock.yml, downloads the npm
tarball into ``<out-dir>/<version>/``, verifies its sha256, and extracts the
bundle's ``package/dist/`` web root to ``<out-dir>/<version>/dist/``. Prints the
absolute path of that dist directory on stdout (last line) so CMake can embed
it via qt_add_resources.

The step is idempotent and offline-friendly: if the dist directory already
exists it does nothing and just prints the path. Offline builds (Flatpak/Snap)
pre-populate ``<out-dir>/<version>/`` on the host and copy it into the sandbox.

Stdlib only -- no PyYAML, mirroring bin/emoji/pipeline.py.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import pathlib
import re
import sys
import tarfile
import tempfile
import urllib.error
import urllib.request

# The web root we embed lives under this prefix inside the npm tarball.
_TARBALL_DIST_PREFIX = "package/dist/"


def _log(msg: str) -> None:
    print(f"[element-call/fetch] {msg}", file=sys.stderr)


def _parse_lock(lock_path: pathlib.Path) -> tuple[str, str, str]:
    """Extract (version, url, sha256) from the simple sources.lock.yml."""
    text = lock_path.read_text(encoding="utf-8")

    def field(name: str) -> str:
        m = re.search(rf'^\s*{name}:\s*"([^"]+)"\s*$', text, re.MULTILINE)
        if not m:
            raise SystemExit(f"error: '{name}' not found in {lock_path}")
        return m.group(1)

    version = field("version")
    url = field("url").replace("{version}", version)
    sha256 = field("sha256")
    return version, url, sha256


def _sha256(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _download(url: str, dest: pathlib.Path) -> None:
    _log(f"downloading {url}")
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(dest.suffix + ".part")
    try:
        with urllib.request.urlopen(url) as resp, tmp.open("wb") as out:
            while True:
                chunk = resp.read(1 << 20)
                if not chunk:
                    break
                out.write(chunk)
    except urllib.error.URLError as exc:
        tmp.unlink(missing_ok=True)
        raise SystemExit(
            f"error: failed to download {url}: {exc}\n"
            "       (offline build? pre-populate the bundle on the host and copy "
            "var/element-call/<version>/ into the build sandbox.)"
        )
    tmp.replace(dest)


def _safe_extract_dist(tarball: pathlib.Path, dist_dir: pathlib.Path) -> None:
    """Extract only members under package/dist/, stripping that prefix."""
    dist_dir.mkdir(parents=True, exist_ok=True)
    dist_root = dist_dir.resolve()
    with tarfile.open(tarball, "r:gz") as tar:
        for member in tar.getmembers():
            if not member.isfile():
                continue
            if not member.name.startswith(_TARBALL_DIST_PREFIX):
                continue
            rel = member.name[len(_TARBALL_DIST_PREFIX):]
            target = (dist_dir / rel).resolve()
            # Guard against path traversal via crafted member names.
            if not str(target).startswith(str(dist_root) + os.sep):
                raise SystemExit(f"error: unsafe path in tarball: {member.name}")
            target.parent.mkdir(parents=True, exist_ok=True)
            src = tar.extractfile(member)
            if src is None:
                continue
            with src, target.open("wb") as out:
                out.write(src.read())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lock", required=True, type=pathlib.Path)
    parser.add_argument(
        "--out-dir",
        required=True,
        type=pathlib.Path,
        help="base dir (e.g. var/element-call); the bundle lands in <out-dir>/<version>/",
    )
    args = parser.parse_args()

    version, url, expected_sha = _parse_lock(args.lock)
    version_dir = args.out_dir / version
    dist_dir = version_dir / "dist"
    index_html = dist_dir / "index.html"

    if index_html.is_file():
        print(dist_dir.resolve())
        return 0

    tarball = version_dir / f"element-call-embedded-{version}.tgz"
    if not tarball.is_file():
        _download(url, tarball)

    actual_sha = _sha256(tarball)
    if actual_sha != expected_sha:
        tarball.unlink(missing_ok=True)
        raise SystemExit(
            f"error: sha256 mismatch for {tarball.name}\n"
            f"       expected {expected_sha}\n"
            f"       actual   {actual_sha}\n"
            "       If you intentionally bumped the version, update sha256 in "
            f"{args.lock} to the actual hash above."
        )

    # Extract into a temp dir then atomically swap, so a half-extracted tree is
    # never mistaken for a complete one on the next run.
    with tempfile.TemporaryDirectory(dir=version_dir) as tmp:
        tmp_dist = pathlib.Path(tmp) / "dist"
        _safe_extract_dist(tarball, tmp_dist)
        if not (tmp_dist / "index.html").is_file():
            raise SystemExit(
                f"error: {_TARBALL_DIST_PREFIX}index.html missing from {tarball.name}"
            )
        if dist_dir.exists():
            import shutil

            shutil.rmtree(dist_dir)
        tmp_dist.replace(dist_dir)

    _log(f"unpacked Element Call {version} -> {dist_dir}")
    print(dist_dir.resolve())
    return 0


if __name__ == "__main__":
    sys.exit(main())
