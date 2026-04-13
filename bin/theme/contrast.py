#!/usr/bin/env python3
"""Report theme contrast ratios for built-in or custom theme files.

Usage examples:
  python3 bin/theme/contrast.py
  python3 bin/theme/contrast.py light-komai
  python3 bin/theme/contrast.py resources/themes/light-komai.yml --fail-aa
"""

from __future__ import annotations

import argparse
import os
import sys
from dataclasses import dataclass

from colors import (
    contrast_ratio,
    derive_readable_accent_text_color,
    normalize_user_color_slot,
    parse_yaml,
    tint_color,
)


@dataclass(frozen=True)
class CheckResult:
    label: str
    ratio: float
    target: float
    hard: bool

    @property
    def status(self) -> str:
        if self.ratio >= self.target:
            return "OK"
        return "FAIL" if self.hard else "WARN"

def build_core_checks(palette: dict[str, str]) -> list[CheckResult]:
    return [
        CheckResult("window/text", contrast_ratio(palette["window"], palette["text"]), 4.5, True),
        CheckResult(
            "window/buttonText",
            contrast_ratio(palette["window"], palette["buttonText"]),
            4.5,
            True,
        ),
        CheckResult("base/text", contrast_ratio(palette["base"], palette["text"]), 4.5, True),
        CheckResult(
            "alternateBase/text",
            contrast_ratio(palette["alternateBase"], palette["text"]),
            4.5,
            True,
        ),
        CheckResult(
            "alternateBase/buttonText",
            contrast_ratio(palette["alternateBase"], palette["buttonText"]),
            4.5,
            True,
        ),
        CheckResult(
            "highlight/highlightedText",
            contrast_ratio(palette["highlight"], palette["highlightedText"]),
            4.5,
            True,
        ),
        CheckResult(
            "dark/brightText",
            contrast_ratio(palette["dark"], palette["brightText"]),
            4.5,
            True,
        ),
        CheckResult(
            "light/windowText",
            contrast_ratio(palette["light"], palette["windowText"]),
            4.5,
            True,
        ),
        CheckResult("window/link", contrast_ratio(palette["window"], palette["link"]), 4.5, True),
        CheckResult("base/link", contrast_ratio(palette["base"], palette["link"]), 4.5, True),
        CheckResult(
            "alternateBase/link",
            contrast_ratio(palette["alternateBase"], palette["link"]),
            4.5,
            True,
        ),
    ]


def build_surface_checks(palette: dict[str, str]) -> list[CheckResult]:
    return [
        CheckResult(
            "window/alternateBase",
            contrast_ratio(palette["window"], palette["alternateBase"]),
            1.2,
            False,
        ),
        CheckResult("window/base", contrast_ratio(palette["window"], palette["base"]), 1.2, False),
        CheckResult(
            "window/highlight",
            contrast_ratio(palette["window"], palette["highlight"]),
            3.0,
            False,
        ),
    ]


def build_activity_tint_checks(palette: dict[str, str]) -> list[CheckResult]:
    """Check that room list / tab bar activity tints have readable text.

    These match the QML blending used in RoomListItemDelegate and RoomTabDelegate:
    - Unread: 15% highlight over window, text is buttonText
    - Draft:  12% attention over window, text is buttonText
    - Active: 50% dark over window, text is text
    - Hover:  30% dark over window, text is buttonText
    """
    unread_bg = tint_color(palette["highlight"], palette["window"], 0.15)
    draft_bg = tint_color(palette["attention"], palette["window"], 0.12)
    active_bg = tint_color(palette["dark"], palette["window"], 0.85)
    hover_bg = tint_color(palette["dark"], palette["window"], 0.30)
    return [
        CheckResult(
            "tint:unread/buttonText",
            contrast_ratio(unread_bg, palette["buttonText"]),
            4.5,
            True,
        ),
        CheckResult(
            "tint:draft/buttonText",
            contrast_ratio(draft_bg, palette["buttonText"]),
            4.5,
            True,
        ),
        CheckResult(
            "tint:active/brightText",
            contrast_ratio(active_bg, palette["brightText"]),
            4.5,
            True,
        ),
        CheckResult(
            "tint:hover/text",
            contrast_ratio(hover_bg, palette["text"]),
            4.5,
            True,
        ),
    ]


