"""Shared color utilities for theme scripts.

Contains the Base16-to-QPalette mapping, contrast heuristics, YAML parsing,
and the canonical list of palette keys. Used by import.py (at import time)
and generate.py (for validation only).
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

# Themes whose display name legitimately ends with "Dark" or "Light".
# Everyone else gets the suffix stripped (the variant field carries that info).
VARIANT_SUFFIX_EXCEPTIONS: set[str] = set()


def strip_variant_suffix(name: str) -> str:
    """Strip trailing ' dark'/'' light' (case-insensitive) from a theme name."""
    if name in VARIANT_SUFFIX_EXCEPTIONS:
        return name
    lower = name.lower()
    for suffix in (" dark", " light"):
        if lower.endswith(suffix):
            return name[: -len(suffix)]
    return name


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


def _delinearize(v):
    """Convert linear-light component back to sRGB (0-255)."""
    v = max(0.0, min(1.0, v))
    if v <= 0.0031308:
        s = v * 12.92
    else:
        s = 1.055 * v ** (1 / 2.4) - 0.055
    return max(0, min(255, round(s * 255)))


def luminance(hex_str):
    """Relative luminance per WCAG 2.0."""
    r, g, b = parse_color(hex_str)
    return 0.2126 * _linearize(r) + 0.7152 * _linearize(g) + 0.0722 * _linearize(b)


def contrast_ratio(hex1, hex2):
    """WCAG 2.0 contrast ratio (always >= 1.0)."""
    l1, l2 = luminance(hex1), luminance(hex2)
    lighter, darker = max(l1, l2), min(l1, l2)
    return (lighter + 0.05) / (darker + 0.05)


def blend_toward(hex_str, target, t):
    """Blend hex_str toward target by factor t (0.0=unchanged, 1.0=target) in linear space."""
    r1, g1, b1 = parse_color(hex_str)
    r2, g2, b2 = parse_color(target)
    lr = _linearize(r1) + (_linearize(r2) - _linearize(r1)) * t
    lg = _linearize(g1) + (_linearize(g2) - _linearize(g1)) * t
    lb = _linearize(b1) + (_linearize(b2) - _linearize(b1)) * t
    return f"{_delinearize(lr):02x}{_delinearize(lg):02x}{_delinearize(lb):02x}"


def best_fg_candidate(bg_hex, candidates):
    """Pick the candidate color with highest contrast against bg_hex."""
    best, best_ratio = None, 0.0
    for c in candidates:
        if c is None:
            continue
        r = contrast_ratio(c, bg_hex)
        if r > best_ratio:
            best, best_ratio = c, r
    return best, best_ratio


def adjust_bg_for_contrast(bg_hex, fg_hex, target):
    """Darken or lighten bg minimally until fg achieves target contrast ratio.

    Blends toward black (if fg is lighter) or white (if fg is darker) using
    binary search to find the minimal adjustment.
    """
    if contrast_ratio(fg_hex, bg_hex) >= target:
        return bg_hex

    toward = "000000" if luminance(fg_hex) > luminance(bg_hex) else "ffffff"

    lo, hi = 0.0, 1.0
    for _ in range(30):
        mid = (lo + hi) / 2
        adjusted = blend_toward(bg_hex, toward, mid)
        if contrast_ratio(fg_hex, adjusted) >= target:
            hi = mid  # less adjustment needed
        else:
            lo = mid  # more adjustment needed

    return blend_toward(bg_hex, toward, hi)


