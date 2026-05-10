# Translation instructions for Komai

## What is Komai?

Komai is a desktop Matrix chat client (fork of nheko). It's a Qt/QML application for Linux, macOS, and Windows. The UI strings you are translating appear in menus, buttons, dialogs, tooltips, settings pages, and notifications.

## Rules

1. **Preserve placeholders exactly**: `%1`, `%2`, `%n`, `%L1`, etc. must appear in the translation in the correct position for the target language. Do not translate or remove them. You may reorder them if the target language's word order requires it.

2. **Preserve HTML tags exactly**: `<b>`, `</b>`, `<br/>`, `<a href='...'>`, `</a>`, etc. must be kept verbatim. Translate only the text between tags.

3. **Preserve XML entities**: `&quot;`, `&lt;`, `&gt;`, `&amp;`, `&apos;` must remain as-is.

4. **Preserve escape sequences**: `\n`, `\t`, etc. must remain as-is.

5. **Do not translate these terms**: "Matrix", "Komai", "Element", "Nheko", room IDs (like `!abc:example.com`), user IDs (like `@user:example.com`), room aliases (like `#room:example.com`).

6. **Matrix vocabulary — use the language's idiomatic Matrix/FOSS-chat term, consistently**:
   - `room`, `space`, `thread`, `direct message` / `DM`, `reaction`, `redact`, `invite`, `encryption`, `verify` / `verification` / `verified` (in E2EE contexts) — each has an established translation per language. Pick one convention and use it everywhere; do not switch between synonyms from one string to the next.
   - "Verify" in E2EE/device contexts is a **security operation** (cryptographic check), not a generic "confirm". Most languages distinguish these — use the security term.
   - "Redact" means **delete a message** in Matrix (not "censor" or "edit"). Use the language's common term for deleting/removing a message.
   - "Recovery key" / "recovery passphrase" — translate as the language's idiomatic equivalent of *recovery* / *restore*, **not** *security* / *safety*. The "recovery key" is a single secret that lets the user regain access to their encrypted messages, not a hardware security token (YubiKey, FIDO2, etc.). The "recovery passphrase" is an optional memorable alternative the user picks during setup; it's deliberately distinct from the homeserver "account password" (the warning string about not reusing it should preserve that contrast). Komai uses "recovery key" / "recovery passphrase" everywhere, matching the Matrix spec and Element's terminology — older translations may say "security key" or bare "passphrase"; update them to the recovery wording for consistency.
   - If your language already has strings for these terms in this file (non-unfinished entries), match them. Consistency with the existing translation beats a nominally better alternative.

7. **Keyboard shortcuts and key names**: distinguish four sub-cases — getting this wrong is one of the most visible AI-translation failure modes.
   - **Modifier combos** (`Ctrl+R`, `Shift+G`, `Alt+F4`, `Meta+…`, `Cmd+…`): keep the entire token verbatim, including the key letter.
   - **Bare keycap labels** (`Space`, `Enter`, `Return`, `Esc`, `Escape`, `Tab`, `Backspace`, `Delete`, `Home`, `End`, `Page Up`/`Page Down`, `F1`–`F12`): keep verbatim — these match what's printed on the key on most keyboards. **Do not translate `Space` as the noun "space" / "area" / "outer space"** (e.g. Russian "Пространство", Polish "Przestrzeń", Greek "Χώρος", Chinese "空间" are all wrong here). If your language has a well-established native term that's actually printed on local keyboards (e.g. German `Leertaste`, Polish `Spacja`, Czech `mezerník`), that's also acceptable — but never the abstract-noun translation.
   - **Direction key references** (`Up`, `Down`, `Left`, `Right` when used to mean an arrow key): the arrow keys do not have these words printed on them — they have arrow glyphs. Translate to the Unicode arrow (`↑` `↓` `←` `→`) or to your language's word for the direction. **Never leave bare `Up` / `Down` in the middle of a translated sentence** — users won't know what `Up` is.
   - **Descriptive verbs around a key label** (`long-press`, `double-click`, `hold`, `press`): always translate. The bracket convention `[long-press Space]` does not freeze the prose verb — it just marks the keycap inside.

   Worked examples:
   - `"Record a voice message [Ctrl+R]"` → translate everything except `Ctrl+R`.
   - `"Hold or click to transcribe speech to text [long-press Space]"` → translate the prose **and** `long-press`; keep `Space`. ✅ German: `[Space lange drücken]`. ❌ `[long-press Space]` left literal.
   - `"Write a message, or press ↑ to select messages."` → keep `↑` verbatim, translate the rest. (If a source string still says `Up` or `Down` in prose, prefer `↑` / `↓` over leaving the English word.)

