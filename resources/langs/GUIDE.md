# Translation instructions for Komai

## What is Komai?

Komai is a desktop Matrix chat client (fork of nheko). It's a Qt/QML application for Linux, macOS, and Windows. The UI strings you are translating appear in menus, buttons, dialogs, tooltips, settings pages, and notifications.

## Rules

1. **Preserve placeholders exactly**: `%1`, `%2`, `%n`, etc. must appear in the translation in the correct position for the target language. Do not translate or remove them.

2. **Preserve HTML tags exactly**: `<b>`, `</b>`, `<br/>`, `<a href='...'>`, `</a>`, etc. must be kept verbatim. Translate only the text between tags.

3. **Preserve XML entities**: `&quot;`, `&lt;`, `&gt;`, `&amp;`, `&apos;` must remain as-is.

4. **Preserve escape sequences**: `\n`, `\t`, etc. must remain as-is.

5. **Do not translate these terms**: "Matrix", "Komai", "Element", room IDs (like `!abc:example.com`), user IDs (like `@user:example.com`), room aliases (like `#room:example.com`).

6. **Keep translations concise**: these are UI strings with limited space. Prefer short, clear phrasing over verbose explanations.

7. **Match the tone**: the UI is somewhat informal but not slangy. It should feel approachable and clear.

8. **Context field**: each string comes with a `context` field indicating which QML component or C++ class it belongs to (e.g., `RoomList`, `UserSettings`, `TimelineView`). Use this to disambiguate words that could be translated differently depending on context.

9. **Keyboard shortcuts**: strings like `Ctrl+K`, `Alt+A`, etc. should not be translated.

## Output format

You will receive a JSON array of objects with `source` and `context` fields. Return a JSON array of objects with `source` and `translation` fields. The `source` field must match exactly (it's used as the lookup key). Return **only** the JSON array, no other text.

Example input:
```json
[
  {"source": "Send", "context": "MessageComposer"},
  {"source": "Reply to %1", "context": "TimelineView"}
]
```

Example output:
```json
[
  {"source": "Send", "translation": "Senden"},
  {"source": "Reply to %1", "translation": "Antwort an %1"}
]
```