def resolve_slot(slot: dict[str, str], palette: dict[str, str]) -> dict[str, str]:
    return {
        "background": slot["background"],
        "text": slot.get("text", palette["text"]),
        "secondaryText": slot.get("secondaryText", palette["buttonText"]),
        "link": slot.get("link", palette["link"]),
    }


def build_bubble_summary_checks(
    palette: dict[str, str], user_colors: dict[str, dict | list[dict]]
) -> list[CheckResult]:
    self_slot = resolve_slot(normalize_user_color_slot(user_colors["self"], "userColors.self"), palette)
    other_slots = [
        resolve_slot(normalize_user_color_slot(slot, f"userColors.others[{index}]"), palette)
        for index, slot in enumerate(user_colors["others"])
    ]
    derived_self = derive_readable_accent_text_color(self_slot["background"], self_slot["background"])
    derived_others = [
        derive_readable_accent_text_color(slot["background"], slot["background"])
        for slot in other_slots
    ]

    return [
        CheckResult(
            "bubble:self/text",
            contrast_ratio(self_slot["background"], self_slot["text"]),
            4.5,
            True,
        ),
        CheckResult(
            "bubble:self/buttonText",
            contrast_ratio(self_slot["background"], self_slot["secondaryText"]),
            4.5,
            False,
        ),
        CheckResult(
            "bubble:self/link",
            contrast_ratio(self_slot["background"], self_slot["link"]),
            4.5,
            True,
        ),
        CheckResult(
            "bubble:self/derivedText",
            contrast_ratio(self_slot["background"], derived_self),
            4.5,
            True,
        ),
        CheckResult(
            "bubble:others/text min",
            min(contrast_ratio(slot["background"], slot["text"]) for slot in other_slots),
            4.5,
            True,
        ),
        CheckResult(
            "bubble:others/buttonText min",
            min(
                contrast_ratio(slot["background"], slot["secondaryText"])
                for slot in other_slots
            ),
            4.5,
            False,
        ),
        CheckResult(
            "bubble:others/link min",
            min(contrast_ratio(slot["background"], slot["link"]) for slot in other_slots),
            4.5,
            True,
        ),
        CheckResult(
            "bubble:others/derivedText min",
            min(
                contrast_ratio(slot["background"], derived)
                for slot, derived in zip(other_slots, derived_others)
            ),
            4.5,
            True,
        ),
    ]


def build_user_color_detail_rows(
    palette: dict[str, str], user_colors: dict[str, dict | list[dict]]
) -> list[list[str]]:
    rows = []
    all_slots = [("self", resolve_slot(normalize_user_color_slot(user_colors["self"], "userColors.self"), palette))] + [
        (
            f"others[{index}]",
            resolve_slot(normalize_user_color_slot(slot, f"userColors.others[{index}]"), palette),
        )
        for index, slot in enumerate(user_colors["others"])
    ]
    for label, slot in all_slots:
        derived = derive_readable_accent_text_color(slot["background"], slot["background"])
        rows.append(
            [
                label,
                slot["background"],
                f"{contrast_ratio(slot['background'], slot['text']):.2f}",
                f"{contrast_ratio(slot['background'], slot['secondaryText']):.2f}",
                f"{contrast_ratio(slot['background'], slot['link']):.2f}",
                derived,
                f"{contrast_ratio(slot['background'], derived):.2f}",
            ]
        )
    return rows


def render_table(headers: list[str], rows: list[list[str]]) -> str:
    widths = [len(header) for header in headers]
    for row in rows:
        for index, cell in enumerate(row):
            widths[index] = max(widths[index], len(cell))

    def render_row(row: list[str]) -> str:
        return "  " + "  ".join(cell.ljust(widths[index]) for index, cell in enumerate(row))

    parts = [render_row(headers), render_row(["-" * width for width in widths])]
    parts.extend(render_row(row) for row in rows)
    return "\n".join(parts)


