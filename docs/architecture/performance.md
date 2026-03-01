# ⚡ Performance Tracing

This page documents runtime knobs and anonymized experiment outcomes for room-switch and timeline performance work.

Important benchmark caveat:

- Results below come from local iterative runs and should be treated as directional, not lab-grade.
- Room identifiers and labels are intentionally anonymized.

## Room-Switch Perf Tracing

- Primary toggle: `KOMAI_ROOM_SWITCH_PERF=1`
- Supported alias: `KOMAI_PERF_ROOM_SWITCH=1`

When enabled, Komai logs structured room-switch phase markers like:

- `[perf][room-switch] ... phase=request`
- `[perf][room-switch] ... phase=qml.message_view.first_visible_item`

Use helper recipes:

```sh
just run-with-perf-trace -- -p default
just perf-room-switch-report-profile default
```

## Timeline Window Tuning

Optional runtime tuning knobs:

- `KOMAI_TIMELINE_INITIAL_WINDOW` (default: `12`)
- `KOMAI_TIMELINE_EXPAND_CHUNK` (default: `50`)

These affect initial timeline materialization and incremental expansion size in `EventStore`.

## Log-Level Controls

Komai supports runtime log-level control via CLI/environment:

- `--log-level <level-or-component-list>`
- `--debug` (alias for `--log-level trace`)
- `KOMAI_LOG_LEVEL` (fallback when CLI flag is not used)

Example:

```sh
var/build/native/komai --log-level info,ui=debug,db=info
```

Important: this works in normal release builds too. A Debug build mainly changes defaults (trace by default), but `--log-level` is still the primary runtime control.

## Room-List Prewarm Logs

Room-list prewarm emits structured logs:

- `[prewarm][room-list] trigger=<...> room_id=<...> action=<...> [reason=<...>]`

Behavior:

- Logged at `info` when room-switch perf tracing is enabled.
- Logged at `debug` otherwise (to reduce default log noise).

## Eager Materialization Guardrail

Komai emits one warning if unexpected startup room materialization exceeds a threshold:

- `[perf][room-list] unexpected eager materialization during startup ...`

This is intended as a low-noise signal for regressions where metadata-only startup accidentally starts initializing many room timeline models.

## Experiment Outcomes (Anonymized)

This section summarizes what was tried during the February-March 2026 optimization pass, including why changes were kept or dropped.

### ACCEPTED

#### 1) Staged room switch with deferred timeline heavy work

What changed:

- Room selection path was decoupled from heavy timeline bind/render work.
- Timeline area can acknowledge the switch immediately while heavy content continues loading.

Why accepted:

- Major perceived-latency improvement without hurting correctness.

Observed results:

- `composer_ready` became near-instant in sampled runs: p50 about `8-9ms`, max about `11-12ms`.
- Earlier comparable runs had room-switch p50 roughly in the `240-300ms` range on the same profile.

#### 2) Boundary-prefetch gating and explicit pagination ownership

What changed:

- Removed/contained eager boundary-driven expansion during initial paint.
- Pagination became controlled by explicit UI-driven paths instead of hidden repeated expansion.

Why accepted:

- Eliminated unrequested expansion bursts during room open.
- Improved worst-case startup switch behavior in heavy cached rooms.

Observed results:

- `first_visible_item` improved to about p50 `191ms`, p95 `253ms`, max `260ms` in sampled runs after gating.

#### 3) Smaller timeline windows for switch-time speedups (experiment phase)

What changed:

- Evaluated different `INITIAL_WINDOW` / `EXPAND_CHUNK` values and compared switch metrics.

Why accepted:

- Smaller windows reduced first-visible latency during the measured campaign.

Observed results:

- `INITIAL_WINDOW=20`, `EXPAND_CHUNK=40`: `first_visible_item` about p50 `135ms`, p95 `140ms`, max `141ms`.
- `INITIAL_WINDOW=80`, `EXPAND_CHUNK=80`: `first_visible_item` about p50 `269.5ms`, p95 `381.5ms`, max `394ms`.

Note:

- Runtime defaults are tuned separately based on ongoing UX validation and may differ from one specific benchmark winner.

### REJECTED

#### 1) "Hide unrelated UI pieces" as the primary fix

What was tested:

- Perf-flag runs with composer/header/effects disabled.

Why rejected:

- Did not produce meaningful wins versus baseline on the slow-path room switch.
- Root issue was timeline data/render path, not those UI sections.

Observed results:

- No-composer/no-header/no-effects runs stayed in roughly the same p50 band (`~280-301ms`) with similar tails.

#### 2) Large initial timeline windows as a default strategy

What was tested:

- Bigger initial windows/chunks to reduce follow-up pagination.

Why rejected:

- Increased first-visible latency and regressions on heavy rooms.
- Worse switch-time responsiveness for the target workload.

Observed results:

- `INITIAL_WINDOW=80`, `EXPAND_CHUNK=80` materially slower than smaller-window configurations (see accepted section above).

#### 3) Aggressive near-top prefetch heuristics (early variants)

What was tested:

- Percentage-based near-top triggers and burst-oriented rearm policies.

Why rejected:

- Produced unstable behavior in manual testing (over-eager expansion, tiny scrollbar, and interaction edge cases).
- Not reliable enough for default behavior.

### POSSIBILITY (Future Work)

#### 1) Lazy room-model materialization

Direction:

- Keep room-list rows metadata-backed by default.
- Materialize full timeline models on-demand (selection and explicit operations).

Potential upside:

- Lower startup and memory cost in large-account profiles.

#### 2) Benchmark hardening and automation

Direction:

- Expand repeat-count and room archetype coverage (small/large/encrypted/media-heavy).
- Automate run aggregation so p50/p95/p99 deltas are easier to track across commits.

Potential upside:

- More defensible, lower-noise perf decisions.

#### 3) Near-top pagination UX revisit

Direction:

- Revisit "near top" activation with stricter release-zone semantics and stronger anti-flicker anchoring.

Potential upside:

- Better scroll continuity without requiring exact hard-top contact.
