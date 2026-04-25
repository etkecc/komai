# Dutch translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register: informal ("je/jij")

Address the user with **je / jij / jou / jouw** — not formal "u". This matches modern FOSS conventions. The existing translations are mostly impersonal; follow suit where possible.

## Matrix / chat vocabulary

| English | Dutch |
|---|---|
| room | **kamer** (pl. **kamers**) |
| space | **ruimte** |
| thread | **thread** or **draadje** |
| direct message, DM | **direct bericht** |
| invite (verb / noun) | **uitnodigen** / **uitnodiging** |
| join (a room) | **binnengaan** |
| leave (a room) | **verlaten** |
| redact (= delete a message) | **intrekken** (the existing translation uses this for message deletion; Matrix-style) |
| encryption | **versleuteling** |
| encrypted | **versleuteld** |
| verify / verification / verified (E2EE) | **verifiëren** / **verificatie** / **geverifieerd** |
| user | **gebruiker** |
| message | **bericht** |
| device | **apparaat** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026) where the source uses `...`.
- Buttons/labels: use infinitive ("Opslaan", "Annuleren", "Verzenden").
- Capitalisation: sentence case (first word only), matching the source.

## Plural forms

This language uses **2 plural forms**, in CLDR canonical order:

1. **one** — count = 1
2. **other** — every other integer count, including 0

Each form must preserve `%n`. Example: `%n member(s)` → `["%n deelnemer", "%n deelnemers"]`.
