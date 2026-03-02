"""Shared color utilities for theme scripts.

Contains YAML parsing, color utilities, and the canonical list of palette
keys. Used by check.py and generate.py.
"""

import re

# -- Canonical palette key lists -----------------------------------------------

PALETTE_KEYS = [
    "window",
    "windowText",
    "base",
    "alternateBase",
    "text",
    "brightText",
    "button",
    "buttonText",
    "light",
    "mid",
    "dark",
    "highlight",
    "highlightedText",
    "link",
    "toolTipBase",
    "toolTipText",
]

CUSTOM_KEYS = ["red", "green", "orange", "error"]

ALL_PALETTE_KEYS = PALETTE_KEYS + CUSTOM_KEYS

# -- Minimal YAML parser -------------------------------------------------------

def parse_yaml(path: str) -> dict:
    """Minimal YAML parser for theme files.

    Handles flat structure: top-level scalars and one level of nested mapping
    (palette:, source_base16:, overrides:). No external dependency.
    """
    result = {}
    current_section = None

    with open(path) as f:
        for line in f:
            line = line.rstrip()
            if not line or line.startswith("#"):
                continue

            # Nested key (2-space indent)
            if line.startswith("  "):
                if current_section is None:
                    continue
                m = re.match(r'  (\w+):\s*"?([^"#]*)"?\s*(?:#.*)?$', line)
                if m:
                    result[current_section][m.group(1)] = m.group(2).strip().strip('"')
                continue

            # Top-level key
            m = re.match(r'(\w+):\s*"?([^"#]*)"?\s*(?:#.*)?$', line)
            if m:
                key = m.group(1)
                value = m.group(2).strip().strip('"')
                if value:
                    result[key] = value
                    current_section = None
                else:
                    # Section header (e.g., "palette:" or "source_base16:")
                    result[key] = {}
                    current_section = key

    return result


# -- Color utilities -----------------------------------------------------------

def parse_color(hex_str: str) -> tuple:
    """Parse a hex color string (with or without #) to (r, g, b) ints."""
    h = hex_str.lstrip("#").strip()
    if len(h) != 6:
        raise ValueError(f"Invalid hex color: {hex_str!r}")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))


def hex_to_qcolor(hex_str: str) -> str:
    """Convert hex string to QColor constructor."""
    r, g, b = parse_color(hex_str)
    return f"QColor(0x{r:02x}, 0x{g:02x}, 0x{b:02x})"


def _linearize(v):
    """Linearize an sRGB component (0-255) for luminance calculation."""
    v = v / 255.0
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4


def luminance(hex_str):
    """Relative luminance per WCAG 2.0."""
    r, g, b = parse_color(hex_str)
    return 0.2126 * _linearize(r) + 0.7152 * _linearize(g) + 0.0722 * _linearize(b)


def contrast_ratio(hex1, hex2):
    """WCAG 2.0 contrast ratio (always >= 1.0)."""
    l1, l2 = luminance(hex1), luminance(hex2)
    lighter, darker = max(l1, l2), min(l1, l2)
    return (lighter + 0.05) / (darker + 0.05)
