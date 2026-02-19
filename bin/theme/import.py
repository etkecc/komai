#!/usr/bin/env python3
"""Import a theme from the tinted-theming/schemes repository.

Fetches a Base16 YAML, applies the QPalette mapping at import time, and writes
the resolved theme YAML. The resulting file contains QPalette-level colors
directly — no Base16 mapping happens at build time.

Usage:
    python3 bin/theme/import.py <slug>
    python3 bin/theme/import.py <slug> --force
    python3 bin/theme/import.py --list

Examples:
    python3 bin/theme/import.py rose-pine
    python3 bin/theme/import.py everforest-dark-hard
    python3 bin/theme/import.py --list          # list available themes
"""

import os
import re
import sys
import urllib.request
import urllib.error

from colors import (
    base16_to_palette,
    base16_to_custom,
    strip_variant_suffix,
    write_theme_yaml,
)

SCHEMES_URL = "https://raw.githubusercontent.com/tinted-theming/schemes/refs/heads/spec-0.11/base16"
THEMES_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "resources", "themes")

HEX_RE = re.compile(r"^[0-9a-fA-F]{6}$")


def fetch(url: str) -> str:
    """Fetch URL content as string."""
    try:
        with urllib.request.urlopen(url, timeout=15) as resp:
            return resp.read().decode("utf-8")
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return ""
        raise


def parse_base16_yaml(content: str) -> dict:
    """Parse a raw Base16 YAML string into structured fields."""
    result = {"palette": {}}
    current_section = None

    for line in content.splitlines():
        line = line.rstrip()
        if not line or line.startswith("#"):
            continue

        # Nested key (2-space indent)
        if line.startswith("  "):
            if current_section is None:
                continue
            m = re.match(r'  (\w+):\s*"?#?([^"#]*)"?\s*(?:#.*)?$', line)
            if m:
                value = m.group(2).strip().strip('"')
                result[current_section][m.group(1)] = value
            continue

        # Top-level key
        m = re.match(r'(\w+):\s*"?#?([^"#]*)"?\s*(?:#.*)?$', line)
        if m:
            key = m.group(1)
            value = m.group(2).strip().strip('"')
            if value:
                result[key] = value
                current_section = None
            else:
                result[key] = {}
                current_section = key

    return result


def detect_variant(palette: dict) -> str:
    """Guess light/dark variant from base00 luminance."""
    h = palette.get("base00", "000000")
    r, g, b = int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)
    luma = 0.299 * r + 0.587 * g + 0.114 * b
    return "light" if luma > 128 else "dark"


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    if sys.argv[1] == "--list":
        print("Fetching theme list from tinted-theming/schemes...")
        import json

        url = "https://api.github.com/repos/tinted-theming/schemes/contents/base16?ref=spec-0.11"
        try:
            req = urllib.request.Request(
                url, headers={"Accept": "application/vnd.github.v3+json"}
            )
            with urllib.request.urlopen(req, timeout=15) as resp:
                entries = json.loads(resp.read())
            yamls = [
                e["name"].removesuffix(".yaml")
                for e in entries
                if e["name"].endswith(".yaml")
            ]
            yamls.sort()
            print(f"\nAvailable themes ({len(yamls)}):\n")
            for y in yamls:
                print(f"  {y}")
        except Exception as e:
            print(f"Error listing themes: {e}", file=sys.stderr)
            sys.exit(1)
        return

    slug = sys.argv[1]
    force = "--force" in sys.argv
    output_path = os.path.join(THEMES_DIR, f"{slug}.yml")

    if os.path.exists(output_path) and not force:
        print(f"Theme already exists: {output_path}")
        print("Use --force to overwrite.")
        sys.exit(1)

    url = f"{SCHEMES_URL}/{slug}.yaml"
    print(f"Downloading {url}...")

    content = fetch(url)
    if not content:
        print(
            f"ERROR: Theme '{slug}' not found in tinted-theming/schemes",
            file=sys.stderr,
        )
        print(f"Use --list to see available themes.", file=sys.stderr)
        sys.exit(1)

    # Parse the raw Base16 YAML
    data = parse_base16_yaml(content)
    raw_palette = data.get("palette", {})

    # Validate all 16 base16 slots
    for i in range(16):
        slot = f"base{i:02X}"
        if slot not in raw_palette:
            print(f"ERROR: Downloaded theme missing {slot}", file=sys.stderr)
            sys.exit(1)
        if not HEX_RE.match(raw_palette[slot]):
            print(f"ERROR: Invalid hex for {slot}: {raw_palette[slot]!r}", file=sys.stderr)
            sys.exit(1)

    name = strip_variant_suffix(data.get("name", slug.replace("-", " ").title()))
    author = data.get("author", "")
    variant = detect_variant(raw_palette)

    # Apply Base16 → QPalette mapping at import time
    mapped = base16_to_palette(raw_palette, variant)
    custom = base16_to_custom(raw_palette)
    final_palette = {**mapped, **custom}

    # Preserve original base16 palette as provenance
    source_base16 = raw_palette

    os.makedirs(THEMES_DIR, exist_ok=True)
    write_theme_yaml(output_path, name, author, variant, final_palette, source_base16)

    print(f"Saved to {output_path}")
    print(f"Run 'just rebuild' to compile with the new theme.")


if __name__ == "__main__":
    main()
