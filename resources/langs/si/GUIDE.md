# Sinhala translation instructions

These supplement the common rules. No prior translations exist to mine — translate fresh.

## Register

Use respectful/neutral register. Prefer impersonal constructions. When direct address is needed, use **ඔබ** (polite/neutral you).

## Matrix / chat vocabulary

| English | Sinhala |
|---|---|
| room | **කාමරය** |
| space | **අවකාශය** |
| thread | **නූල** |
| direct message, DM | **සෘජු පණිවිඩය** |
| invite (verb / noun) | **ආරාධනා කරන්න** / **ආරාධනාව** |
| join (a room) | **සම්බන්ධ වන්න** |
| leave (a room) | **පිටවන්න** |
| redact (= delete a message) | **මකන්න** |
| encryption | **සංකේතනය** |
| encrypted | **සංකේතිත** |
| verify / verification / verified (E2EE) | **තහවුරු කරන්න** / **තහවුරු කිරීම** / **තහවුරු කළ** |
| user | **පරිශීලක** |
| message | **පණිවිඩය** |
| device | **උපකරණය** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026), not `...`.
- Sinhala uses ASCII punctuation `. , ? !`.
- Technical loanwords are acceptable when Sinhala equivalents would be unclear in UI context.

## Plural forms

This language uses **2 plural forms**, in CLDR canonical order:

1. **one** — count is **0 or 1**
2. **other** — every other integer count

Each form must preserve `%n`. Example: `%n member(s)` → `["%n සාමාජිකයෙක්", "%n සාමාජිකයන්"]`.
