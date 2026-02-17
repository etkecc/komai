#!/usr/bin/env python3
"""Validate resources/themes/*.yaml files.

Checks:
  - Required fields: system, name, variant, palette
  - variant is "light" or "dark"
  - All 16 Base16 palette slots (base00-base0F) are present
  - All color values are valid 6-character hex strings
  - Override keys (if present) are known QPalette roles or custom properties
"""

import os
import re
import sys

REQUIRED_FIELDS = ("system", "name", "variant", "palette")
VALID_VARIANTS = ("light", "dark")
BASE16_SLOTS = [f"base{i:02X}" for i in range(16)]
VALID_OVERRIDE_KEYS = {
    # QPalette roles
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
    # Custom Theme properties
    "red",
    "green",
    "orange",
    "error",
}

HEX_RE = re.compile(r"^[0-9a-fA-F]{6}$")


def parse_yaml(path: str) -> dict:
    """Minimal YAML parser for Base16 theme files.

    Handles the flat structure of Base16 YAML: top-level scalars and one
    level of nested mapping (palette:, overrides:). No external dependency.
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
                    # Section header (e.g., "palette:" or "overrides:")
                    result[key] = {}
                    current_section = key

    return result


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

    # Check palette
    palette = data.get("palette")
    if isinstance(palette, dict):
        # Check all 16 slots exist
        for slot in BASE16_SLOTS:
            if slot not in palette:
                errors.append(f"{filename}: Missing palette slot: {slot}")
            else:
                value = palette[slot]
                if not HEX_RE.match(value):
                    errors.append(
                        f"{filename}: Invalid hex color for {slot}: {value!r}"
                    )

        # Warn about unexpected palette keys
        for key in palette:
            if key not in BASE16_SLOTS:
                errors.append(f"{filename}: Unexpected palette key: {key}")

    elif "palette" in data:
        errors.append(f"{filename}: 'palette' must be a mapping, not a scalar")

    # Check overrides (optional)
    overrides = data.get("overrides")
    if isinstance(overrides, dict):
        for key, value in overrides.items():
            if key not in VALID_OVERRIDE_KEYS:
                errors.append(f"{filename}: Unknown override key: {key}")
            if not HEX_RE.match(value):
                errors.append(
                    f"{filename}: Invalid hex color for override {key}: {value!r}"
                )

    return errors


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.join(script_dir, "..")
    themes_dir = os.path.join(project_root, "resources", "themes")

    if not os.path.isdir(themes_dir):
        print("No resources/themes/ directory found — nothing to validate.")
        sys.exit(0)

    yaml_files = sorted(f for f in os.listdir(themes_dir) if f.endswith(".yaml"))

    if not yaml_files:
        print("No .yaml files found in resources/themes/ — nothing to validate.")
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
