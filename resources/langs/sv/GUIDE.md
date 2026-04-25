# Swedish translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register: informal ("du")

Swedish has effectively abandoned the T-V distinction since the 1960s "du-reform". Use **"du"** universally — formal "ni" is outdated and sounds wrong in modern UI.

The existing translations are mostly impersonal ("Kunde inte gå med i rum"); follow that where possible.

## Matrix / chat vocabulary

| English | Swedish |
|---|---|
| room | **rum** (pl. same) |
| space | **område** / **utrymme** |
| thread | **tråd** |
| direct message, DM | **direktmeddelande** |
| invite (verb / noun) | **bjuda in** / **inbjudan** |
| join (a room) | **gå med i** |
| leave (a room) | **lämna** |
| redact (= delete a message) | **radera** / **ta bort** |
| encryption | **kryptering** |
| encrypted | **krypterad/t/e** |
| verify / verification / verified (E2EE) | **verifiera** / **verifiering** / **verifierad** |
| user | **användare** |
| message | **meddelande** |
| device | **enhet** |

## Typography

- The existing translations use ASCII `...` for ellipsis; `…` (U+2026) is also acceptable.
- Buttons/labels: imperative ("Spara", "Avbryt", "Skicka").

## Plural forms

This language uses **2 plural forms**, in CLDR canonical order:

1. **one** — count = 1
2. **other** — every other integer count, including 0

Each form must preserve `%n`. Example: `%n member(s)` → `["%n medlem", "%n medlemmar"]`.
