# Catalan translation instructions

These supplement the common rules. No prior translations exist to mine — translate fresh, following modern Catalan UI conventions.

## Register

Prefer **impersonal constructions** where possible. When direct address is unavoidable, use **informal "tu"** (modern Catalan UI convention).

## Matrix / chat vocabulary

| English | Catalan |
|---|---|
| room | **sala** (pl. **sales**) |
| space | **espai** |
| thread | **fil** |
| direct message, DM | **missatge directe** |
| invite (verb / noun) | **convidar** / **invitació** |
| join (a room) | **unir-se a** |
| leave (a room) | **sortir de** |
| redact (= delete a message) | **suprimir** / **eliminar** |
| encryption | **xifratge** |
| encrypted | **xifrat/da** |
| verify / verification / verified (E2EE) | **verificar** / **verificació** / **verificat/da** |
| user | **usuari/ària** |
| message | **missatge** |
| device | **dispositiu** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026), not `...`.
- For quotes, follow the common GUIDE's rule (prefer ASCII in JSON); Catalan's typographic convention is `«…»` if you do use them.
- Use the **interpunct `·`** (U+00B7) in `l·l` geminates (e.g., `il·lustració`) — this is standard Catalan, not a typo.
- Buttons/labels: use infinitive ("Desa", "Cancel·la", "Envia") or noun forms.

## Plural forms

This language uses **2 plural forms**, in CLDR canonical order:

1. **one** — count = 1
2. **other** — every other integer count, including 0

Each form must preserve `%n`. Example: `%n member(s)` → `["%n membre", "%n membres"]`.
