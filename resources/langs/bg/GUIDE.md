# Bulgarian translation instructions

These supplement the common rules. No prior translations exist to mine — translate fresh, following modern Bulgarian UI conventions.

## Register

Prefer **impersonal constructions** where possible ("Не може да се установи връзка…", "Съобщението не е шифровано!"). When direct address is unavoidable, use **informal "ти"** (modern FOSS convention) — never formal "Вие".

## Matrix / chat vocabulary

Use these consistently — do not switch between synonyms from one string to the next:

| English | Bulgarian |
|---|---|
| room | **стая** (pl. **стаи**) |
| space | **пространство** |
| thread | **тред** |
| direct message, DM | **директно съобщение** |
| invite (verb / noun) | **поканя** / **покана** |
| join (a room) | **вляза в** |
| leave (a room) | **напусна** |
| knock (request access to a private room) | **почукам** |
| ban (verb) | **банна** |
| unban (verb) | **разбанна** |
| reaction | **реакция** |
| redact (= delete a message) | **изтрия** |
| encryption | **шифроване** |
| encrypted | **шифрован/а/о** |
| verify / verification / verified (E2EE) | **верифицирам** / **верификация** / **верифициран/а/о** |
| user | **потребител** |
| message | **съобщение** |
| server | **сървър** |
| device | **устройство** |
| avatar | **аватар** |

**"Verify" is a security operation** in E2EE/device contexts (cryptographic check), not a generic "confirm". Use **верифицирам / верификация** consistently — do not substitute **потвърдя** ("confirm") or **проверя** ("check"), even though both might read more naturally in isolation. The distinction matters: a button that triggers a key-verification flow is not the same as a button that confirms a dialog.

**Verb-form gotchas:**

- "join / enter": use **влезе**, **влезеш**. Not **влезне**, **влезнеш**. Same for "exit / leave": **излезе**, **излезеш**, not **излезне**, **излезнеш**.
- "was banned / unbanned": use **беше баннат/а** / **беше разбаннат/а**. Not **беше банен/а** / **беше разбанен/а**.

Examples: `%1 joined the room` → `%1 влезе в стаята`. `Invited you to join this room` → `Покани те да влезеш в тази стая`. `%1 was banned` → `%1 беше баннат/а`.

**"Thread" stays as тред**, the term Bulgarian users encounter in Slack / Discord / Twitter / Reddit. Do not substitute **тема** (which collides with Matrix's "room topic" — a separate, separately-translated feature) or **нишка** (computing-thread / sewing-thread connotation).

## Untranslatable terms (keep in Latin script)

In addition to the common GUIDE's list ("Matrix", "Komai", "Element", "Nheko", and IDs/aliases), do **not** transliterate Matrix/protocol proper nouns into Cyrillic. Write **Matrix** — never **Матрикс** / **Матрица**. Same for other client / project names that appear (Element X, Cinny, Synapse, Dendrite, etc.) and packaging-format names (Flatpak, AppImage, Snap).

**`Homeserver`** also stays in Latin — do **not** translate to **домашен сървър** / **домашни сървъри**.

The term takes Bulgarian definite articles and plural endings via a **hyphen suffix** when the grammar requires them — this is the standard Bulgarian convention for borrowed tech terms (cf. `URL-ът`, `API-то`):

| Form | Example |
|---|---|
| Singular indef | `Влезни на конкретен homeserver` (a specific homeserver) |
| Singular def, subj | `Homeserver-ът поддържа федерация` (the homeserver supports federation) |
| Singular def, obl | `URL на homeserver-а`, `от homeserver-а`, `на homeserver-а` |
| Plural indef | `разрешени и блокирани homeserver-и` |
| Plural def | `Homeserver-ите се свързват чрез федерация` |

Bare labels (e.g. `Homeserver`, `Homeserver: %1`) stay undeclined. Do **not** use the bare English plural `Homeservers` mid-sentence — use `homeserver-и` (indef) or `Homeserver-ите` (def).

## Style

- **Buttons / short labels** — **mixed convention**:
  - **Imperative for affirmative actions**: "Запази", "Изпрати", "Изтрий", "Покани", "Влез", "Присъедини се".
  - **Verbal noun for "Cancel" / negative-axis labels**: "Отказ" (not "Откажи"), "Затваряне" where contextually appropriate.
  - When in doubt for a positive action, use the imperative.
- **Sentence case** for labels and titles (Bulgarian convention) — only the first word and proper nouns are capitalised. "Настройки на стаята", not "Настройки На Стаята".
- Use the **horizontal ellipsis `…`** (U+2026), not three dots `...`. Example: `"Calling..."` → `"Свързване …"`.
- Preserve Bulgarian diacritics and the letter **ѝ** (the stressed feminine pronoun, U+045D) where grammatically required — do not replace it with **й**.

## Typography

- **Quotes**: prefer ASCII `"…"` in translations — the pipeline emits JSON, and mixing typographic „…" with ASCII `"` inside the same string breaks the JSON envelope. If you do use Bulgarian typographic quotes, both the opening **„** (U+201E) and closing **"** (U+201C) must be typographic — never mix with ASCII `"` inside the same string.
- Bulgarian typographic convention is `„…"` (low-9 opening, high-9 closing), but ASCII is the safe default.

## Plural forms

This language uses **2 plural forms**, in CLDR canonical order:

1. **one** — count = 1
2. **other** — every other integer count, including 0 (e.g., 0, 2, 3, 4, 5, …)

Each form must preserve `%n`. Example: `%n member(s)` → `["%n член", "%n членове"]`.
