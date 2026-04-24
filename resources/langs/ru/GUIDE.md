# Russian translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register: informal ("ты")

Address the user with **ты / тебя / тебе / твой** — not formal "Вы". This matches the modern FOSS convention and the tone of the existing translations, which are informal or impersonal.

Many existing strings are **impersonal** ("Не удалось...", "Выгнанный пользователь: %1") — prefer this style when the source allows.

## Matrix / chat vocabulary

| English | Russian |
|---|---|
| room | **комната** (pl. **комнаты**) |
| space | **пространство** |
| thread | **тред** or **цепочка** |
| direct message, DM | **личное сообщение** |
| invite (verb / noun) | **пригласить** / **приглашение** |
| join (a room) | **присоединиться к** |
| leave (a room) | **покинуть** |
| redact (= delete a message) | **удалить** |
| encryption | **шифрование** |
| encrypted | **зашифрован/а/о** |
| verify / verification / verified (E2EE) | **верифицировать** / **верификация** / **верифицирован/а/о** |
| user | **пользователь** |
| message | **сообщение** |
| device | **устройство** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026), not `...`.
- Prefer ASCII `"…"` in translations — the pipeline emits JSON, and mixing typographic `«` with ASCII `"` inside the same string breaks the JSON envelope. If you do use guillemets «…», both the opening `«` and closing `»` must be typographic — never mix with ASCII `"`.
- Buttons/labels: use infinitive ("Сохранить", "Отмена", "Отправить").