8. **Qt mnemonic accelerators**: Qt uses `&` to mark a menu/button keyboard accelerator.
   - `&File` means "F" is the mnemonic key. Keep exactly one `&` immediately before a letter suitable for your language, ideally one that matches the English letter when the target language has it. If no sensible accelerator exists, you may drop the `&` — but prefer to keep one.
   - `&&` is an **escaped literal ampersand** (renders as a single `&`). Preserve the double `&&` verbatim. Example: `"Close && preserve data"` → translate the surrounding words but keep `&&` intact.

9. **Quote style inside translations**: **prefer ASCII `"` and `'`** — you are emitting JSON, and mixing typographic opening quotes (`„`, `«`, `「`, `"`) with bare ASCII closing `"` inside a translation string breaks the JSON envelope and causes the pipeline to reject the whole batch. If a language convention strongly prefers typographic quotes, you may use them **only if every opening has a matching typographic closing quote** (never mix with ASCII `"` inside the same string). When in doubt, use ASCII — the pipeline can swap in typographic quotes later if desired.

10. **Keep translations concise**: these are UI strings with limited space — buttons, menu items, tooltips. Prefer short, clear phrasing over verbose explanations. Match the source length where possible.

11. **Match the tone**: the UI is somewhat informal but not slangy. It should feel approachable and clear. Use the conventional UI register for your language (e.g., German infinitive for buttons: "Speichern", not "Speicher!"). See the per-language instructions below if present.

12. **Context field**: each string comes with a `context` field indicating which QML component or C++ class it belongs to (e.g., `RoomList`, `UserSettings`, `TimelineView`). Use this to disambiguate words that could be translated differently depending on context. A `location` field (source file path) may also be provided for further disambiguation.

13. **Optional `comment` / `extracomment` fields**: when present, these are explicit hints from the developers about what the string means or how it's used. Treat them as authoritative — they exist specifically to remove ambiguity.

14. **Never omit entries from your response**: return exactly one object per input string, in the same order. If a string is ambiguous or you're unsure, translate it literally using the most common UI meaning for its `context`. Do not skip items — missing entries force the pipeline to re-process, costing time and consistency. When truly uncertain, a literal translation is better than nothing.

15. **Plural forms (numerus messages)**: some sources use `%n` as a count placeholder and require multiple grammatical-number forms — these are sent in a **separate translation pass** with a different output shape. Each item returns a `forms` array (one string per form) instead of a single `translation` string. Form count and category labels (`zero` / `one` / `two` / `few` / `many` / `other`) are language-specific — see the per-language GUIDE for the rules that apply. **Every rule above (placeholders, HTML, escapes, mnemonics, shortcuts, etc.) applies to every form individually** — each form must preserve `%n` and any other placeholders / tags / literals from the source.

## Output format

You will receive a JSON array of objects with `source` and `context` fields (optionally also `location`, `comment`, `extracomment`). Return a JSON array of objects with `source` and `translation` fields. The `source` field must match exactly (it's used as the lookup key). Return **only** the JSON array, no other text.

Example input:
```json
[
  {"source": "Send", "context": "MessageComposer"},
  {"source": "Reply to %1", "context": "TimelineView"},
  {"source": "Close && preserve data", "context": "SessionTab", "extracomment": "Keeps the local database when closing the app."}
]
```

Example output:
```json
[
  {"source": "Send", "translation": "Senden"},
  {"source": "Reply to %1", "translation": "Antwort an %1"},
  {"source": "Close && preserve data", "translation": "Schließen && Daten behalten"}
]
```
