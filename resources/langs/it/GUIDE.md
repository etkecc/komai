# Italian translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register

Prefer **impersonal / passive constructions** ("Impossibile accedere a...", "Espulso utente: %1") where possible — this matches the existing translations. When direct address is needed, use **informal "tu"** (modern FOSS convention in Italian).

## Matrix / chat vocabulary

| English | Italian |
|---|---|
| room | **stanza** (pl. **stanze**) |
| space | **spazio** |
| thread | **thread** (commonly used untranslated) or **discussione** |
| direct message, DM | **messaggio diretto** |
| invite (verb / noun) | **invitare** / **invito** |
| join (a room) | **accedere a** / **entrare in** |
| leave (a room) | **uscire da** |
| redact (= delete a message) | **eliminare** |
| encryption | **crittografia** |
| encrypted | **crittato/a** (existing translations; alternative: **cifrato/a**) |
| verify / verification / verified (E2EE) | **verificare** / **verifica** / **verificato/a** |
| user | **utente** |
| message | **messaggio** |
| device | **dispositivo** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026), not `...`.
- Buttons/labels: use infinitive ("Salva", "Annulla", "Invia").
- Capitalisation: sentence case (first word of a sentence/label only).

## Plural forms

This language uses **2 plural forms**, in CLDR canonical order:

1. **one** — count = 1
2. **other** — every other integer count, including 0

Each form must preserve `%n`. Example: `%n member(s)` → `["%n membro", "%n membri"]`.
