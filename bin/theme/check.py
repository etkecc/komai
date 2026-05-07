#!/usr/bin/env python3
"""Validate resources/themes/*.yml files.

Checks:
  - Required fields: name, variant, palette, userColors
  - variant is "light" or "dark"
  - All 21 palette keys (16 QPalette + 5 custom) are present with valid #-prefixed hex
  - userColors.self is a mapping with required background and optional text roles
  - userColors.others is a list of such mappings (minimum 1)
  - source_base16 keys (if present) are valid #-prefixed hex
  - Perceptual distinctness of palette role pairs that are layered as
    surface-on-surface in the UI (see SURFACE_DISTINCTNESS_PAIRS)
"""

import os
import sys

from colors import (
    parse_yaml,
    ALL_PALETTE_KEYS,
    HEX_COLOR_RE,
    delta_e_lab,
    normalize_user_color_slot,
)

# Pairs of palette roles that appear as adjacent UI surfaces (one rendered on
# top of the other). If they are perceptually too close, the upper surface
# blends into the lower one and visually disappears.
#
# `min_delta_e` is the minimum CIE76 ΔE*ab distance (perceptual distance in
# CIELAB space) we require between the two roles. A value of 5.0 is the
# classical "clearly distinguishable surfaces" boundary in color science.
# All built-in themes today sit at ΔE >= 8, so 5.0 leaves comfortable
# headroom while catching pathological cases (e.g. role-aliased to identical
# hex). Survey the current distribution with:
#
#   python3 bin/theme/contrast.py
#
# before tightening this further.
SURFACE_DISTINCTNESS_PAIRS = (
    # Setting cards (palette.window) sit on the settings tab content backdrop
    # (palette.alternateBase); the sidebar and the search-bar header strip
    # also use alternateBase. Equal hex makes setting rows invisible until
    # hover — see resources/qml/pages/UserSettingsPage.qml and
    # resources/qml/pages/settings/SettingsContent.qml.
    ("window", "alternateBase", 5.0),
)

REQUIRED_FIELDS = ("name", "variant", "palette", "userColors")
VALID_VARIANTS = ("light", "dark")
VALID_THEME_PREFIXES = tuple(f"{variant}-" for variant in VALID_VARIANTS)
BASE16_SLOTS = [f"base{i:02X}" for i in range(16)]

