# Arabic translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Script direction: right-to-left

Arabic is RTL. Keep placeholders (`%1`, `%2`) and Latin-script tokens (keyboard shortcuts like `Ctrl+R`, user IDs, URLs) unchanged — they render correctly inside RTL text thanks to Unicode bidi.

## Register

Arabic UIs conventionally use **impersonal or passive constructions** — follow the existing translations (e.g., "تم فتح أسرار التشفير"). When direct address is unavoidable, use the neutral/masculine **أنت** (conventional default in Arabic UI).

## Matrix / chat vocabulary

| English | Arabic |
|---|---|
| room | **غرفة** (pl. **غرف**) |
| space | **فضاء** (the existing translations use this) |
| thread | **سلسلة** (the existing translations use this) |
| direct message, DM | **رسالة مباشرة** |
| invite (verb / noun) | **دعوة** / **الدعوة** |
| join (a room) | **الانضمام إلى** |
| leave (a room) | **المغادرة** / **الخروج** |
| redact (= delete a message) | **حذف** (existing translations use this) |
| encryption | **التشفير** |
| encrypted | **مشفر** / **مشفّر** |
| verify / verification / verified (E2EE) | **توثيق** / **التوثيق** / **موثق** (existing translations use these; alternative: **تحقق / التحقق / محقق**) |
| user | **مستخدم** |
| message | **رسالة** |
| device | **جهاز** |

## Typography

- Use **Eastern Arabic numerals** (`٠١٢٣٤٥٦٧٨٩`) **only if they appear in the source or existing translations**; otherwise keep Western Arabic numerals (`0123456789`). The existing translations use Western numerals.
- Use Arabic comma `،` and semicolon `؛` — not ASCII `,` `;`.
- Arabic question mark: `؟` (not `?`).
- The existing translations are mixed between `…` and `...` for ellipsis — prefer `…` (U+2026) going forward.
- Keep placeholders `%1`/`%2` in the grammatically correct position for Arabic, reordering from source as needed.
