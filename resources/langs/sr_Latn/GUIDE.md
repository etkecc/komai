# Serbian (Latin script) translation instructions

These supplement the common rules. No prior translations exist to mine — translate fresh.

**Important**: this is **Serbian written in Latin script** (`sr_Latn`), not Cyrillic. Use Latin letters exclusively: š, đ, č, ć, ž — not Cyrillic equivalents.

## Register

Prefer **impersonal constructions** where possible. When direct address is unavoidable, use **informal "ti"** (modern FOSS convention) — not formal "Vi".

## Matrix / chat vocabulary

| English | Serbian (Latin) |
|---|---|
| room | **soba** (pl. **sobe**) |
| space | **prostor** |
| thread | **nit** |
| direct message, DM | **direktna poruka** |
| invite (verb / noun) | **pozvati** / **poziv** |
| join (a room) | **pridružiti se** |
| leave (a room) | **napustiti** |
| redact (= delete a message) | **obrisati** / **ukloniti** |
| encryption | **šifrovanje** |
| encrypted | **šifrovan/a/o** |
| verify / verification / verified (E2EE) | **verifikovati** / **verifikacija** / **verifikovan/a** |
| user | **korisnik/korisnica** |
| message | **poruka** |
| device | **uređaj** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026), not `...`.
- For quotes, follow the common GUIDE's rule (prefer ASCII in JSON); Serbian's typographic convention is `„…"` if you do use them.
- Buttons/labels: use infinitive ("Sačuvaj", "Otkaži", "Pošalji").

## Plural forms

Serbian uses **3 plural forms**, in CLDR canonical order:

1. **one** — n%10 = 1 and n%100 ≠ 11 (e.g., 1, 21, 31, …)
2. **few** — n%10 ∈ {2, 3, 4} and n%100 ∉ {12, 13, 14} (e.g., 2-4, 22-24, …)
3. **other** — everything else, including 0 (e.g., 0, 5-20, 25-30, …)

Each form must preserve `%n`. Example: `%n member(s)` → `["%n član", "%n člana", "%n članova"]`.
