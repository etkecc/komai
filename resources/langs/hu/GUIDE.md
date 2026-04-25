# Hungarian translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register

Prefer **impersonal constructions** ("Ez az üzenet nincs titkosítva!", "Nem sikerült...") — matching the existing translations. When direct address is unavoidable, use **formal "Ön"** (Hungarian UI convention) or the universal "te" (informal) — but match existing strings.

## Matrix / chat vocabulary

| English | Hungarian |
|---|---|
| room | **szoba** (pl. **szobák**) |
| space | **tér** |
| thread | **szál** |
| direct message, DM | **közvetlen üzenet** |
| invite (verb / noun) | **meghívni** / **meghívás** |
| join (a room) | **csatlakozni** |
| leave (a room) | **elhagyni** |
| redact (= delete a message) | **törölni** |
| encryption | **titkosítás** |
| encrypted | **titkosított** |
| verify / verification / verified (E2EE) | **hitelesíteni** / **hitelesítés** / **hitelesített** |
| user | **felhasználó** |
| message | **üzenet** |
| device | **eszköz** |

## Typography

- For quotes, follow the common GUIDE's rule (prefer ASCII in JSON); Hungarian's typographic convention is `„…"` if you do use them.
- The existing translations use ASCII `...` for ellipsis — continue that, or use `…` (U+2026) if the source uses it.
- Buttons/labels: imperative ("Mentés", "Mégsem", "Küldés").
- Hungarian uses **vowel-harmony article forms** (`a` / `az`); match the following word's initial sound correctly (common mistake).

## Plural forms

This language uses **a single plural form** for all counts. When a `numerus` source like `%n member(s)` is presented in the plural-form pass, return one translation that works grammatically regardless of count.

The form must preserve `%n`. Example: `%n member(s)` → `["%n tag"]`.
