#!/usr/bin/env python3
"""Verify translation .ts files are in sync with the source tree.

Pre-commit hook: when C++ / QML / translation-tool files change, check
whether `just translations-update` would modify any .ts file. If so,
fail with a clear message pointing at the fix.

Approach: copy the committed .ts files into a temp tree, run lupdate
against the copies (scanning the live src/ and resources/qml/ trees,
same flags as the justfile recipe), normalize the copies, then diff
against the committed originals. Any diff means the live tree has
drifted from what lupdate would produce.

The normalization reproduces write_ts() from bin/translations/translate.py
so that the diff reflects real drift, not formatting quirks.
"""

import difflib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
LANGS_DIR = os.path.join(REPO_ROOT, "resources", "langs")
SRC_DIR = os.path.join(REPO_ROOT, "src")
QML_DIR = os.path.join(REPO_ROOT, "resources", "qml")
LUPDATE = "/usr/lib/qt6/bin/lupdate"


def find_ts_files(langs_root: str) -> list[str]:
    """Return all komai_<lang>.ts files under a langs root, sorted."""
    out = []
    for lang in sorted(os.listdir(langs_root)):
        lang_dir = os.path.join(langs_root, lang)
        ts_path = os.path.join(lang_dir, f"komai_{lang}.ts")
        if os.path.isfile(ts_path):
            out.append(ts_path)
    return out


def normalize_in_place(ts_path: str) -> None:
    """Reproduce write_ts() from translate.py on an existing file."""
    with open(ts_path, "r", encoding="utf-8") as f:
        original = f.read()

    header_lines = []
    for line in original.split("\n"):
        stripped = line.strip()
        if stripped.startswith("<?xml") or stripped.startswith("<!DOCTYPE"):
            header_lines.append(line)
        else:
            break

    tree = ET.parse(ts_path)
    body = ET.tostring(tree.getroot(), encoding="unicode", xml_declaration=False)
    body = re.sub(r" />", "/>", body)

    with open(ts_path, "w", encoding="utf-8") as f:
        if header_lines:
            f.write("\n".join(header_lines) + "\n")
        f.write(body)
        f.write("\n")


def main() -> int:
    if not os.path.isfile(LUPDATE):
        print(f"ERROR: {LUPDATE} not found — install Qt6 linguist tools.", file=sys.stderr)
        return 2

    live_ts = find_ts_files(LANGS_DIR)
    if not live_ts:
        print("No .ts files found under resources/langs/; nothing to check.")
        return 0

    with tempfile.TemporaryDirectory(prefix="komai-translations-drift-") as tmp:
        # Mirror the repo layout inside tmp using symlinks for src/ and
        # resources/qml/, and real copies of the .ts files. This keeps
        # lupdate's -locations relative output matching the committed
        # form (../../qml/... and ../../../src/...) because the ts file
        # sits at the same depth relative to those dirs as in the repo.
        os.symlink(SRC_DIR, os.path.join(tmp, "src"))
        tmp_resources = os.path.join(tmp, "resources")
        os.makedirs(tmp_resources)
        os.symlink(QML_DIR, os.path.join(tmp_resources, "qml"))
        tmp_langs = os.path.join(tmp_resources, "langs")
        os.makedirs(tmp_langs)
        tmp_ts = []
        for live_path in live_ts:
            lang = os.path.basename(os.path.dirname(live_path))
            dest_dir = os.path.join(tmp_langs, lang)
            os.makedirs(dest_dir)
            dest = os.path.join(dest_dir, os.path.basename(live_path))
            shutil.copy2(live_path, dest)
            tmp_ts.append(dest)

        cmd = [
            LUPDATE,
            "-locations", "relative",
            "-no-obsolete",
            os.path.join(tmp, "src"),
            os.path.join(tmp_resources, "qml"),
            "-ts", *tmp_ts,
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print("ERROR: lupdate failed:", file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            return 2

        for tmp_path in tmp_ts:
            normalize_in_place(tmp_path)

        drifted: list[tuple[str, str]] = []
        for live_path, tmp_path in zip(live_ts, tmp_ts):
            with open(live_path, "r", encoding="utf-8") as f:
                live_content = f.read()
            with open(tmp_path, "r", encoding="utf-8") as f:
                tmp_content = f.read()
            if live_content != tmp_content:
                lang = os.path.basename(os.path.dirname(live_path))
                diff = "".join(
                    difflib.unified_diff(
                        live_content.splitlines(keepends=True),
                        tmp_content.splitlines(keepends=True),
                        fromfile=f"live/{lang}",
                        tofile=f"regenerated/{lang}",
                        n=2,
                    )
                )
                drifted.append((lang, diff))

        if not drifted:
            print(f"All {len(live_ts)} translation file(s) are in sync with source.")
            return 0

        print("ERROR: Translation files are out of date.", file=sys.stderr)
        print("", file=sys.stderr)
        print("The following languages would change under `just translations-update`:", file=sys.stderr)
        for lang, _ in drifted:
            print(f"  {lang}", file=sys.stderr)
        print("", file=sys.stderr)
        print("Fix with:", file=sys.stderr)
        print("  just translations-update                # regenerate .ts files", file=sys.stderr)
        print("  just translations-claude-translate-all  # AI-fill new `unfinished` entries", file=sys.stderr)
        print("then re-stage resources/langs/. Land translations in the same commit", file=sys.stderr)
        print("(or PR) as the source change that introduced the new string.", file=sys.stderr)
        print("See docs/maintainers/translations.md for details.", file=sys.stderr)

        # Show the first drift diff (truncated) so the failure is actionable
        # without needing to re-run manually.
        first_lang, first_diff = drifted[0]
        diff_lines = first_diff.splitlines(keepends=True)
        max_lines = 40
        print("", file=sys.stderr)
        print(f"--- First drift ({first_lang}, up to {max_lines} lines):", file=sys.stderr)
        sys.stderr.writelines(diff_lines[:max_lines])
        if len(diff_lines) > max_lines:
            print(
                f"  ... ({len(diff_lines) - max_lines} more diff lines omitted)",
                file=sys.stderr,
            )
        return 1


if __name__ == "__main__":
    sys.exit(main())
