# Czech translation instructions

These supplement the common rules. No prior translations exist to mine — translate fresh, following modern Czech UI conventions.

## Register

Prefer **impersonal constructions** where possible. When direct address is unavoidable, use **informal "ty"** (modern FOSS convention) — not formal "vy/Vy".

## Matrix / chat vocabulary

| English | Czech |
|---|---|
| room | **místnost** (pl. **místnosti**) |
| space | **prostor** |
| thread | **vlákno** |
| direct message, DM | **přímá zpráva** |
| invite (verb / noun) | **pozvat** / **pozvánka** |
| join (a room) | **připojit se k** |
| leave (a room) | **opustit** |
| redact (= delete a message) | **smazat** / **odstranit** |
| encryption | **šifrování** |
| encrypted | **šifrovaný/á/é** |
| verify / verification / verified (E2EE) | **ověřit** / **ověření** / **ověřený/á** |
| user | **uživatel/ka** |
| message | **zpráva** |
| device | **zařízení** |

## Typography

- For quotes, follow the common GUIDE's rule (prefer ASCII in JSON); Czech's typographic convention is `„…"` if you do use them.
- Use the **horizontal ellipsis `…`** (U+2026), not `...`.
- Buttons/labels: use imperative/infinitive ("Uložit", "Zrušit", "Odeslat").
- Preserve diacritics precisely (á č ď é ě í ň ó ř š ť ú ů ý ž).

## Plural forms

Czech uses **3 plural forms**, in CLDR canonical order:

1. **one** — count = 1
2. **few** — count ∈ {2, 3, 4}
3. **other** — everything else for integer counts, including 0 (e.g., 0, 5, 6, …)

Each form must preserve `%n`. Example: `%n member(s)` → `["%n člen", "%n členové", "%n členů"]`.
