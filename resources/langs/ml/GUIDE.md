# Malayalam translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register

Use respectful/neutral register. The existing translations favour impersonal constructions — match where possible.

## Matrix / chat vocabulary

| English | Malayalam |
|---|---|
| room | **മുറി** (the existing translations use this) |
| space | **സ്പേസ്** |
| thread | **ത്രെഡ്** |
| direct message, DM | **നേരിട്ടുള്ള സന്ദേശം** |
| invite (verb / noun) | **ക്ഷണിക്കുക** / **ക്ഷണം** |
| join (a room) | **ചേരുക** |
| leave (a room) | **വിട്ടുപോകുക** |
| redact (= delete a message) | **നീക്കം ചെയ്യുക** |
| encryption | **എൻക്രിപ്ഷൻ** |
| encrypted | **എൻക്രിപ്റ്റഡ്** |
| verify / verification / verified (E2EE) | **പരിശോധിക്കുക** / **പരിശോധന** / **പരിശോധിച്ച** |
| user | **ഉപയോക്താവ്** (the existing translations use this) |
| message | **സന്ദേശം** |
| device | **ഉപകരണം** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026) where possible.
- Technical loanwords in common use (e.g., ത്രെഡ്, എൻക്രിപ്ഷൻ) are acceptable transliterations; prefer native terms when they are widely understood.

## Plural forms

This language uses **2 plural forms**, in CLDR canonical order:

1. **one** — count = 1
2. **other** — every other integer count, including 0

Each form must preserve `%n`. Example: `%n member(s)` → `["%n അംഗം", "%n അംഗങ്ങൾ"]`.
