# Vietnamese translation instructions

These supplement the common rules. No prior translations exist to mine — translate fresh.

## Register

Vietnamese UI conventionally uses **"bạn"** for the user — neutral, polite, universal. Avoid intimate/familial pronouns (em, anh, chị) and overly formal ones (ngài, quý vị).

## Matrix / chat vocabulary

| English | Vietnamese |
|---|---|
| room | **phòng** |
| space | **không gian** |
| thread | **chủ đề** / **luồng** |
| direct message, DM | **tin nhắn trực tiếp** |
| invite (verb / noun) | **mời** / **lời mời** |
| join (a room) | **tham gia** |
| leave (a room) | **rời khỏi** |
| redact (= delete a message) | **xóa** |
| encryption | **mã hóa** |
| encrypted | **đã mã hóa** |
| verify / verification / verified (E2EE) | **xác minh** / **xác minh** / **đã xác minh** |
| user | **người dùng** |
| message | **tin nhắn** |
| device | **thiết bị** |

## Typography

- Use the **horizontal ellipsis `…`** (U+2026), not `...`.
- Vietnamese uses ASCII punctuation `. , ? !`.
- Preserve diacritics precisely — Vietnamese tone marks are semantically critical (dấu sắc, huyền, hỏi, ngã, nặng).
- Buttons/labels: use verb forms ("Lưu", "Hủy", "Gửi").

## Plural forms

This language uses **a single plural form** for all counts. When a `numerus` source like `%n member(s)` is presented in the plural-form pass, return one translation that works grammatically regardless of count.

The form must preserve `%n`. Example: `%n member(s)` → `["%n thành viên"]`.