def _ensure_contrast(mapping, palette, variant):
    """Fix poor contrast in the automatic Base16 mapping.

    Checks key foreground/background pairs and adjusts colors to ensure
    adequate visibility. Called before overrides so manual tuning wins.
    """
    MIN_TEXT_ON_ACCENT = 3.0
    MIN_HOVER_DISTINCT = 1.5

    if variant == "dark":
        text_candidates = [
            palette.get("base07"), palette.get("base06"),
            palette.get("base05"), "ffffff",
        ]
    else:
        text_candidates = [
            palette.get("base00"), palette.get("base01"),
            palette.get("base02"), "000000",
        ]

    # highlightedText on highlight (selected items)
    if contrast_ratio(mapping["highlightedText"], mapping["highlight"]) < MIN_TEXT_ON_ACCENT:
        best_ht, best_ratio = best_fg_candidate(mapping["highlight"], text_candidates)
        if best_ht:
            mapping["highlightedText"] = best_ht
        if best_ratio < MIN_TEXT_ON_ACCENT:
            mapping["highlight"] = adjust_bg_for_contrast(
                mapping["highlight"], mapping["highlightedText"], MIN_TEXT_ON_ACCENT
            )
            mapping["link"] = mapping["highlight"]

    # brightText on dark (hover states)
    if contrast_ratio(mapping["brightText"], mapping["dark"]) < MIN_TEXT_ON_ACCENT:
        best_bt, _ = best_fg_candidate(mapping["dark"], text_candidates)
        if best_bt:
            mapping["brightText"] = best_bt

    # Hover background (dark) must be visually distinct from both window and
    # button, and buttonText should remain readable on it.  Derive by blending
    # button in the variant-appropriate direction (toward black for light
    # themes, toward white for dark themes), capped to avoid overly dramatic
    # hover effects.
    MIN_TEXT_ON_HOVER = 2.5
    MAX_HOVER_CONTRAST = 3.0
    toward = "000000" if variant == "light" else "ffffff"

    def _hover_distinct(candidate):
        return (
            contrast_ratio(candidate, mapping["window"]) >= MIN_HOVER_DISTINCT
            and contrast_ratio(candidate, mapping["button"]) >= MIN_HOVER_DISTINCT
        )

    if not _hover_distinct(mapping["dark"]):
        # Try base palette slots first
        for slot in ("base02", "base03"):
            c = palette.get(slot)
            if c and _hover_distinct(c):
                mapping["dark"] = c
                break
        else:
            # Find minimal blend from button for hover distinction
            lo, hi = 0.0, 1.0
            for _ in range(30):
                mid = (lo + hi) / 2
                candidate = blend_toward(mapping["button"], toward, mid)
                if _hover_distinct(candidate):
                    hi = mid
                else:
                    lo = mid
            mapping["dark"] = blend_toward(mapping["button"], toward, hi)

    # If buttonText is hard to read on hover, push dark further but cap
    # at MAX_HOVER_CONTRAST to avoid overly dramatic jumps
    if contrast_ratio(mapping["buttonText"], mapping["dark"]) < MIN_TEXT_ON_HOVER:
        # Find the allowed range: [current, max_blend] where max gives MAX_HOVER
        lo, hi = 0.0, 1.0
        for _ in range(30):
            mid = (lo + hi) / 2
            candidate = blend_toward(mapping["button"], toward, mid)
            if contrast_ratio(candidate, mapping["window"]) <= MAX_HOVER_CONTRAST:
                lo = mid
            else:
                hi = mid
        max_blend = lo

        # Scan the allowed range for best buttonText readability
        best_dark = mapping["dark"]
        best_cr = contrast_ratio(mapping["buttonText"], mapping["dark"])
        for i in range(51):
            t = max_blend * i / 50
            candidate = blend_toward(mapping["button"], toward, t)
            if not _hover_distinct(candidate):
                continue
            cr = contrast_ratio(mapping["buttonText"], candidate)
            if cr > best_cr:
                best_cr = cr
                best_dark = candidate
        mapping["dark"] = best_dark


# -- Base16 → QPalette mapping -------------------------------------------------

def base16_to_palette(palette: dict, variant: str) -> dict:
    """Map Base16 slots to QPalette role colors.

    Returns a dict with all 16 QPalette role keys.
    """
    mapping = {
        "window": palette["base00"],
        "windowText": palette["base05"],
        "base": palette.get("base01", palette["base00"]),
        "alternateBase": palette.get(
            "base02", palette.get("base01", palette["base00"])
        ),
        "text": palette["base05"],
        "brightText": palette.get("base07", palette.get("base06", palette["base05"])),
        "button": palette.get("base01", palette["base00"]),
        "buttonText": palette.get("base04", palette.get("base03", palette["base05"])),
        "light": palette.get("base06", palette.get("base05", "ffffff")),
        "mid": palette.get("base03", palette.get("base02", palette["base01"])),
        "dark": palette.get("base01", palette["base00"]),
        "highlight": palette.get("base0D", "38a3d8"),
        "highlightedText": palette.get("base07", "ffffff")
        if variant == "dark"
        else palette.get("base00", "ffffff"),
        "link": palette.get("base0D", "38a3d8"),
        "toolTipBase": palette.get("base01", palette["base00"]),
        "toolTipText": palette["base05"],
    }

    # Fix contrast issues in automatic mapping
    _ensure_contrast(mapping, palette, variant)

    return mapping


def base16_to_custom(palette: dict) -> dict:
    """Map Base16 slots to Theme custom color properties."""
    return {
        "red": palette.get("base08", "a82353"),
        "green": palette.get("base0B", "008000"),
        "orange": palette.get("base09", "fcbe05"),
        "error": palette.get("base08", "dd3d3d"),
    }


# -- YAML writer ---------------------------------------------------------------

def write_theme_yaml(path, name, author, variant, palette, source_base16=None):
    """Write a theme YAML file in the new QPalette-level format."""
    lines = []
    lines.append(f'name: "{name}"')
    if author:
        lines.append(f'author: "{author}"')
    lines.append(f'variant: "{variant}"')
    lines.append("palette:")
    for key in ALL_PALETTE_KEYS:
        if key in palette:
            lines.append(f'  {key}: "{palette[key]}"')

    if source_base16:
        lines.append("source_base16:")
        for i in range(16):
            slot = f"base{i:02X}"
            if slot in source_base16:
                lines.append(f'  {slot}: "{source_base16[slot]}"')

    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
