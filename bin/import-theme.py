#!/usr/bin/env python3
"""Import a Base16 theme from the tinted-theming/schemes repository.

Usage:
    python3 bin/import-theme.py <slug>
    python3 bin/import-theme.py <slug> --list

Examples:
    python3 bin/import-theme.py rose-pine
    python3 bin/import-theme.py everforest-dark-hard
    python3 bin/import-theme.py --list          # list available themes
"""

import os
import sys
import urllib.request
import urllib.error

SCHEMES_URL = "https://raw.githubusercontent.com/tinted-theming/schemes/refs/heads/spec-0.11/base16"
THEMES_DIR = os.path.join(os.path.dirname(__file__), "..", "resources", "themes")


def fetch(url: str) -> str:
    """Fetch URL content as string."""
    try:
        with urllib.request.urlopen(url, timeout=15) as resp:
            return resp.read().decode("utf-8")
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return ""
        raise


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    if sys.argv[1] == "--list":
        print("Fetching theme list from tinted-theming/schemes...")
        # The GitHub API for directory listing
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
    output_path = os.path.join(THEMES_DIR, f"{slug}.yaml")

    if os.path.exists(output_path):
        print(f"Theme already exists: {output_path}")
        print("Delete it first if you want to re-import.")
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

    # Strip '#' from hex colors for consistency with our format
    lines = []
    for line in content.splitlines():
        # Match palette color lines like '  base00: "#rrggbb"'
        stripped = line.strip()
        if stripped.startswith("base0") and ':"' in stripped.replace(" ", ""):
            line = line.replace('"#', '"').replace("'#", "'")
            # Also strip inline comments for cleanliness
            if " #" in line:
                line = line[: line.index(" #")]
        lines.append(line)

    content = "\n".join(lines) + "\n"

    os.makedirs(THEMES_DIR, exist_ok=True)
    with open(output_path, "w") as f:
        f.write(content)

    print(f"Saved to {output_path}")
    print(f"Run 'just rebuild' to compile with the new theme.")


if __name__ == "__main__":
    main()
