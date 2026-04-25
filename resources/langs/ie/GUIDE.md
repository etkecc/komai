# Interlingue translation instructions

These supplement the common rules. No prior translations exist to mine — translate fresh.

**Important**: this is **Interlingue** (language code `ie`, also known as **Occidental**), a constructed auxiliary language created by Edgar de Wahl in 1922. It is **distinct from Interlingua (`ia`)** and **not Irish (`ga`)**. Interlingue aims for naturalistic, Romance-based vocabulary recognizable to European speakers. If you cannot produce confident Interlingue translations, translate literally from English using Romance cognates and flag uncertain strings.

## Register

Interlingue has no T-V distinction; use **"vu"** as the universal second-person pronoun.

## Matrix / chat vocabulary (reasonable Interlingue forms)

| English | Interlingue |
|---|---|
| room | **chambre** / **sala** |
| space | **spacie** |
| thread | **filament** / **thread** |
| direct message, DM | **mesage direct** |
| invite (verb / noun) | **invitar** / **invitation** |
| join | **juntar se** / **partiсipar** |
| leave | **departer** / **sortir** |
| redact (= delete a message) | **deleter** / **supresser** |
| encryption | **cripteration** |
| encrypted | **criptat** |
| verify / verification / verified (E2EE) | **verificar** / **verification** / **verificat** |
| user | **usator** |
| message | **mesage** |
| device | **aparate** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026), not `...`.
- Buttons/labels: use infinitive (Interlingue infinitive ends in `-ar`/`-er`/`-ir`).

## Plural forms

This language uses **2 plural forms**, in CLDR canonical order:

1. **one** — count = 1
2. **other** — every other integer count, including 0

Each form must preserve `%n`. Example: `%n member(s)` → `["%n membre", "%n membres"]`.
