# Traditional Chinese translation instructions

These supplement the common rules.

**Important**: this is **Traditional Chinese (zh_Hant)** — use traditional characters (繁體字), Taiwan/Hong Kong conventions. Do not use simplified characters.

## Register

Chinese has no T-V distinction. Use the second person **你** when direct address is needed; prefer impersonal constructions for general UI text.

## Matrix / chat vocabulary

| English | Traditional Chinese |
|---|---|
| room (chat) | **聊天室** (common Matrix convention; not literal 房間) |
| space | **空間** |
| thread | **討論串** (Taiwan convention) |
| direct message, DM | **私訊** |
| invite (verb / noun) | **邀請** |
| join (a room) | **加入** |
| leave (a room) | **離開** |
| redact (= delete a message) | **刪除** |
| encryption | **加密** |
| encrypted | **已加密** |
| verify / verification / verified (E2EE) | **驗證** / **已驗證** |
| user | **使用者** (Taiwan) — note: Mainland Chinese uses "用戶" |
| message | **訊息** (Taiwan) — note: Mainland uses "消息" |
| device | **裝置** (Taiwan) — note: Mainland uses "设备" (simplified) |

## Typography

- Use **full-width Chinese punctuation**: `，。！？；：（）` — not ASCII.
- Use full-width quotes **「」** for quoted text (Taiwan/HK convention).
- Ellipsis: **`……`** (two U+2026, Chinese convention) or a single `…`.
- No space between Chinese text and placeholders.

## Plural forms

This language uses **a single plural form** for all counts. When a `numerus` source like `%n member(s)` is presented in the plural-form pass, return one translation that works grammatically regardless of count.

The form must preserve `%n`. Example: `%n member(s)` → `["%n 名成員"]`.
