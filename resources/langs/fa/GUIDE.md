# Persian (Farsi) translation instructions

These supplement the common rules.

## Script direction: right-to-left

Persian is RTL. Keep placeholders (`%1`, `%2`) and Latin-script tokens (keyboard shortcuts `Ctrl+R`, user IDs, URLs) unchanged — Unicode bidi handles rendering.

## Register

Persian UI conventionally uses **impersonal / passive constructions** or **formal "شما"**. Avoid informal "تو" in UI.

## Matrix / chat vocabulary

| English | Persian |
|---|---|
| room | **اتاق** |
| space | **فضا** |
| thread | **رشته** |
| direct message, DM | **پیام مستقیم** |
| invite (verb / noun) | **دعوت کردن** / **دعوت** |
| join (a room) | **پیوستن به** |
| leave (a room) | **ترک کردن** |
| redact (= delete a message) | **حذف کردن** |
| encryption | **رمزگذاری** |
| encrypted | **رمزگذاری‌شده** |
| verify / verification / verified (E2EE) | **تأیید کردن** / **تأیید** / **تأییدشده** |
| user | **کاربر** |
| message | **پیام** |
| device | **دستگاه** |

## Typography

- Use the Persian-specific characters **ی** (U+06CC) and **ک** (U+06A9), not the Arabic ي / ك.
- Use Persian digits **۰۱۲۳۴۵۶۷۸۹** only if the source uses them; otherwise keep Western digits.
- Use Persian punctuation: **،** (comma), **؛** (semicolon), **؟** (question mark).
- Use **ZWNJ** (U+200C, zero-width non-joiner) where required for compound words (e.g., `رمزگذاری‌شده`).
- Use the **horizontal ellipsis `…`** (U+2026) where possible.

## Plural forms

This language uses **a single plural form** for all counts. When a `numerus` source like `%n member(s)` is presented in the plural-form pass, return one translation that works grammatically regardless of count.

The form must preserve `%n`. Example: `%n member(s)` → `["%n عضو"]`.
