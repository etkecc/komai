#!/usr/bin/env python3
"""Report theme contrast ratios for built-in or custom theme files.

Usage examples:
  python3 bin/theme/contrast.py
  python3 bin/theme/contrast.py komai-light
  python3 bin/theme/contrast.py resources/themes/komai-light.yml --fail-aa
"""

from __future__ import annotations

import argparse
import colorsys
import os
import sys
from dataclasses import dataclass

from colors import contrast_ratio, parse_color, parse_yaml


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


def rgb_to_hex(rgb: tuple[int, int, int]) -> str:
    return "#{:02x}{:02x}{:02x}".format(*rgb)


def qcolor_darker(hex_str: str, factor: int) -> str:
    r, g, b = parse_color(hex_str)
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    v = max(0.0, min(1.0, v * 100.0 / factor))
    rr, gg, bb = colorsys.hsv_to_rgb(h, s, v)
    return rgb_to_hex((round(rr * 255), round(gg * 255), round(bb * 255)))


def qcolor_lighter(hex_str: str, factor: int) -> str:
    r, g, b = parse_color(hex_str)
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    v = max(0.0, min(1.0, v * factor / 100.0))
    rr, gg, bb = colorsys.hsv_to_rgb(h, s, v)
    return rgb_to_hex((round(rr * 255), round(gg * 255), round(bb * 255)))


def derive_readable_accent_text_color(
    accent_color: str, background_color: str, min_contrast: float = 4.5
) -> str:
    if contrast_ratio(accent_color, background_color) >= min_contrast:
        return accent_color

    prefer_darker = contrast_ratio("#000000", background_color) >= contrast_ratio(
        "#ffffff", background_color
    )
    best_color: str | None = None
    best_distance = sys.maxsize
    best_contrast = 0.0

    def consider(candidate: str, distance: int) -> None:
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


def build_bubble_summary_checks(
    palette: dict[str, str], user_colors: dict[str, str | list[str]]
) -> list[CheckResult]:
    others = list(user_colors["others"])
    self_color = str(user_colors["self"])
    derived_self = derive_readable_accent_text_color(self_color, self_color)
    derived_others = [derive_readable_accent_text_color(color, color) for color in others]

    return [
        CheckResult("bubble:self/text", contrast_ratio(self_color, palette["text"]), 4.5, True),
        CheckResult(
            "bubble:self/buttonText",
            contrast_ratio(self_color, palette["buttonText"]),
            4.5,
            True,
        ),
        CheckResult("bubble:self/link", contrast_ratio(self_color, palette["link"]), 4.5, True),
        CheckResult(
            "bubble:self/derivedText",
            contrast_ratio(self_color, derived_self),
            4.5,
            True,
        ),
        CheckResult(
            "bubble:others/text min",
            min(contrast_ratio(color, palette["text"]) for color in others),
            4.5,
            True,
        ),
        CheckResult(
            "bubble:others/buttonText min",
            min(contrast_ratio(color, palette["buttonText"]) for color in others),
            4.5,
            True,
        ),
        CheckResult(
            "bubble:others/link min",
            min(contrast_ratio(color, palette["link"]) for color in others),
            4.5,
            True,
        ),
        CheckResult(
            "bubble:others/derivedText min",
            min(contrast_ratio(color, derived) for color, derived in zip(others, derived_others)),
            4.5,
            True,
        ),
    ]


def build_user_color_detail_rows(
    palette: dict[str, str], user_colors: dict[str, str | list[str]]
) -> list[list[str]]:
    rows = []
    all_colors = [("self", str(user_colors["self"]))] + [
        (f"others[{index}]", color) for index, color in enumerate(user_colors["others"])
    ]
    for label, color in all_colors:
        derived = derive_readable_accent_text_color(color, color)
        rows.append(
            [
                label,
                color,
                f"{contrast_ratio(color, palette['text']):.2f}",
                f"{contrast_ratio(color, palette['buttonText']):.2f}",
                f"{contrast_ratio(color, palette['link']):.2f}",
                derived,
                f"{contrast_ratio(color, derived):.2f}",
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
    bubble_checks = build_bubble_summary_checks(palette, user_colors)
    all_checks = core_checks + surface_checks + bubble_checks
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