def resolve_theme_paths(args: argparse.Namespace) -> list[str]:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.join(script_dir, "..", "..")
    themes_dir = os.path.join(project_root, "resources", "themes")

    if not args.themes:
        return sorted(
            os.path.join(themes_dir, filename)
            for filename in os.listdir(themes_dir)
            if filename.endswith(".yml")
        )

    paths = []
    for theme in args.themes:
        if os.path.isfile(theme):
            paths.append(os.path.abspath(theme))
            continue

        candidate = os.path.join(themes_dir, f"{theme}.yml")
        if os.path.isfile(candidate):
            paths.append(candidate)
            continue

        print(f"ERROR: Theme not found: {theme}", file=sys.stderr)
        sys.exit(1)

    return paths


def theme_slug_from_path(path: str) -> str:
    return os.path.splitext(os.path.basename(path))[0]


def report_theme(path: str, verbose: bool) -> tuple[str, int]:
    data = parse_yaml(path)
    palette = data["palette"]
    user_colors = data["userColors"]
    slug = theme_slug_from_path(path)
    variant = data.get("variant", "?")

    core_checks = build_core_checks(palette)
    surface_checks = build_surface_checks(palette)
    activity_checks = build_activity_tint_checks(palette)
    bubble_checks = build_bubble_summary_checks(palette, user_colors)
    all_checks = core_checks + surface_checks + activity_checks + bubble_checks
    hard_failures = sum(1 for check in all_checks if check.hard and check.ratio < check.target)

    lines = [f"Theme: {slug} ({variant})"]
    lines.append("")
    lines.append("Core checks")
    lines.append(
        render_table(
            ["Pair", "Ratio", "Target", "Status"],
            [
                [check.label, f"{check.ratio:.2f}", f">={check.target:.1f}", check.status]
                for check in core_checks
            ],
        )
    )
    lines.append("")
    lines.append("Surface checks")
    lines.append(
        render_table(
            ["Pair", "Ratio", "Target", "Status"],
            [
                [check.label, f"{check.ratio:.2f}", f">={check.target:.1f}", check.status]
                for check in surface_checks
            ],
        )
    )
    lines.append("")
    lines.append("Activity tint checks")
    lines.append(
        render_table(
            ["Pair", "Ratio", "Target", "Status"],
            [
                [check.label, f"{check.ratio:.2f}", f">={check.target:.1f}", check.status]
                for check in activity_checks
            ],
        )
    )
    lines.append("")
    lines.append("Bubble summary")
    lines.append(
        render_table(
            ["Pair", "Ratio", "Target", "Status"],
            [
                [check.label, f"{check.ratio:.2f}", f">={check.target:.1f}", check.status]
                for check in bubble_checks
            ],
        )
    )

    if verbose:
        lines.append("")
        lines.append("User color details")
        lines.append(
            render_table(
                ["Slot", "Fill", "Text", "Button", "Link", "Derived", "Derived C"],
                build_user_color_detail_rows(palette, user_colors),
            )
        )

    return "\n".join(lines), hard_failures


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "themes",
        nargs="*",
        help="Theme slug(s) from resources/themes/ or direct .yml paths. Defaults to all themes.",
    )
    parser.add_argument(
        "--fail-aa",
        action="store_true",
        help="Exit non-zero if any hard AA-style checks fail.",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print individual userColors rows, not only summary checks.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    theme_paths = resolve_theme_paths(args)
    verbose = args.verbose or len(theme_paths) == 1

    total_failures = 0
    for index, path in enumerate(theme_paths):
        report, failures = report_theme(path, verbose)
        if index:
            print("")
        print(report)
        total_failures += failures

    if args.fail_aa and total_failures:
        print("")
        print(f"AA check failed: {total_failures} hard check(s) below target.", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
