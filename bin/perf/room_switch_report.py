#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import argparse
import math
import os
import re
from collections import defaultdict
from pathlib import Path


LINE_RE = re.compile(
    r"\[perf\]\[room-switch\]\s+switch_id=(?P<switch_id>\d+)\s+"
    r"room_id=(?P<room_id>\S+)\s+phase=(?P<phase>\S+)"
    r"(?:\s+source=(?P<source>\S+)\s+elapsed_ms=(?P<elapsed_ms>-?\d+))?"
    r"(?:\s+active_match=(?P<active_match>true|false)"
    r"(?:\s+active_room_id=(?P<active_room_id>\S+))?)?"
)
START_RE = re.compile(r"\[ui\]\s+\[info\]\s+starting komai\b")


def percentile(values: list[int], p: float) -> float:
    if not values:
        return float("nan")
    if len(values) == 1:
        return float(values[0])

    sorted_values = sorted(values)
    rank = (len(sorted_values) - 1) * p
    low = math.floor(rank)
    high = math.ceil(rank)
    if low == high:
        return float(sorted_values[low])
    weight = rank - low
    return sorted_values[low] * (1 - weight) + sorted_values[high] * weight


def parse_logs(paths: list[Path]) -> list[dict[str, object]]:
    switches: list[dict[str, object]] = []
    active_by_switch_id: dict[int, dict[str, object]] = {}
    run_id = 0

    for path in paths:
        with path.open("r", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                if START_RE.search(line):
                    run_id += 1
                    active_by_switch_id = {}

                match = LINE_RE.search(line)
                if not match:
                    continue

                switch_id = int(match.group("switch_id"))
                room_id = match.group("room_id")
                phase = match.group("phase")
                elapsed_ms = match.group("elapsed_ms")
                active_match_raw = match.group("active_match")
                active_match = (
                    None if active_match_raw is None else active_match_raw.lower() == "true"
                )

                # switch_id resets per app run; "request" starts a new switch instance.
                if phase == "request":
                    switch = {
                        "switch_id": switch_id,
                        "run_id": run_id,
                        "room_id": room_id,
                        "phases": {},
                        "raw_lines": 0,
                    }
                    switches.append(switch)
                    active_by_switch_id[switch_id] = switch

                switch = active_by_switch_id.get(switch_id)
                if switch is None:
                    # Be resilient to partial logs that don't include request lines.
                    switch = {
                        "switch_id": switch_id,
                        "run_id": run_id,
                        "room_id": room_id,
                        "phases": {},
                        "raw_lines": 0,
                    }
                    switches.append(switch)
                    active_by_switch_id[switch_id] = switch

                switch["room_id"] = room_id
                switch["raw_lines"] = int(switch["raw_lines"]) + 1
                phases = switch["phases"]
                assert isinstance(phases, dict)
                phases[phase] = {
                    "elapsed_ms": int(elapsed_ms) if elapsed_ms is not None else None,
                    "active_match": active_match,
                }

    return switches


def default_log_path_candidates(profile: str) -> list[Path]:
    paths: list[Path] = []
    cache_root = os.environ.get("XDG_CACHE_HOME")
    if cache_root:
        paths.append(Path(cache_root) / "komai" / "profiles" / profile / "komai.log")

    # Always include the conventional fallback even if XDG_CACHE_HOME is set differently.
    home_cache = Path.home() / ".cache" / "komai" / "profiles" / profile / "komai.log"
    if home_cache not in paths:
        paths.append(home_cache)

    return paths


def expand_rotated_paths(base: Path) -> list[Path]:
    if not base.parent.exists():
        return [base]

    expanded: list[Path] = []
    for candidate in base.parent.glob(f"{base.name}*"):
        if candidate.name == base.name:
            expanded.append(candidate)
            continue

        suffix = candidate.name[len(base.name) + 1 :]
        if suffix.isdigit():
            expanded.append(candidate)

    if not expanded:
        return [base]

    return sorted(expanded, key=lambda p: p.stat().st_mtime)


def resolve_log_paths(logfile: Path | None, profile: str) -> list[Path]:
    if logfile is not None:
        return expand_rotated_paths(logfile)

    resolved: list[Path] = []
    for base in default_log_path_candidates(profile):
        for path in expand_rotated_paths(base):
            if path not in resolved:
                resolved.append(path)

    return resolved


def pick_latest_existing_path(paths: list[Path]) -> Path | None:
    existing = [path for path in paths if path.exists()]
    if not existing:
        return None
    return max(existing, key=lambda p: p.stat().st_mtime)


def summarize(switches: list[dict[str, object]]) -> str:
    if not switches:
        return "No [perf][room-switch] markers found."

    phase_values: dict[str, list[int]] = defaultdict(list)
    skipped_inactive = 0
    for data in switches:
        phases = data["phases"]
        assert isinstance(phases, dict)
        for phase, details in phases.items():
            assert isinstance(details, dict)
            elapsed = details.get("elapsed_ms")
            active_match = details.get("active_match")
            if active_match is False:
                skipped_inactive += 1
                continue
            if isinstance(elapsed, int) and elapsed >= 0:
                phase_values[phase].append(elapsed)

    lines = []
    lines.append(f"Switches: {len(switches)}")
    lines.append("")

    key_phase_aliases = [
        ("qml.message_view.first_visible_item", "first_visible_item_ms"),
        ("qml.message_input.next_tick", "composer_ready_ms"),
        ("qml.timeline_view.next_tick", "timeline_next_tick_ms"),
        ("cpp.current_room_changed_emitted", "current_room_changed_ms"),
    ]

    lines.append("Key Metrics:")
    for phase, alias in key_phase_aliases:
        values = phase_values.get(phase, [])
        if not values:
            lines.append(f"- {alias}: n=0")
            continue
        lines.append(
            f"- {alias}: n={len(values)} p50={percentile(values, 0.50):.1f} "
            f"p95={percentile(values, 0.95):.1f} p99={percentile(values, 0.99):.1f} "
            f"max={max(values)}"
        )

    lines.append("")
    lines.append("All Phases:")
    for phase in sorted(phase_values):
        values = phase_values[phase]
        lines.append(
            f"- {phase}: n={len(values)} p50={percentile(values, 0.50):.1f} "
            f"p95={percentile(values, 0.95):.1f} max={max(values)}"
        )
    if skipped_inactive:
        lines.append("")
        lines.append(f"Filtered out {skipped_inactive} phase marker(s) with active_match=false.")

    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Summarize room-switch performance markers from Komai logs."
    )
    parser.add_argument(
        "logfile",
        nargs="?",
        type=Path,
        help="Path to Komai log file (optional when using --profile).",
    )
    parser.add_argument(
        "--profile",
        default="default",
        help="Komai profile id used to resolve default log path when logfile is omitted.",
    )
    parser.add_argument(
        "--all-log-files",
        action="store_true",
        help="Aggregate markers from all rotated log files instead of only the latest log file.",
    )
    parser.add_argument(
        "--all-runs",
        action="store_true",
        help="Include all app runs found in the selected log files (default: latest run only).",
    )
    args = parser.parse_args()

    log_paths = resolve_log_paths(args.logfile, args.profile)
    latest_path = pick_latest_existing_path(log_paths)
    if latest_path is None:
        primary_candidate = (
            args.logfile if args.logfile is not None else default_log_path_candidates(args.profile)[0]
        )
        print(f"Log file not found: {primary_candidate}")
        print("Hint: use the profile you actually ran, for example:")
        print("  just run-with-perf-trace -- -p perf")
        print("  just perf-room-switch-report-profile perf")
        return 1

    selected_paths = [path for path in log_paths if path.exists()] if args.all_log_files else [latest_path]

    switches = parse_logs(selected_paths)
    if switches and not args.all_runs:
        latest_run_id = max(int(switch.get("run_id", 0)) for switch in switches)
        switches = [switch for switch in switches if int(switch.get("run_id", 0)) == latest_run_id]

    if args.all_log_files:
        print("Log files:")
        for path in selected_paths:
            print(f"- {path}")
    else:
        print(f"Log file: {selected_paths[0]}")
    print("")
    summary = summarize(switches)
    print(summary)
    if summary == "No [perf][room-switch] markers found.":
        print("")
        print("Hint: perf markers can be stderr-only if KOMAI_LOG_TYPE excludes 'file'.")
        print("Run via 'just run-with-perf-trace -- -p <profile>' and keep 'file' in KOMAI_LOG_TYPE.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
