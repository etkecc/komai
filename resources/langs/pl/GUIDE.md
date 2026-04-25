# Polish translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register: informal ("ty")

Address the user with **ty / ciebie / tobie / twój** — not formal "Pan/Pani". The existing translations use informal address ("poprosił(a) cię..."), which matches modern FOSS conventions.

Existing strings often use **impersonal** forms ("Nie udało się...", "Zaproszenie %1 do %2 nie powiodło się") — prefer those where possible.

## Matrix / chat vocabulary

| English | Polish |
|---|---|
| room | **pokój** (pl. **pokoje**) |
| space | **przestrzeń** |
| thread | **wątek** |
| direct message, DM | **wiadomość bezpośrednia** |
| invite (verb / noun) | **zaprosić** / **zaproszenie** |
| join (a room) | **dołączyć do** |
| leave (a room) | **opuścić** |
| redact (= delete a message) | **zredagować** / **usunąć** (existing translations use "redagowanie" for redact) |
| encryption | **szyfrowanie** |
| encrypted | **zaszyfrowana/y/e** |
| verify / verification / verified (E2EE) | **weryfikować** / **weryfikacja** / **zweryfikowana/y** |
| user | **użytkownik** (m.) / **użytkowniczka** (f.) |
| message | **wiadomość** |
| device | **urządzenie** |

## Typography

- For quotes, follow the common GUIDE's rule (prefer ASCII in JSON); Polish's typographic convention is `„…"` if you do use them.
- Use the **horizontal ellipsis `…`** (U+2026) where possible.
- Buttons/labels: use infinitive ("Zapisz", "Anuluj", "Wyślij").
- Inclusive forms like "poprosił(a)" appear in the existing translation — use when the referent's gender is unknown.

## Plural forms

Polish uses **3 plural forms**, in CLDR canonical order:

1. **one** — count = 1 only
2. **few** — n%10 ∈ {2, 3, 4} and n%100 ∉ {12, 13, 14} (e.g., 2-4, 22-24, …)
3. **many** — everything else, including 0 (e.g., 0, 5-21, 25-31, …)

Each form must preserve `%n`. Example: `%n member(s)` → `["%n członek", "%n członkowie", "%n członków"]`.
