# Live emoticon replacement in the composer

Date: 2026-08-02
Status: Approved

## Problem

Typing a text emoticon like `:D` or `:)` in the composer shows the literal
text; it only becomes an emoji when the message is sent (via
`emoji::replaceEmoticons`, called from the various send paths). The user
can't see what will actually be sent until after it's gone.

## Goal

Convert a completed emoticon shortcut to its emoji in the composer itself,
as soon as it's unambiguously finished, so what's shown is what gets sent.

## Behavior

- **Trigger**: fires only when the user types a single literal space
  character immediately after a completed shortcut. No other boundary
  (Enter, punctuation, end of message) triggers it. This intentionally
  differs from `replaceEmoticons`'s send-time triggers (trimmed-end / always)
  — it's a new, narrower live-typing trigger.
- **Scope**: applies anywhere in the composer the user is typing or editing,
  not only when appending at the end of the message.
- **Token boundary**: the candidate token is the text between the space just
  typed and the previous whitespace character or the start of the message
  (same left-boundary rule `replaceEmoticons` already uses).
- **Matching**: exact match only, against the existing fixed pattern table in
  `src/emoji/EmoticonReplace.cpp` (`:)`, `:D`, `;)`, `<3`, `</3`, etc.),
  case-insensitive, same as today.
- **Gating**: only runs when `Settings.composerInputAutoReplaceEmoji !==
  AutoReplaceEmoji.Never`. The existing `Always` vs `OnlyAtEnd` distinction
  does not otherwise affect live behavior (see "Always vs OnlyAtEnd" below).
- **Non-triggers**: bulk insertions (paste, drag-drop, IME
  composition/autocomplete) do not trigger live replacement — only a single
  typed space character does. Pasted shortcuts remain unconverted until send,
  same as today (existing send-time fallback still applies, see below).
- **Backspace**: no special-case undo. Backspace after a conversion deletes
  the trailing space first, then the emoji as a whole grapheme cluster on the
  next press, like any other character.
- **Send-time behavior unchanged**: `replaceEmoticons` still runs on the
  final body at send time, in all existing call sites. This is now mostly a
  no-op for text the user watched convert live, but still catches
  shortcuts introduced via paste or other insertion paths that never passed
  through the live trigger.
- **Emoji picker completer**: no change needed. The completer already
  suppresses its popup once a typed token becomes an exact shortcut match
  (`MessageInput.qml`'s `refreshCompleterSearchString`); live replacement
  only acts on the space that follows, after the popup is already closed.

### Always vs OnlyAtEnd

The existing `composerInputAutoReplaceEmoji` setting has three modes,
originally designed for the send-time whole-body replacement:

- `Always`: replace every boundary-safe match anywhere in the message.
- `OnlyAtEnd`: replace only a single trailing match.
- `Never`: no replacement.

Live replacement is inherently a per-keystroke, single-token reaction (to
the token immediately before a just-typed space), not a whole-message pass.
That doesn't map onto the `Always`/`OnlyAtEnd` distinction, so both modes
enable live replacement identically; only `Never` disables it. This was
explicitly confirmed with the user rather than assumed.

## Implementation sketch

- **New C++ surface**: one small `Q_INVOKABLE` on `KomaiGlobalObject`
  (alongside the existing `isEmoticonShortcut`), e.g.
  `emoticonReplacementFor(const QString &token) -> QString`, returning the
  emoji for an exact shortcut match or an empty string otherwise. Implemented
  by reusing the existing pattern table in `emoji::EmoticonReplace.cpp`
  (single-token exact lookup, not the multi-match scan used by
  `replaceEmoticons`) — the pattern table stays single-sourced, nothing is
  duplicated in QML.
- **QML integration**: `resources/qml/composer/MessageInput.qml`'s existing
  `onTextChanged` handler already computes `insertedLength` per keystroke.
  Extend it: when `insertedLength === 1` and the inserted character is a
  space, extract the token immediately before that space (scan backward to
  the previous whitespace or start of text), look it up via
  `Komai.emoticonReplacementFor(token)`, and if non-empty, replace the token
  in place with the emoji (keeping the space after it) and advance
  `cursorPosition` to just after the space.
- **Guard**: skip the check entirely when
  `Settings.composerInputAutoReplaceEmoji === AutoReplaceEmoji.Never`, and
  when the space was part of a bulk insert (`insertedLength !== 1`) or typed
  during IME composition (`messageInput.inputMethodComposing`).

## Testing

- C++ unit test for the new `emoticonReplacementFor`-backing lookup
  (exact match, case-insensitivity, no match, empty input) alongside the
  existing `EmoticonReplace` tests.
- Manual verification in the running app: typing `:) ` converts inline;
  typing `:Dog` does not convert; editing mid-message and typing a space
  after a shortcut there also converts; pasting a shortcut and sending still
  converts via the existing send-time path; Backspace behavior after a
  conversion matches normal character deletion.

## Out of scope

- No new user-facing setting; reuses `composerInputAutoReplaceEmoji`.
- No backspace-based "undo the conversion" affordance.
- No change to the emoticon pattern table itself.
- No change to send-time replacement behavior or call sites.
