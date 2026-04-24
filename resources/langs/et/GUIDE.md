# Estonian translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register

Prefer **impersonal constructions** — "See sõnum on krüptimata!", "Jututoaga liitumine ei õnnestunud" — matching the existing translations. When direct address is unavoidable, use **informal "sina"** (modern FOSS convention).

## Matrix / chat vocabulary

| English | Estonian |
|---|---|
| room | **jututuba** (pl. **jututoad**) — Estonian Matrix convention; literally "chat room" |
| space | **ruum** |
| thread | **lõim** |
| direct message, DM | **otsesõnum** |
| invite (verb / noun) | **kutsuda** / **kutse** |
| join (a room) | **liituda** |
| leave (a room) | **lahkuda** |
| redact (= delete a message) | **muuta** / **kustutada** (the existing translations use "muutmine" for redact; follow that) |
| encryption | **krüptimine** |
| encrypted | **krüptitud** |
| verify / verification / verified (E2EE) | **verifitseerida** / **verifitseerimine** / **verifitseeritud** |
| user | **kasutaja** |
| message | **sõnum** |
| device | **seade** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026) where possible.
- Prefer ASCII `"..."` in translations — the pipeline emits JSON, and mixing typographic `„` with ASCII `"` inside the same string breaks the JSON envelope. If you do use typographic „…", both the opening `„` and closing `"` (U+201C) must be typographic — never mix with ASCII `"`.
- Buttons/labels: use noun or `-ma` infinitive ("Salvesta", "Tühista", "Saada").
