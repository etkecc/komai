# Ukrainian translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register

Prefer **impersonal constructions** ("Не вдалося приєднатися...", "Це повідомлення не зашифровано!") — matching the existing translations. When direct address is needed, use **informal "ти"** (modern FOSS convention).

## Matrix / chat vocabulary

| English | Ukrainian |
|---|---|
| room | **кімната** (pl. **кімнати**) |
| space | **простір** |
| thread | **гілка** |
| direct message, DM | **пряме повідомлення** |
| invite (verb / noun) | **запросити** / **запрошення** |
| join (a room) | **приєднатися до** |
| leave (a room) | **покинути** |
| redact (= delete a message) | **редагувати** (existing translations use this) / **видалити** |
| encryption | **шифрування** |
| encrypted | **зашифрований/а/е** |
| verify / verification / verified (E2EE) | **перевірити** / **перевірка** / **перевірений/а** |
| user | **користувач** |
| message | **повідомлення** |
| device | **пристрій** |

## Typography

- For quotes, follow the common GUIDE's rule (prefer ASCII in JSON); Ukrainian's typographic convention is `«…»` if you do use them.
- Use the **horizontal ellipsis `…`** (U+2026) — existing translations prefer it.
- Buttons/labels: use infinitive ("Зберегти", "Скасувати", "Надіслати").

## Plural forms

This language uses **3 plural forms**, in CLDR canonical order:

1. **one** — n%10 = 1 and n%100 ≠ 11 (e.g., 1, 21, 31, 41, …)
2. **few** — n%10 ∈ {2, 3, 4} and n%100 ∉ {12, 13, 14} (e.g., 2-4, 22-24, 32-34, …)
3. **many** — everything else, including 0 (e.g., 0, 5-20, 25-30, …)

Each form must preserve `%n`. Example: `%n member(s)` → `["%n учасник", "%n учасники", "%n учасників"]`.
