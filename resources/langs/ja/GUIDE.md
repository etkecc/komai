# Japanese translation instructions

These supplement the common rules; follow the existing translated strings as the primary reference.

## Register: polite 〜です/〜ます form

Use the **polite (desu/masu) form** for full sentences — the UI convention in Japanese software. For button labels and short commands, drop sentence-final politeness and use noun phrases or the plain stem (する, 削除, 保存).

- "Failed to join room: %1" → "ルームに参加できませんでした: %1"
- "Send" (button) → "送信"
- "Save" (button) → "保存"

## Matrix / chat vocabulary

| English | Japanese |
|---|---|
| room | **ルーム** |
| space | **スペース** |
| thread | **スレッド** |
| direct message, DM | **ダイレクトメッセージ** |
| invite (verb / noun) | **招待する** / **招待** |
| invited | **招待済** |
| join (a room) | **参加する** |
| leave (a room) | **退出する** |
| redact (= delete a message) | **削除する** |
| decrypt | **復号** |
| encryption | **暗号化** |
| encrypted | **暗号化済** |
| verify / verification / verified (E2EE) | **検証する** / **検証** / **検証済** |
| user | **ユーザー** |
| message | **メッセージ** |
| device | **デバイス** |

## Typography

- Use **full-width Japanese punctuation**: `、。！？：（）` — not ASCII.
- Use **「」** for quoted text (Japanese corner brackets).
- The existing translations use a regular ASCII colon `:` before values (`"ルームに参加できませんでした: %1"`). Keep that convention for consistency unless the source uses a full-width `：`.
- Use `…` (U+2026) for ellipsis, not `...`.
- Avoid unnecessary spaces between Japanese characters and placeholders.
