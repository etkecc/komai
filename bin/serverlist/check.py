#!/usr/bin/env python3
"""Validate resources/serverlist/servers.yml.

Checks:
  - description and editorial locale maps have an 'en' key
  - All locale keys match a supported locale (resources/langs/ directories)
  - No locale key maps to an empty/blank string
"""

import os
import sys

import yaml

REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
SERVERS_YAML = os.path.join(REPO_ROOT, "resources", "serverlist", "servers.yml")
LANGS_DIR = os.path.join(REPO_ROOT, "resources", "langs")

LOCALIZED_FIELDS = ("description", "editorial")


def supported_locales() -> set[str]:
    """Return locale codes from resources/langs/ (directory names, skipping non-dirs)."""
    return {
        entry
        for entry in os.listdir(LANGS_DIR)
        if os.path.isdir(os.path.join(LANGS_DIR, entry))
    }


def validate_localized_field(
    server_name: str,
    field_name: str,
    value,
    locales: set[str],
) -> list[str]:
    """Validate a single localized field value. Returns error messages."""
    if isinstance(value, str):
        # Plain string — nothing locale-specific to check
        return []

    if not isinstance(value, dict):
        return [f"{server_name}: '{field_name}' must be a string or locale map, got {type(value).__name__}"]

    errors = []

    if not value:
        errors.append(f"{server_name}: '{field_name}' locale map is empty")
        return errors

    if "en" not in value:
        errors.append(f"{server_name}: '{field_name}' locale map is missing required 'en' key")

    for key, text in value.items():
        if key not in locales:
            errors.append(f"{server_name}: '{field_name}' has unknown locale key '{key}'")
        if not isinstance(text, str) or not text.strip():
            errors.append(f"{server_name}: '{field_name}[{key}]' is empty or not a string")

    return errors


def main() -> int:
    locales = supported_locales()

    with open(SERVERS_YAML) as f:
        data = yaml.safe_load(f)

    errors = []
    for server in data.get("servers", []):
        name = server.get("name", "<unknown>")
        for field in LOCALIZED_FIELDS:
            if field in server:
                errors.extend(validate_localized_field(name, field, server[field], locales))

    for err in errors:
        print(f"ERROR: {err}", file=sys.stderr)

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
