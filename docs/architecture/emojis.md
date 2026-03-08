# Emoji Architecture

This document describes how Komai obtains emoji metadata, builds localized search datasets, and loads them at runtime.

## Goals

- Keep the app self-contained at runtime (single-binary packaging via Qt resources).
- Avoid tracking large upstream emoji datasets in git.
- Support localized emoji completion/search tokens per UI locale.
- Allow small tracked per-locale overrides for custom tokens.
- Preserve future flexibility for skin tone filtering/preference features.

## Source of Truth

Tracked configuration and tooling:

- `bin/emoji/sources.lock.yml`: pinned upstream source configuration (URLs/ref/checksum).
- `bin/emoji/pipeline.py`: fetch/build/check/add-token pipeline tool.
- `resources/emoji/overrides/global.yml`: optional global overrides.
- `resources/emoji/overrides/locale/*.yml`: optional locale-specific overrides.
- Legacy `resources/shortcodes.txt` is retired; baseline aliases now live in
  `resources/emoji/overrides/locale/en.yml`.

Not tracked (generated/cache):

- `var/emoji/cache/<lock-hash>/...`: fetched upstream payload cache.
- `var/emoji/generated/<lock-hash>/...`: generated runtime JSON datasets.

## Upstream Inputs

- Unicode emoji data (`emoji-test.txt`):
  - https://unicode.org/Public/emoji/latest/emoji-test.txt
  - https://unicode.org/Public/emoji/latest/
- CLDR localized annotations:
  - https://github.com/unicode-org/cldr-json
  - Example annotations:
    - https://github.com/unicode-org/cldr-json/blob/main/cldr-json/cldr-annotations-full/annotations/en/annotations.json
    - https://github.com/unicode-org/cldr-json/blob/main/cldr-json/cldr-annotations-derived-full/annotationsDerived/en/annotations.json
- Unicode licensing:
  - https://www.unicode.org/policies/licensing_policy.html
  - https://www.unicode.org/faq/unicode_license.html

## Build Pipeline

Primary commands:

- `just emoji-fetch`: fetch/refresh upstream cache in `var/emoji/cache/<lock-hash>/`.
- `just emoji-build`: generate runtime JSON in `var/emoji/generated/<lock-hash>/`.
- `just emoji-check`: validate overrides and run cache-based reproducibility/build check.
- `just emoji-add-token "<emoji>" <locale> "<token>"`: append locale override token.

Build integration:

- CMake runs `bin/emoji/pipeline.py build` into `${CMAKE_CURRENT_BINARY_DIR}/emoji-data`.
- Generated files are embedded with `qt_add_resources` under `:/emoji/...`.
- `komai` depends on the `emoji_runtime_data` target, so emoji data is generated during build.

If network is unavailable and cache is missing for the current lock hash, emoji generation fails.

## Runtime Dataset Shape

Core file (`core.json`):

- one record per emoji with:
  - stable `id` (codepoint sequence, e.g. `1F943`)
  - rendered `unicode`
  - `short_name` and `unicode_name`
  - category/subgroup/order
  - skin tone metadata: `skin_tone_class`, `base_id`, `has_skin_tone_variants`

Locale file (`locale/<locale>.json`):

- map keyed by emoji `id`
- per emoji:
  - `display_name`: localized visible label
  - `primary_token`: preferred token
  - `tokens`: all searchable tokens (CLDR + overrides)

## Overrides Model

Override entries are list-based YAML records. Example:

```yaml
version: 1
emoji:
  - id: "1F943"
    preview: "🥃"
    locale: "bg"
    display_name: "Уиски"
    primary_token: "уиски"
    tokens_add: ["бърбън", "скоч"]
    tokens_remove: ["алкохол"]
```

Validation in `emoji-check`/build:

- locale must exist in `resources/langs/*`
- duplicate override entries (`locale + id`) are rejected
- `tokens_add`/`tokens_remove` overlap is rejected
- `primary_token` cannot be removed
- when core dataset is available, `id` and `preview` are validated

Override precedence:

- `locale/en.yml` tokens are treated as a baseline and applied to all locales
- locale-specific file overrides (for example `locale/ja.yml`) are then applied on top
- for non-`en` locales, `en.yml` baseline does not override localized `display_name` or
  `primary_token`; it only contributes token add/remove behavior

## Runtime Loading and Search

- `src/emoji/Provider.cpp` lazily loads:
  - `:/emoji/core.json`
  - best-match `:/emoji/locale/<locale>.json` with region/script fallback
- search text is built from:
  - `short_name`
  - `unicode_name`
  - locale tokens

Consumers:

- `src/imagepacks/CombinedImagePackModel.cpp`
- `src/imagepacks/GridImagePackModel.cpp`

These models now include provider-computed search text so localized tokens participate in completion matching.

### Fuzzy-Match Guardrails

Emoji completion supports fuzzy matching, but short queries need tighter limits to avoid noisy/unrelated results.

Real regression observed during rollout:

- Locale: Japanese (`LANG=ja`)
- Query: `:酒`
- Expected: alcohol-related emoji (for example `🍶`, `🍺`, `🍷`)
- Broken behavior (before fix): unrelated results such as country flags and skin-tone hand variants appeared.

Root cause:

- completion trie search used edit distance `2` even for 1-character inputs
- this made very short keys effectively over-broad

Current behavior:

- composer-style trigger prefixes are stripped before trie lookup (for example `:whiskey`, `:酒`,
  and full-width `：酒`)
- both indexed search text and query input are normalized with Unicode NFKD + case-fold to avoid
  script/combining-mark mismatches (for example kana with dakuten forms)
- whitespace-delimited tokens are indexed verbatim in addition to word-boundary pieces, so
  multi-character CJK tokens like `玄米茶` remain directly searchable
- trie search now runs in two phases:
  - exact/prefix search first
  - fuzzy search only if exact/prefix returned no results
- query length `<= 2`: fuzzy edit distance forced to `0` (exact/prefix only)
- query length `3-6`: fuzzy edit distance capped at `1`
- query length `>= 7`: configured fuzzy distance is used

This policy is applied in both:

- inline completion proxy (`src/models/CompletionProxyModel.cpp`)
- emoji/sticker grid search (`src/imagepacks/GridImagePackModel.cpp`)

## Packaging and Repository Size

- Upstream Unicode/CLDR payloads are not vendored in git.
- Only small lock/config/override files are tracked.
- Runtime emoji metadata remains embedded in the binary through Qt resources.

## Update Workflow

1. Update `bin/emoji/sources.lock.yml` (Unicode URL/hash and CLDR ref as needed).
2. Run:
   - `just emoji-fetch`
   - `just emoji-build`
3. Run verification:
   - `just lint`
   - `just build`
4. If needed, add localized project-specific tokens via:
   - `just emoji-add-token "<emoji>" <locale> "<token>"`
