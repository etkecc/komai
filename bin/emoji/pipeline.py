#!/usr/bin/env python3
# SPDX-FileCopyrightText: Nheko Contributors
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Emoji pipeline: fetch upstream data, build runtime datasets, and validate overrides."""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
import os
import pathlib
import re
import shutil
import sys
import tempfile
import typing as t
import urllib.error
import urllib.request

try:
    import yaml
except ModuleNotFoundError:  # pragma: no cover - depends on local env
    yaml = None


CATEGORY_ORDER = [
    "People",
    "Nature",
    "Food",
    "Activity",
    "Travel",
    "Objects",
    "Symbols",
    "Flags",
]

GROUP_TO_CATEGORY = {
    "Smileys & Emotion": "People",
    "People & Body": "People",
    "Animals & Nature": "Nature",
    "Food & Drink": "Food",
    "Travel & Places": "Travel",
    "Activities": "Activity",
    "Objects": "Objects",
    "Symbols": "Symbols",
    "Flags": "Flags",
    "Component": "Symbols",
}

SKIN_TONE_MODIFIER_TO_CLASS = {
    "1F3FB": "single_light",
    "1F3FC": "single_medium_light",
    "1F3FD": "single_medium",
    "1F3FE": "single_medium_dark",
    "1F3FF": "single_dark",
}
SKIN_TONE_MODIFIERS = set(SKIN_TONE_MODIFIER_TO_CLASS)


@dataclasses.dataclass
class CoreEmoji:
    id: str
    unicode: str
    codepoints: list[str]
    category: str
    subgroup: str
    unicode_name: str
    short_name: str
    search_order: int
    skin_tone_class: str
    base_id: str | None
    has_skin_tone_variants: bool = False


def repo_root_from_arg(value: str | None) -> pathlib.Path:
    if value:
        return pathlib.Path(value).resolve()
    return pathlib.Path(__file__).resolve().parents[2]


def require_yaml() -> None:
    if yaml is None:
        raise SystemExit(
            "PyYAML is required for emoji pipeline scripts. "
            "Install `python3-yaml` (or pip install pyyaml)."
        )


def load_yaml(path: pathlib.Path) -> dict[str, t.Any]:
    require_yaml()
    with path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f)
    if data is None:
        return {}
    if not isinstance(data, dict):
        raise ValueError(f"{path} must contain a YAML mapping at top level")
    return data


def dump_yaml(path: pathlib.Path, data: dict[str, t.Any]) -> None:
    require_yaml()
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        yaml.safe_dump(data, f, allow_unicode=True, sort_keys=False)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def lock_hash(lock_path: pathlib.Path) -> str:
    return sha256_file(lock_path)[:16]


def normalize_token(value: str) -> str:
    token = value.strip()
    token = re.sub(r"\s+", " ", token)
    return token


def slugify_name(name: str) -> str:
    shortname = name.lower()
    if shortname.endswith(" (blood type)"):
        shortname = shortname[:-13]
    if shortname.endswith(": red hair"):
        shortname = "red_haired_" + shortname[:-10]
    if shortname.endswith(": curly hair"):
        shortname = "curly_haired_" + shortname[:-12]
    if shortname.endswith(": white hair"):
        shortname = "white_haired_" + shortname[:-12]
    if shortname.endswith(": bald"):
        shortname = "bald_" + shortname[:-6]
    if shortname.endswith(": beard"):
        shortname = "bearded_" + shortname[:-7]
    if shortname.endswith(" face"):
        shortname = shortname[:-5]
    if shortname.endswith(" button"):
        shortname = shortname[:-7]
    if shortname.endswith(" banknote"):
        shortname = shortname[:-9]
    if shortname.startswith("flag: "):
        shortname = shortname[5:] + " flag"

    shortname = shortname.replace("u.s.", "us")
    shortname = shortname.replace("&", "and")
    shortname = shortname.replace("-", "_")
    shortname = re.sub(r"\W", "_", shortname)
    shortname = re.sub(r"_{2,}", "_", shortname)
    shortname = shortname.strip("_")
    return shortname


def locale_candidates(locale: str) -> list[str]:
    candidates = []
    as_hyphen = locale.replace("_", "-")
    for c in [locale, as_hyphen]:
        if c and c not in candidates:
            candidates.append(c)
    if "_" in locale:
        base = locale.split("_", 1)[0]
        if base not in candidates:
            candidates.append(base)
    if "-" in as_hyphen:
        base = as_hyphen.split("-", 1)[0]
        if base not in candidates:
            candidates.append(base)
    return candidates


def fetch_url_bytes(url: str) -> bytes:
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": "komai-emoji-pipeline/1.0",
            "Accept": "application/json, text/plain, */*",
        },
    )
    with urllib.request.urlopen(req, timeout=45) as resp:
        return resp.read()


def split_cldr_tokens(raw: str) -> list[str]:
    tokens: list[str] = []
    for part in raw.split("|"):
        normalized = normalize_token(part)
        if normalized:
            tokens.append(normalized)
    return tokens


def looks_like_unicode_emoji_test(payload: bytes) -> bool:
    head = payload[:8192].decode("utf-8", errors="replace")
    return (
        "emoji-test.txt" in head
        and "# group:" in head
        and "fully-qualified" in head
    )


def payload_is_json(payload: bytes) -> bool:
    try:
        json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return False
    return True


def ensure_unicode_data(
    *,
    lock: dict[str, t.Any],
    cache_dir: pathlib.Path,
    force: bool,
) -> pathlib.Path:
    unicode_cfg = lock["unicode"]["emoji_test"]
    target = cache_dir / "unicode" / "emoji-test.txt"
    target.parent.mkdir(parents=True, exist_ok=True)

    if target.is_file() and not force:
        return target

    url = str(unicode_cfg["url"])
    expected_sha = str(unicode_cfg.get("sha256", "")).strip().lower()

    try:
        payload = fetch_url_bytes(url)
    except urllib.error.URLError as e:
        raise RuntimeError(
            f"Failed to fetch Unicode emoji data from {url}: {e}. "
            f"Populate cache first with `just emoji-fetch` (cache dir: {cache_dir})."
        ) from e

    if not looks_like_unicode_emoji_test(payload):
        raise RuntimeError(
            f"Downloaded payload from {url} is not a valid emoji-test.txt document."
        )

    actual_sha = sha256_bytes(payload)
    if expected_sha and actual_sha != expected_sha:
        raise RuntimeError(
            "Unicode emoji data checksum mismatch: "
            f"expected {expected_sha}, got {actual_sha}. "
            "Update bin/emoji/sources.lock.yml if intentional."
        )

    tmp = target.with_suffix(".tmp")
    tmp.write_bytes(payload)
    tmp.replace(target)
    return target


def ensure_cldr_locale_data(
    *,
    lock: dict[str, t.Any],
    cache_dir: pathlib.Path,
    locale: str,
    force: bool,
) -> tuple[pathlib.Path, pathlib.Path, str | None]:
    locale_dir = cache_dir / "cldr" / locale
    ann_path = locale_dir / "annotations.json"
    der_path = locale_dir / "annotationsDerived.json"
    meta_path = locale_dir / "meta.json"

    if ann_path.is_file() and der_path.is_file() and meta_path.is_file() and not force:
        source_locale = json.loads(meta_path.read_text(encoding="utf-8")).get("source_locale")
        return ann_path, der_path, source_locale

    locale_dir.mkdir(parents=True, exist_ok=True)

    ref = str(lock["cldr"]["ref"])
    ann_tpl = str(lock["cldr"]["annotations_url"])
    der_tpl = str(lock["cldr"]["derived_url"])

    errors: list[str] = []
    not_found: list[str] = []
    for candidate in locale_candidates(locale):
        ann_url = ann_tpl.format(ref=ref, locale=candidate)
        der_url = der_tpl.format(ref=ref, locale=candidate)

        try:
            ann_payload = fetch_url_bytes(ann_url)
        except urllib.error.HTTPError as e:
            if e.code == 404:
                not_found.append(f"{candidate}: annotations not found (404)")
            else:
                errors.append(f"{candidate}: annotations fetch failed (HTTP {e.code})")
            continue
        except urllib.error.URLError as e:
            errors.append(f"{candidate}: annotations fetch failed ({e})")
            continue
        if not payload_is_json(ann_payload):
            errors.append(f"{candidate}: annotations payload is not valid JSON")
            continue

        try:
            der_payload = fetch_url_bytes(der_url)
        except (urllib.error.HTTPError, urllib.error.URLError):
            # derived annotations are optional; keep empty object when unavailable
            der_payload = b"{}\n"
        if not payload_is_json(der_payload):
            # derived annotations are optional; ignore invalid payloads.
            der_payload = b"{}\n"

        ann_path.write_bytes(ann_payload)
        der_path.write_bytes(der_payload)
        meta_path.write_text(
            json.dumps(
                {
                    "locale": locale,
                    "source_locale": candidate,
                    "annotations_url": ann_url,
                    "derived_url": der_url,
                },
                ensure_ascii=False,
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
        return ann_path, der_path, candidate

    if not errors:
        ann_path.write_text("{}\n", encoding="utf-8")
        der_path.write_text("{}\n", encoding="utf-8")
        meta_path.write_text(
            json.dumps(
                {
                    "locale": locale,
                    "source_locale": None,
                    "status": "not_found",
                    "details": not_found,
                },
                ensure_ascii=False,
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
        return ann_path, der_path, None

    tried = ", ".join(locale_candidates(locale))
    details = "; ".join(errors) if errors else "no matching locale payload found"
    raise RuntimeError(
        f"Failed to fetch CLDR annotation data for locale '{locale}' "
        f"(candidates: {tried}): {details}. "
        f"Run `just emoji-fetch` with network access to populate cache first."
    )


def supported_komai_locales(repo_root: pathlib.Path) -> list[str]:
    langs_dir = repo_root / "resources" / "langs"
    locales: list[str] = []
    for child in sorted(langs_dir.iterdir()):
        if child.is_dir():
            locales.append(child.name)
    return locales


def parse_shortcode_overrides(path: pathlib.Path) -> dict[str, str]:
    if not path.is_file():
        return {}

    overrides: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        key = key.strip()
        value = value.strip()
        if key and value:
            overrides[key] = value
    return overrides


def parse_emoji_test_files(
    *,
    unicode_file: pathlib.Path,
    extra_file: pathlib.Path,
    shortcode_overrides: dict[str, str],
) -> list[CoreEmoji]:
    entries: list[CoreEmoji] = []

    def parse_file(path: pathlib.Path, search_offset: int) -> int:
        current_group = ""
        current_subgroup = ""
        order = search_offset

        for raw in path.read_text(encoding="utf-8").splitlines():
            line = raw.strip()
            if line.startswith("# group:"):
                current_group = line.split(":", 1)[1].strip()
                continue
            if line.startswith("# subgroup:"):
                current_subgroup = line.split(":", 1)[1].strip()
                continue
            if not line or line.startswith("#"):
                continue

            parts = re.split(r"\s+[#;]\s+", line)
            if len(parts) != 3:
                continue
            codes, qualification, char_and_name = parts
            if qualification != "fully-qualified":
                continue

            m = re.match(r"^(\S+)\s+E\d+(?:\.\d+)?\s+(.*)$", char_and_name)
            if not m:
                continue
            emoji_char = m.group(1)
            unicode_name = m.group(2)

            category = GROUP_TO_CATEGORY.get(current_group)
            if category is None:
                continue

            codepoints = [cp.strip().upper() for cp in codes.split() if cp.strip()]
            if not codepoints:
                continue

            emoji_id = "-".join(codepoints)
            short_name = shortcode_overrides.get(codes, slugify_name(unicode_name))

            tone_modifiers = [cp for cp in codepoints if cp in SKIN_TONE_MODIFIERS]
            if not tone_modifiers:
                skin_tone_class = "none"
                base_id = None
            elif len(tone_modifiers) > 1:
                skin_tone_class = "multi"
                base_id = "-".join([cp for cp in codepoints if cp not in SKIN_TONE_MODIFIERS])
            else:
                skin_tone_class = SKIN_TONE_MODIFIER_TO_CLASS[tone_modifiers[0]]
                base_id = "-".join([cp for cp in codepoints if cp not in SKIN_TONE_MODIFIERS])

            entries.append(
                CoreEmoji(
                    id=emoji_id,
                    unicode=emoji_char,
                    codepoints=codepoints,
                    category=category,
                    subgroup=current_subgroup,
                    unicode_name=unicode_name,
                    short_name=short_name,
                    search_order=order,
                    skin_tone_class=skin_tone_class,
                    base_id=base_id if base_id else None,
                )
            )
            order += 1
        return order

    next_order = parse_file(extra_file, 0)
    parse_file(unicode_file, next_order)

    deduped: dict[str, CoreEmoji] = {}
    for entry in entries:
        # Keep first occurrence (extra_emoji first, then upstream)
        if entry.id not in deduped:
            deduped[entry.id] = entry

    ordered = list(deduped.values())
    cat_order = {name: i for i, name in enumerate(CATEGORY_ORDER)}
    ordered.sort(key=lambda e: (cat_order.get(e.category, 999), e.search_order))

    base_to_variants: dict[str, int] = {}
    for e in ordered:
        if e.base_id:
            base_to_variants[e.base_id] = base_to_variants.get(e.base_id, 0) + 1

    for e in ordered:
        e.has_skin_tone_variants = base_to_variants.get(e.id, 0) > 0 or bool(e.base_id)

    return ordered


def codepoint_id_from_emoji(emoji_text: str) -> str:
    return "-".join(f"{ord(ch):X}" for ch in emoji_text)


def extract_cldr_entries(node: t.Any, out: dict[str, dict[str, t.Any]]) -> None:
    if isinstance(node, dict):
        is_annotation_map = any(
            isinstance(k, str) and isinstance(v, dict) and ("default" in v or "tts" in v)
            for k, v in node.items()
        )
        if is_annotation_map:
            for cp, rec in node.items():
                if not isinstance(cp, str) or not isinstance(rec, dict):
                    continue

                tts_value = rec.get("tts")
                display = ""
                if isinstance(tts_value, str):
                    display = normalize_token(tts_value)
                elif isinstance(tts_value, dict):
                    raw = tts_value.get("_value")
                    if isinstance(raw, str):
                        display = normalize_token(raw)
                elif isinstance(tts_value, list):
                    for candidate in tts_value:
                        if isinstance(candidate, str) and normalize_token(candidate):
                            display = normalize_token(candidate)
                            break
                        if isinstance(candidate, dict) and isinstance(candidate.get("_value"), str):
                            val = normalize_token(candidate["_value"])
                            if val:
                                display = val
                                break

                default = rec.get("default")
                tokens: list[str] = []
                if isinstance(default, list):
                    for value in default:
                        if isinstance(value, str):
                            tokens.extend(split_cldr_tokens(value))
                        elif isinstance(value, dict) and isinstance(value.get("_value"), str):
                            tokens.extend(split_cldr_tokens(value["_value"]))
                elif isinstance(default, str):
                    tokens.extend(split_cldr_tokens(default))
                elif isinstance(default, dict) and isinstance(default.get("_value"), str):
                    tokens.extend(split_cldr_tokens(default["_value"]))

                out[cp] = {
                    "display_name": display,
                    "tokens": tokens,
                }
            return

        if isinstance(node.get("cp"), str):
            cp = node["cp"]
            display = node.get("tts")
            if isinstance(display, dict):
                display = display.get("_value")
            if not isinstance(display, str):
                display = ""

            default = node.get("default")
            tokens: list[str] = []
            if isinstance(default, list):
                for value in default:
                    if isinstance(value, str):
                        tokens.extend(split_cldr_tokens(value))
                    elif isinstance(value, dict) and isinstance(value.get("_value"), str):
                        tokens.extend(split_cldr_tokens(value["_value"]))
            elif isinstance(default, str):
                tokens.extend(split_cldr_tokens(default))
            elif isinstance(default, dict) and isinstance(default.get("_value"), str):
                tokens.extend(split_cldr_tokens(default["_value"]))

            out[cp] = {
                "display_name": display,
                "tokens": tokens,
            }

        for value in node.values():
            extract_cldr_entries(value, out)
    elif isinstance(node, list):
        for value in node:
            extract_cldr_entries(value, out)


def load_cldr_locale_annotations(ann_path: pathlib.Path, der_path: pathlib.Path) -> dict[str, dict[str, t.Any]]:
    merged: dict[str, dict[str, t.Any]] = {}

    for path in [ann_path, der_path]:
        if not path.is_file():
            continue
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            continue

        cp_map: dict[str, dict[str, t.Any]] = {}
        extract_cldr_entries(data, cp_map)
        for cp, payload in cp_map.items():
            current = merged.setdefault(cp, {"display_name": "", "tokens": []})
            if payload.get("display_name"):
                current["display_name"] = payload["display_name"]
            current_tokens = current.setdefault("tokens", [])
            current_tokens.extend(payload.get("tokens", []))

    by_id: dict[str, dict[str, t.Any]] = {}
    for cp, payload in merged.items():
        emoji_id = codepoint_id_from_emoji(cp)
        by_id[emoji_id] = {
            "display_name": payload.get("display_name", "") or "",
            "tokens": payload.get("tokens", []),
        }
    return by_id


def read_override_file(path: pathlib.Path, default_locale: str | None) -> list[dict[str, t.Any]]:
    if not path.is_file():
        return []

    data = load_yaml(path)
    items = data.get("emoji", [])
    if items is None:
        return []
    if not isinstance(items, list):
        raise ValueError(f"{path}: key 'emoji' must be a list")

    normalized: list[dict[str, t.Any]] = []
    for i, item in enumerate(items):
        if not isinstance(item, dict):
            raise ValueError(f"{path}: emoji[{i}] must be a mapping")

        emoji_id = str(item.get("id", "")).strip().upper()
        if not emoji_id:
            raise ValueError(f"{path}: emoji[{i}] is missing required 'id'")

        locale = str(item.get("locale") or default_locale or "").strip()
        if not locale:
            raise ValueError(f"{path}: emoji[{i}] is missing 'locale'")

        tokens_add_raw = item.get("tokens_add", [])
        if tokens_add_raw is None:
            tokens_add_raw = []
        if not isinstance(tokens_add_raw, list):
            raise ValueError(f"{path}: emoji[{i}].tokens_add must be a list")

        tokens_remove_raw = item.get("tokens_remove", [])
        if tokens_remove_raw is None:
            tokens_remove_raw = []
        if not isinstance(tokens_remove_raw, list):
            raise ValueError(f"{path}: emoji[{i}].tokens_remove must be a list")

        normalized.append(
            {
                "id": emoji_id,
                "locale": locale,
                "preview": str(item.get("preview", "")).strip(),
                "display_name": str(item.get("display_name", "")).strip(),
                "primary_token": str(item.get("primary_token", "")).strip(),
                "tokens_add": [
                    normalize_token(str(x))
                    for x in tokens_add_raw
                    if normalize_token(str(x))
                ],
                "tokens_remove": [
                    normalize_token(str(x))
                    for x in tokens_remove_raw
                    if normalize_token(str(x))
                ],
            }
        )

    return normalized


def load_overrides(repo_root: pathlib.Path) -> list[dict[str, t.Any]]:
    overrides_root = repo_root / "resources" / "emoji" / "overrides"
    locale_dir = overrides_root / "locale"

    all_entries: list[dict[str, t.Any]] = []

    global_file = overrides_root / "global.yml"
    all_entries.extend(read_override_file(global_file, None))

    if locale_dir.is_dir():
        for file in sorted(locale_dir.glob("*.yml")):
            all_entries.extend(read_override_file(file, file.stem))

    return all_entries


def validate_overrides(
    *,
    overrides: list[dict[str, t.Any]],
    locales: list[str],
    core_entries: list[CoreEmoji] | None,
) -> None:
    errors: list[str] = []
    supported_locales = set(locales)
    core_by_id = {e.id: e for e in core_entries} if core_entries is not None else {}

    seen_keys: set[tuple[str, str]] = set()
    for idx, item in enumerate(overrides):
        locale = str(item.get("locale", "")).strip()
        emoji_id = str(item.get("id", "")).strip().upper()
        label = f"override[{idx}] ({locale}:{emoji_id})"

        if locale not in supported_locales:
            errors.append(f"{label}: locale is not supported by Komai")

        key = (locale, emoji_id)
        if key in seen_keys:
            errors.append(f"{label}: duplicate override entry for same locale+id")
        else:
            seen_keys.add(key)

        add_set = {str(x).casefold() for x in item.get("tokens_add", [])}
        remove_set = {str(x).casefold() for x in item.get("tokens_remove", [])}
        overlap = sorted(add_set.intersection(remove_set))
        if overlap:
            errors.append(f"{label}: tokens_add and tokens_remove overlap: {', '.join(overlap)}")

        primary = normalize_token(str(item.get("primary_token", "")))
        if primary and primary.casefold() in remove_set:
            errors.append(f"{label}: primary_token must not be removed in tokens_remove")

        if core_entries is not None:
            core = core_by_id.get(emoji_id)
            if core is None:
                errors.append(f"{label}: id not found in parsed core emoji dataset")
                continue
            preview = str(item.get("preview", "")).strip()
            if preview and preview != core.unicode:
                errors.append(
                    f"{label}: preview mismatch, expected '{core.unicode}', got '{preview}'"
                )

    if errors:
        details = "\n".join(f"  - {msg}" for msg in errors[:40])
        suffix = "\n  - ..." if len(errors) > 40 else ""
        raise ValueError(f"Invalid emoji override entries:\n{details}{suffix}")


def apply_overrides_to_locale(
    *,
    locale: str,
    core_entries: list[CoreEmoji],
    locale_annotations: dict[str, dict[str, t.Any]],
    overrides: list[dict[str, t.Any]],
) -> dict[str, dict[str, t.Any]]:
    by_id: dict[str, dict[str, t.Any]] = {}

    for core in core_entries:
        tokens = [core.short_name, core.unicode_name]
        display_name = core.unicode_name
        primary_token = core.short_name

        cldr = locale_annotations.get(core.id, {})
        cldr_name = normalize_token(str(cldr.get("display_name", "")))
        if cldr_name:
            display_name = cldr_name
        cldr_tokens = [normalize_token(str(x)) for x in cldr.get("tokens", [])]
        tokens.extend([t for t in cldr_tokens if t])
        if cldr_name:
            tokens.append(cldr_name)

        merged: list[str] = []
        seen: set[str] = set()
        for token in tokens:
            token_norm = normalize_token(token)
            if not token_norm:
                continue
            folded = token_norm.casefold()
            if folded in seen:
                continue
            seen.add(folded)
            merged.append(token_norm)

        by_id[core.id] = {
            "display_name": display_name,
            "primary_token": primary_token,
            "tokens": merged,
        }

    for item in overrides:
        item_locale = item["locale"]
        apply_full_locale_override = item_locale == locale
        apply_en_baseline_override = item_locale == "en" and locale != "en"

        if not (apply_full_locale_override or apply_en_baseline_override):
            continue
        record = by_id.get(item["id"])
        if record is None:
            continue

        if apply_full_locale_override and item.get("display_name"):
            record["display_name"] = item["display_name"]

        tokens: list[str] = list(record.get("tokens", []))
        seen = {t.casefold() for t in tokens}

        for token in item.get("tokens_add", []):
            folded = token.casefold()
            if folded not in seen:
                tokens.append(token)
                seen.add(folded)

        remove_set = {x.casefold() for x in item.get("tokens_remove", [])}
        if remove_set:
            tokens = [t for t in tokens if t.casefold() not in remove_set]

        if apply_full_locale_override:
            primary_token = item.get("primary_token") or record.get("primary_token") or ""
            if primary_token:
                primary_token = normalize_token(str(primary_token))
                if primary_token and primary_token.casefold() not in {t.casefold() for t in tokens}:
                    tokens.insert(0, primary_token)
        else:
            # "en" overrides are a baseline for all locales: aliases/tokens are shared, but
            # locale-specific display name and primary token remain locale-owned.
            primary_token = record.get("primary_token") or ""
            if primary_token:
                primary_token = normalize_token(str(primary_token))

        record["tokens"] = tokens
        record["primary_token"] = primary_token or (tokens[0] if tokens else "")

    return by_id


def write_json(path: pathlib.Path, data: t.Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def generate_runtime_data(
    *,
    repo_root: pathlib.Path,
    lock_path: pathlib.Path,
    lock: dict[str, t.Any],
    out_dir: pathlib.Path,
    force_fetch: bool,
) -> dict[str, t.Any]:
    cache_root = repo_root / "var" / "emoji" / "cache" / lock_hash(lock_path)

    unicode_file = ensure_unicode_data(
        lock=lock,
        cache_dir=cache_root,
        force=force_fetch,
    )

    locales = supported_komai_locales(repo_root)
    cldr_meta: dict[str, str | None] = {}
    locale_ann_paths: dict[str, tuple[pathlib.Path, pathlib.Path]] = {}

    for locale in locales:
        ann, der, source_locale = ensure_cldr_locale_data(
            lock=lock,
            cache_dir=cache_root,
            locale=locale,
            force=force_fetch,
        )
        locale_ann_paths[locale] = (ann, der)
        cldr_meta[locale] = source_locale

    shortcodes = parse_shortcode_overrides(repo_root / "resources" / "shortcodes.txt")
    core_entries = parse_emoji_test_files(
        unicode_file=unicode_file,
        extra_file=repo_root / "resources" / "extra_emoji.txt",
        shortcode_overrides=shortcodes,
    )

    overrides = load_overrides(repo_root)
    validate_overrides(
        overrides=overrides,
        locales=locales,
        core_entries=core_entries,
    )

    core_data = {
        "version": 1,
        "lock_hash": lock_hash(lock_path),
        "emoji": [
            {
                "id": e.id,
                "unicode": e.unicode,
                "codepoints": e.codepoints,
                "category": e.category,
                "subgroup": e.subgroup,
                "unicode_name": e.unicode_name,
                "short_name": e.short_name,
                "search_order": e.search_order,
                "skin_tone_class": e.skin_tone_class,
                "base_id": e.base_id,
                "has_skin_tone_variants": e.has_skin_tone_variants,
            }
            for e in core_entries
        ],
    }
    write_json(out_dir / "core.json", core_data)

    for locale in locales:
        ann_path, der_path = locale_ann_paths[locale]
        locale_annotations = load_cldr_locale_annotations(ann_path, der_path)
        locale_data = apply_overrides_to_locale(
            locale=locale,
            core_entries=core_entries,
            locale_annotations=locale_annotations,
            overrides=overrides,
        )

        write_json(
            out_dir / "locale" / f"{locale}.json",
            {
                "version": 1,
                "locale": locale,
                "emoji": locale_data,
            },
        )

    manifest = {
        "version": 1,
        "lock_hash": lock_hash(lock_path),
        "cache_root": str(cache_root),
        "locales": locales,
        "cldr_source_locale": cldr_meta,
        "emoji_count": len(core_entries),
    }
    write_json(out_dir / "manifest.json", manifest)
    return manifest


def cmd_fetch(args: argparse.Namespace) -> int:
    repo_root = repo_root_from_arg(args.repo_root)
    lock_path = repo_root / "bin" / "emoji" / "sources.lock.yml"
    lock = load_yaml(lock_path)

    cache_root = repo_root / "var" / "emoji" / "cache" / lock_hash(lock_path)
    ensure_unicode_data(
        lock=lock,
        cache_dir=cache_root,
        force=args.force,
    )

    for locale in supported_komai_locales(repo_root):
        ensure_cldr_locale_data(
            lock=lock,
            cache_dir=cache_root,
            locale=locale,
            force=args.force,
        )

    print(f"Fetched/validated emoji upstream cache: {cache_root}")
    return 0


def cmd_build(args: argparse.Namespace) -> int:
    repo_root = repo_root_from_arg(args.repo_root)
    lock_path = repo_root / "bin" / "emoji" / "sources.lock.yml"
    lock = load_yaml(lock_path)

    out_dir = pathlib.Path(args.out_dir).resolve() if args.out_dir else (
        repo_root / "var" / "emoji" / "generated" / lock_hash(lock_path)
    )

    out_dir.mkdir(parents=True, exist_ok=True)

    manifest = generate_runtime_data(
        repo_root=repo_root,
        lock_path=lock_path,
        lock=lock,
        out_dir=out_dir,
        force_fetch=args.force_fetch,
    )

    print(
        f"Built emoji runtime data in {out_dir} "
        f"(emoji={manifest['emoji_count']}, locales={len(manifest['locales'])})."
    )
    return 0


def cmd_check(args: argparse.Namespace) -> int:
    repo_root = repo_root_from_arg(args.repo_root)
    lock_path = repo_root / "bin" / "emoji" / "sources.lock.yml"
    load_yaml(lock_path)

    # Validate override schema by attempting to parse all files.
    locales = supported_komai_locales(repo_root)
    overrides = load_overrides(repo_root)
    validate_overrides(overrides=overrides, locales=locales, core_entries=None)

    # If cache exists for this lock, run a local offline build test into temp dir.
    cache_root = repo_root / "var" / "emoji" / "cache" / lock_hash(lock_path)
    if not cache_root.exists():
        msg = f"emoji cache not found for lock hash ({cache_root}); run emoji-fetch/emoji-build first."
        if args.strict:
            print(f"ERROR: {msg}", file=sys.stderr)
            return 1
        print(f"WARNING: {msg}", file=sys.stderr)
        return 0

    with tempfile.TemporaryDirectory(prefix="komai-emoji-check-") as tmp:
        rc = cmd_build(
            argparse.Namespace(
                repo_root=str(repo_root),
                out_dir=tmp,
                force_fetch=False,
            )
        )
        if rc != 0:
            return rc

    print("Emoji check passed.")
    return 0


def cmd_add_token(args: argparse.Namespace) -> int:
    repo_root = repo_root_from_arg(args.repo_root)
    locale = args.locale
    token = normalize_token(args.token)
    if not token:
        raise SystemExit("token cannot be empty")

    emoji_input = args.emoji
    if re.fullmatch(r"[0-9A-Fa-f]+(?:-[0-9A-Fa-f]+)*", emoji_input):
        emoji_id = emoji_input.upper()
        preview = ""
    else:
        emoji_id = codepoint_id_from_emoji(emoji_input)
        preview = emoji_input

    override_file = repo_root / "resources" / "emoji" / "overrides" / "locale" / f"{locale}.yml"
    data = load_yaml(override_file) if override_file.exists() else {"version": 1, "emoji": []}

    if "emoji" not in data or not isinstance(data["emoji"], list):
        data["emoji"] = []

    existing = None
    for item in data["emoji"]:
        if isinstance(item, dict) and str(item.get("id", "")).upper() == emoji_id:
            existing = item
            break

    if existing is None:
        existing = {
            "id": emoji_id,
            "preview": preview,
            "locale": locale,
            "tokens_add": [],
        }
        data["emoji"].append(existing)

    existing.setdefault("tokens_add", [])
    tokens_add = [normalize_token(str(x)) for x in existing.get("tokens_add", []) if normalize_token(str(x))]
    if token.casefold() not in {t.casefold() for t in tokens_add}:
        tokens_add.append(token)
    existing["tokens_add"] = tokens_add

    if preview and not existing.get("preview"):
        existing["preview"] = preview
    if locale and not existing.get("locale"):
        existing["locale"] = locale

    dump_yaml(override_file, data)
    print(f"Updated {override_file}: + token '{token}' for {emoji_id}")
    return 0


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Komai emoji pipeline")
    sub = parser.add_subparsers(dest="cmd", required=True)

    fetch = sub.add_parser("fetch", help="Fetch upstream Unicode/CLDR data into var/emoji cache")
    fetch.add_argument("--repo-root", help="Repository root")
    fetch.add_argument("--force", action="store_true", help="Force re-download")
    fetch.set_defaults(func=cmd_fetch)

    build = sub.add_parser("build", help="Build runtime emoji data JSON files")
    build.add_argument("--repo-root", help="Repository root")
    build.add_argument("--out-dir", help="Output directory (defaults to var/emoji/generated/<lock-hash>)")
    build.add_argument(
        "--force-fetch",
        action="store_true",
        help="Force upstream re-fetch before building",
    )
    build.set_defaults(func=cmd_build)

    check = sub.add_parser("check", help="Validate lock/overrides and offline-build from cache")
    check.add_argument("--repo-root", help="Repository root")
    check.add_argument("--strict", action="store_true", help="Fail when cache missing")
    check.set_defaults(func=cmd_check)

    add_token = sub.add_parser("add-token", help="Add one search token override for locale")
    add_token.add_argument("emoji", help="Emoji glyph (e.g. 🥃) or emoji id (e.g. 1F943)")
    add_token.add_argument("locale", help="Locale code, e.g. bg")
    add_token.add_argument("token", help="Search token to add")
    add_token.add_argument("--repo-root", help="Repository root")
    add_token.set_defaults(func=cmd_add_token)

    return parser


def main() -> int:
    parser = build_arg_parser()
    args = parser.parse_args()

    try:
        return int(args.func(args))
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
