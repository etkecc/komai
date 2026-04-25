# Romanian translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register

Prefer **impersonal constructions** ("Nu s-a putut alătura la cameră", "Utilizator eliminat") — matching existing translations. When direct address is unavoidable, use **informal "tu"** (modern FOSS convention) — not the formal "dumneavoastră".

## Matrix / chat vocabulary

| English | Romanian |
|---|---|
| room | **cameră** (pl. **camere**) |
| space | **spațiu** |
| thread | **fir** |
| direct message, DM | **mesaj direct** |
| invite (verb / noun) | **a invita** / **invitație** |
| join (a room) | **a se alătura** |
| leave (a room) | **a părăsi** |
| redact (= delete a message) | **a șterge** / **a redacta** |
| encryption | **criptare** |
| encrypted | **criptat/ă** |
| verify / verification / verified (E2EE) | **a verifica** / **verificare** / **verificat/ă** |
| user | **utilizator/utilizatoare** |
| message | **mesaj** |
| device | **dispozitiv** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026) where possible.
- For quotes, follow the common GUIDE's rule (prefer ASCII in JSON); Romanian's typographic convention is `„…"` if you do use them.
- Use **ș** (s-comma) and **ț** (t-comma), **not** **ş** (s-cedilla) or **ţ** (t-cedilla) — the cedilla forms are a legacy encoding mistake.
- Buttons/labels: use infinitive with "a" ("Salvează" imperative is common, or "Salvare" noun).

## Plural forms

Romanian uses **3 plural forms**, in CLDR canonical order:

1. **one** — count = 1
2. **few** — count = 0, or n%100 ∈ {1..19} when count ≠ 1 (e.g., 0, 2-19, 101-119, 201-219, …)
3. **other** — everything else (e.g., 20-100, 120-200, …)

Each form must preserve `%n`. Example: `%n member(s)` → `["%n membru", "%n membri", "%n de membri"]`.
