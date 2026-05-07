"""Shared color utilities for theme scripts.

Contains a tiny YAML parser, color utilities, and helpers for the theme
schema. Used by check.py, contrast.py, and preview support.
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

CUSTOM_KEYS = ["attention", "attentionText", "success", "warning", "error"]

ALL_PALETTE_KEYS = PALETTE_KEYS + CUSTOM_KEYS
USER_COLOR_SLOT_KEYS = ["background", "text", "secondaryText", "link"]
HEX_COLOR_RE = re.compile(r"^#[0-9a-fA-F]{6}$")

# -- Minimal YAML parser -------------------------------------------------------

def parse_yaml(path: str) -> dict:
    """Parse the small subset of YAML used by theme files.

    Supported constructs:
      - mappings
      - lists
      - quoted and unquoted scalars
      - list items that start with an inline mapping pair
    """

    def strip_comments(line: str) -> str:
        in_quotes = False
        escaped = False
        result = []
        for char in line:
            if char == '"' and not escaped:
                in_quotes = not in_quotes
            if char == "#" and not in_quotes:
                break
            result.append(char)
            escaped = char == "\\" and not escaped
        return "".join(result).rstrip()

    def parse_scalar(value: str):
        value = value.strip()
        if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
            return value[1:-1]
        return value

    def split_key_value(text: str):
        in_quotes = False
        escaped = False
        for index, char in enumerate(text):
            if char == '"' and not escaped:
                in_quotes = not in_quotes
            elif char == ":" and not in_quotes:
                return text[:index].strip(), text[index + 1 :].strip()
            escaped = char == "\\" and not escaped
        return None, None

    lines = []
    with open(path, encoding="utf-8") as handle:
        for raw_line in handle:
            stripped = strip_comments(raw_line.rstrip("\n"))
            if not stripped.strip():
                continue
            indent = len(stripped) - len(stripped.lstrip(" "))
            lines.append((indent, stripped.lstrip(" ")))

    def parse_block(index: int, indent: int):
        if index >= len(lines):
            return {}, index
        current_indent, text = lines[index]
        if current_indent != indent:
            raise ValueError(
                f"Unexpected indentation at line {index + 1}: {current_indent}, expected {indent}"
            )
        if text.startswith("- "):
            return parse_sequence(index, indent)
        return parse_mapping(index, indent)

    def parse_mapping(index: int, indent: int):
        mapping = {}
        while index < len(lines):
            current_indent, text = lines[index]
            if current_indent < indent:
                break
            if current_indent > indent:
                raise ValueError(f"Unexpected nested mapping line {index + 1}: {text}")
            if text.startswith("- "):
                raise ValueError(f"Unexpected sequence item line {index + 1}: {text}")

            key, rest = split_key_value(text)
            if key is None:
                raise ValueError(f"Invalid mapping line {index + 1}: {text}")

            index += 1
            if rest:
                mapping[key] = parse_scalar(rest)
                continue

            if index >= len(lines) or lines[index][0] <= indent:
                mapping[key] = {}
                continue

            value, index = parse_block(index, lines[index][0])
            mapping[key] = value

        return mapping, index

    def parse_sequence(index: int, indent: int):
        items = []
        while index < len(lines):
            current_indent, text = lines[index]
            if current_indent < indent:
                break
            if current_indent != indent or not text.startswith("- "):
                break

            rest = text[2:].strip()
            index += 1

            if not rest:
                if index >= len(lines) or lines[index][0] <= indent:
                    items.append(None)
                    continue
                value, index = parse_block(index, lines[index][0])
                items.append(value)
                continue

            key, value = split_key_value(rest)
            if key is not None:
                item = {}
                if value:
                    item[key] = parse_scalar(value)
                elif index >= len(lines) or lines[index][0] <= indent:
                    item[key] = {}
                else:
                    nested_value, index = parse_block(index, lines[index][0])
                    item[key] = nested_value

                while index < len(lines):
                    next_indent, next_text = lines[index]
                    if next_indent <= indent:
                        break
                    if next_indent != indent + 2:
                        raise ValueError(
                            f"Unexpected indentation in sequence item at line {index + 1}: {next_text}"
                        )
                    extra_key, extra_value = split_key_value(next_text)
                    if extra_key is None:
                        raise ValueError(
                            f"Invalid sequence mapping line {index + 1}: {next_text}"
                        )
                    index += 1
                    if extra_value:
                        item[extra_key] = parse_scalar(extra_value)
                    elif index >= len(lines) or lines[index][0] <= next_indent:
                        item[extra_key] = {}
                    else:
                        nested_value, index = parse_block(index, lines[index][0])
                        item[extra_key] = nested_value

                items.append(item)
                continue

            items.append(parse_scalar(rest))

        return items, index

    if not lines:
        return {}

    result, final_index = parse_block(0, lines[0][0])
    if final_index != len(lines):
        raise ValueError("Failed to parse the complete YAML document")
    return result


def normalize_user_color_slot(slot, label: str) -> dict:
    """Validate and normalize a theme userColors slot mapping."""
    if not isinstance(slot, dict):
        raise ValueError(f"{label} must be a mapping")

    normalized = {}
    for key in USER_COLOR_SLOT_KEYS:
        value = slot.get(key, "")
        if value in ("", None):
            continue
        if not isinstance(value, str) or not HEX_COLOR_RE.match(value):
            raise ValueError(f"{label}.{key} must be a #-prefixed hex color")
        normalized[key] = value

    if "background" not in normalized:
        raise ValueError(f"{label}.background is required")

    for key in slot:
        if key not in USER_COLOR_SLOT_KEYS:
            raise ValueError(f"{label}: unexpected key {key!r}")

    return normalized


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


def _f_lab(t):
    delta = 6.0 / 29.0
    return t ** (1.0 / 3.0) if t > delta ** 3 else t / (3 * delta ** 2) + 4.0 / 29.0


def _hex_to_lab(hex_str):
    """Convert a hex color to CIE L*a*b* (D65 reference white)."""
    r, g, b = parse_color(hex_str)
    rl, gl, bl = _linearize(r), _linearize(g), _linearize(b)
    # sRGB (D65) -> XYZ
    x = rl * 0.4124564 + gl * 0.3575761 + bl * 0.1804375
    y = rl * 0.2126729 + gl * 0.7151522 + bl * 0.0721750
    z = rl * 0.0193339 + gl * 0.1191920 + bl * 0.9503041
    # Normalize by D65 reference white
    fx = _f_lab(x / 0.95047)
    fy = _f_lab(y / 1.00000)
    fz = _f_lab(z / 1.08883)
    L = 116.0 * fy - 16.0
    a = 500.0 * (fx - fy)
    b = 200.0 * (fy - fz)
    return L, a, b


def delta_e_lab(hex1, hex2):
    """CIE76 perceptual distance (ΔE*ab) between two sRGB colors.

    Rough interpretation of the result:
      <  1  imperceptible
      1-2  perceptible only on close inspection
      2-5  perceptible at a glance
      > 5  clearly distinguishable surfaces
    """
    L1, a1, b1 = _hex_to_lab(hex1)
    L2, a2, b2 = _hex_to_lab(hex2)
    return ((L1 - L2) ** 2 + (a1 - a2) ** 2 + (b1 - b2) ** 2) ** 0.5


def rgb_to_hex(rgb):
    """Convert (r, g, b) ints to a #rrggbb string."""
    return "#{:02x}{:02x}{:02x}".format(*rgb)


def tint_color(fg_hex, bg_hex, alpha):
    """Alpha-composite fg over bg at the given opacity (0.0-1.0).

    Returns a #rrggbb string representing the resulting opaque color,
    matching QML's Qt.rgba() blending: result = fg * alpha + bg * (1 - alpha).
    """
    fr, fg_, fb = parse_color(fg_hex)
    br, bg_, bb = parse_color(bg_hex)
    return rgb_to_hex((
        round(fr * alpha + br * (1 - alpha)),
        round(fg_ * alpha + bg_ * (1 - alpha)),
        round(fb * alpha + bb * (1 - alpha)),
    ))


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
