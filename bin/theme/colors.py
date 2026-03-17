"""Shared color utilities for theme scripts.

Contains YAML parsing, color utilities, and the canonical list of palette
keys. Used by check.py, contrast.py, and generate.py.
"""

import colorsys
import re
import sys

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

CUSTOM_KEYS = ["attention", "success", "warning", "error"]

ALL_PALETTE_KEYS = PALETTE_KEYS + CUSTOM_KEYS

# -- Minimal YAML parser -------------------------------------------------------

def parse_yaml(path: str) -> dict:
    """Minimal YAML parser for theme files.

    Handles flat structure: top-level scalars and one level of nested mapping
    (palette:, source_base16:, userColors:). Also handles YAML lists under
    nested sections (e.g. userColors.others). No external dependency.
    """
    result = {}
    current_section = None
    current_subsection = None  # for list items under a nested key

    with open(path) as f:
        for line in f:
            line = line.rstrip()
            if not line or line.startswith("#"):
                continue

            # List item (4-space indent + "- ")
            if line.startswith("    - "):
                if current_section is not None and current_subsection is not None:
                    m = re.match(r'    - "([^"]*)"', line) or \
                        re.match(r'    - ([^"#\s]*)\s*(?:#.*)?$', line)
                    if m:
                        val = m.group(1).strip()
                        if val:
                            result[current_section][current_subsection].append(val)
                continue

            # Nested key (2-space indent)
            if line.startswith("  "):
                current_subsection = None
                if current_section is None:
                    continue
                m = re.match(r'  (\w+):\s*"([^"]*)"', line) or \
                    re.match(r'  (\w+):\s*([^"#\s]*)\s*(?:#.*)?$', line)
                if m:
                    key = m.group(1)
                    value = m.group(2).strip()
                    if value:
                        result[current_section][key] = value
                    else:
                        # Sub-section that will contain list items
                        result[current_section][key] = []
                        current_subsection = key
                continue

            # Top-level key
            m = re.match(r'(\w+):\s*"([^"]*)"', line) or \
                re.match(r'(\w+):\s*([^"#\s]*)\s*(?:#.*)?$', line)
            if m:
                key = m.group(1)
                value = m.group(2).strip()
                current_subsection = None
                if value:
                    result[key] = value
                    current_section = None
                else:
                    # Section header (e.g., "palette:" or "userColors:")
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


def rgb_to_hex(rgb):
    """Convert (r, g, b) ints to a #rrggbb string."""
    return "#{:02x}{:02x}{:02x}".format(*rgb)


def qcolor_darker(hex_str, factor):
    """Approximate QColor::darker(factor) for a hex color string."""
    r, g, b = parse_color(hex_str)
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    v = max(0.0, min(1.0, v * 100.0 / factor))
    rr, gg, bb = colorsys.hsv_to_rgb(h, s, v)
    return rgb_to_hex((round(rr * 255), round(gg * 255), round(bb * 255)))


def qcolor_lighter(hex_str, factor):
    """Approximate QColor::lighter(factor) for a hex color string."""
    r, g, b = parse_color(hex_str)
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    v = max(0.0, min(1.0, v * factor / 100.0))
    rr, gg, bb = colorsys.hsv_to_rgb(h, s, v)
    return rgb_to_hex((round(rr * 255), round(gg * 255), round(bb * 255)))


def derive_readable_accent_text_color(accent_color, background_color, min_contrast=4.5):
    """Adjust an accent toward readability on a background with minimal drift."""
    if contrast_ratio(accent_color, background_color) >= min_contrast:
        return accent_color

    prefer_darker = contrast_ratio("#000000", background_color) >= contrast_ratio(
        "#ffffff", background_color
    )
    best_color = None
    best_distance = sys.maxsize
    best_contrast = 0.0

    def consider(candidate, distance):
        nonlocal best_color, best_distance, best_contrast
        ratio = contrast_ratio(candidate, background_color)
        if ratio < min_contrast:
            return
        if (
            best_color is None
            or distance < best_distance
            or (distance == best_distance and ratio > best_contrast)
        ):
            best_color = candidate
            best_distance = distance
            best_contrast = ratio

    for factor in range(105, 401, 5):
        if prefer_darker:
            consider(qcolor_darker(accent_color, factor), factor - 100)
            consider(qcolor_lighter(accent_color, factor), factor - 100)
        else:
            consider(qcolor_lighter(accent_color, factor), factor - 100)
            consider(qcolor_darker(accent_color, factor), factor - 100)
        if best_color is not None and best_distance == 5:
            break

    if best_color is not None:
        return best_color

    return (
        "#000000"
        if contrast_ratio("#000000", background_color)
        >= contrast_ratio("#ffffff", background_color)
        else "#ffffff"
    )