def validate_theme(path: str) -> list[str]:
    """Validate a single theme YAML file. Returns list of error messages."""
    errors = []
    filename = os.path.basename(path)

    try:
        data = parse_yaml(path)
    except Exception as e:
        return [f"{filename}: Failed to parse: {e}"]

    # Check required fields
    for field in REQUIRED_FIELDS:
        if field not in data:
            errors.append(f"{filename}: Missing required field: {field}")

    # Check variant
    variant = data.get("variant", "")
    if variant and variant not in VALID_VARIANTS:
        errors.append(
            f"{filename}: variant must be 'light' or 'dark', got: {variant!r}"
        )

    # Check name doesn't redundantly include variant
    name = data.get("name", "")
    if name:
        name_lower = name.lower()
        for suffix in (" dark", " light"):
            if name_lower.endswith(suffix):
                errors.append(
                    f"{filename}: name {name!r} ends with {suffix.strip()!r}"
                    " — the variant field already carries this; strip the suffix"
                )

    # Check theme identifier prefix consistency
    slug = os.path.splitext(filename)[0]
    slug_lower = slug.lower()
    if not any(slug_lower.startswith(prefix) for prefix in VALID_THEME_PREFIXES):
        prefixes = ", ".join(VALID_THEME_PREFIXES)
        errors.append(
            f"{filename}: theme identifier must start with one of: {prefixes}"
        )

    if variant and variant in VALID_VARIANTS:
        if not slug_lower.startswith(f"{variant}-"):
            errors.append(
                f"{filename}: variant={variant!r} does not match theme identifier '{filename}'"
            )

    # Check palette — all keys must exist with valid hex
    palette = data.get("palette")
    if isinstance(palette, dict):
        for key in ALL_PALETTE_KEYS:
            if key not in palette:
                errors.append(f"{filename}: Missing palette key: {key}")
            else:
                value = palette[key]
                if not HEX_COLOR_RE.match(value):
                    errors.append(
                        f"{filename}: Invalid hex color for {key}: {value!r}"
                    )

        # Warn about unexpected palette keys
        valid_palette_keys = set(ALL_PALETTE_KEYS)
        for key in palette:
            if key not in valid_palette_keys:
                errors.append(f"{filename}: Unexpected palette key: {key}")

        # Surface-on-surface perceptual distinctness. Only run when both keys
        # parsed cleanly — otherwise the earlier hex/missing-key errors already
        # told the maintainer what to fix.
        for role_a, role_b, min_delta_e in SURFACE_DISTINCTNESS_PAIRS:
            value_a = palette.get(role_a)
            value_b = palette.get(role_b)
            if (
                isinstance(value_a, str)
                and isinstance(value_b, str)
                and HEX_COLOR_RE.match(value_a)
                and HEX_COLOR_RE.match(value_b)
            ):
                de = delta_e_lab(value_a, value_b)
                if de < min_delta_e:
                    errors.append(
                        f"{filename}: palette.{role_a} ({value_a}) and "
                        f"palette.{role_b} ({value_b}) are too close "
                        f"perceptually (ΔE={de:.2f}, need >={min_delta_e:.1f}). "
                        f"These roles are layered as adjacent UI surfaces; "
                        f"the upper surface will blend into the lower one."
                    )

    elif "palette" in data:
        errors.append(f"{filename}: 'palette' must be a mapping, not a scalar")

    # Check userColors — required section
    user_colors = data.get("userColors")
    if isinstance(user_colors, dict):
        # Check self
        if "self" not in user_colors:
            errors.append(f"{filename}: Missing userColors.self")
        else:
            try:
                normalize_user_color_slot(user_colors["self"], "userColors.self")
            except ValueError as exc:
                errors.append(f"{filename}: {exc}")

        # Check others
        if "others" not in user_colors:
            errors.append(f"{filename}: Missing userColors.others")
        elif not isinstance(user_colors["others"], list):
            errors.append(f"{filename}: userColors.others must be a list")
        else:
            others = user_colors["others"]
            if len(others) < 1:
                errors.append(
                    f"{filename}: userColors.others must have at least 1 entry, got {len(others)}"
                )
            for i, value in enumerate(others):
                try:
                    normalize_user_color_slot(value, f"userColors.others[{i}]")
                except ValueError as exc:
                    errors.append(f"{filename}: {exc}")

        # Warn about unexpected userColors keys
        valid_user_color_keys = {"self", "others"}
        for key in user_colors:
            if key not in valid_user_color_keys:
                errors.append(f"{filename}: Unexpected userColors key: {key}")

    elif "userColors" in data:
        errors.append(f"{filename}: 'userColors' must be a mapping, not a scalar")

    # Check source_base16 (optional provenance section)
    source_base16 = data.get("source_base16")
    if isinstance(source_base16, dict):
        for key, value in source_base16.items():
            if key not in BASE16_SLOTS:
                errors.append(f"{filename}: Unexpected source_base16 key: {key}")
            elif not HEX_COLOR_RE.match(value):
                errors.append(
                    f"{filename}: Invalid hex color for source_base16 {key}: {value!r}"
                )

    return errors


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.join(script_dir, "..", "..")
    themes_dir = os.path.join(project_root, "resources", "themes")

    if not os.path.isdir(themes_dir):
        print("No resources/themes/ directory found — nothing to validate.")
        sys.exit(0)

    yaml_files = sorted(f for f in os.listdir(themes_dir) if f.endswith(".yml"))

    if not yaml_files:
        print("No .yml files found in resources/themes/ — nothing to validate.")
        sys.exit(0)

    all_errors = []
    for filename in yaml_files:
        filepath = os.path.join(themes_dir, filename)
        errors = validate_theme(filepath)
        all_errors.extend(errors)

    if all_errors:
        print("ERROR: Theme YAML validation failed!")
        print("")
        for error in all_errors:
            print(f"  {error}")
        print("")
        print(f"Checked {len(yaml_files)} theme(s), found {len(all_errors)} error(s).")
        sys.exit(1)

    print(f"All {len(yaml_files)} theme YAML files are valid.")


if __name__ == "__main__":
    main()
