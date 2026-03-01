# 🧵 Timeline HTML Rendering

This page documents how formatted timeline message content is processed before it is rendered in QML.

## Scope

- Applies to timeline formatted message content (`formatted_body`) in `src/timeline/TimelineModel.cpp`.
- The code-highlighting feature is display-time only; it does not mutate Matrix event payloads.

## Pipeline Overview

For the `FormattedBody` role in `TimelineModel`:

1. Read `formatted_body(event)`.
2. Fallback: if empty, use escaped plain `body(event)` with `\n -> <br>`.
3. Remove reply fallback block for replies (`<mx-reply>...</mx-reply>`).
4. Sanitize HTML via `utils::escapeBlacklistedHtml(...)`.
5. Rewrite `mxc://` image URLs for the QML image provider.
6. Apply formatted code highlighting via `timeline::highlightFormattedCodeBlocks(...)` if enabled.
7. Run `utils::linkifyMessage(...)`.
8. Run `utils::replaceEmoji(...)`.
9. Render as Qt RichText in QML (`TextEdit.RichText` in `resources/qml/ui/MatrixText.qml`).

Primary callsites:

- `src/timeline/TimelineModel.cpp`
- `src/Utils.cpp` (`escapeBlacklistedHtml`, `linkifyMessage`)
- `src/timeline/formattedmessage/HtmlProcessor.cpp` (implementation for sanitization/linkification)
- `resources/qml/ui/MatrixText.qml`

## Code Highlighting: What Uses Which Library

- Parsing/orchestration of HTML code blocks is implemented in:
  - `src/timeline/FormattedCodeBlockHighlighter.cpp`
- Language resolution and content-based detection live in:
  - `src/timeline/formattedcode/LanguageResolver.cpp`
- Highlighted HTML span generation lives in:
  - `src/timeline/formattedcode/HtmlRenderer.cpp`
- Syntax grammar/theme lookup and token styling are done by KDE Frameworks:
  - `KSyntaxHighlighting::Repository`
  - `KSyntaxHighlighting::SyntaxHighlighter`
- MIME sniffing for snippet data is provided by Qt:
  - `QMimeDatabase`

So, auto-detection is primarily Komai logic with MIME assistance; it is not delegated to a standalone external language-detection library.

## Language Resolution Behavior

For `<pre><code>...</code></pre>` blocks:

1. Try explicit language from `class` on `<code>`:
   - `language-<token>`
   - `lang-<token>`
2. If missing, apply Komai heuristics:
   - First, try MIME-based definition lookup (`QMimeDatabase` -> `Repository::definitionForMimeType`)
   - Then fallback to Komai heuristics when MIME lookup is inconclusive
   - PHP prolog (`<?php`)
   - shebang detection (`python`, `bash/zsh/sh`, `node`)
   - diff markers (`diff --git`, `@@`, `---`, `+++`)
   - JSON (validated parse via `QJsonDocument`)
   - XML-like content
3. If no valid definition is found, preserve original block unchanged.

## Safety and Performance Guardrails

Current guardrails in `FormattedCodeBlockHighlighter` include:

- Total HTML size cap.
- Parsed-tag count cap.
- Maximum highlighted code blocks per message.
- Encoded and decoded code-size caps.
- Decoded line-count cap.
- Attribute-length and language-token-length caps.

On guardrail breach or malformed structure, processing falls back to original HTML/block content.

Code content emitted by the highlighter is HTML-escaped before span styling.

For formatted-message HTML preparation:

- Sanitization uses explicit allowlists for tags and tag-specific attributes.
- `href` and `src` values are scheme-filtered (`a[href]` allows web/mail/magnet schemes; `img[src]` is restricted to `mxc://`).
- `mx-reply` fallback blocks are stripped from formatted HTML.
- Tag nesting depth is capped.
- Linkification runs on HTML text segments only (not inside tag attributes or inside `<a>`, `<pre>`, `<code>` content).

## Settings

Feature toggle:

- Setting ID: `TimelineFormattedCodeSyntaxHighlighting`
- Config key: `timeline.messages.formatted.code_syntax_highlighting`
- UI label: `Syntax highlight formatted code blocks`
- UI location: Settings -> Timeline -> Presentation

## Raw Message Dialog

`View raw message` and `View decrypted raw message` also reuse the highlighting stack for JSON:

- formatter: `src/timeline/formattedcode/RawJsonFormatter.cpp`
- source JSON: serialized Matrix event (`dump(4)`)
- rendering: wrapped as `language-json` code block and highlighted before showing in `RawMessageDialog.qml`
