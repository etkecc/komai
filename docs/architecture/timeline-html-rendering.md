# Timeline HTML Rendering

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
9. Render via litehtml (`LitehtmlItem` QQuickPaintedItem) in QML for timeline message delegates (`TextMessage.qml`, `NoticeMessage.qml`). Blockquotes, tables, and other CSS-styled elements are handled natively by litehtml. Non-timeline uses (dialogs, room headers, login pages) continue to use Qt RichText via `MatrixText.qml`.

Primary callsites:

- `src/timeline/TimelineModel.cpp`
- `src/Utils.cpp` (`escapeBlacklistedHtml`, `linkifyMessage`)
- `src/timeline/formattedmessage/HtmlProcessor.cpp` (implementation for sanitization/linkification)
- `src/timeline/litehtml/LitehtmlItem.cpp` (timeline message rendering via litehtml)
- `src/timeline/litehtml/LitehtmlContainer.cpp` (QPainter-based litehtml document_container)
- `src/timeline/litehtml/LitehtmlStylesheet.cpp` (master CSS generation from theme palette)
- `resources/qml/ui/MatrixText.qml` (still used for non-timeline rich text)

## litehtml Rendering Architecture

Timeline messages use [litehtml](https://github.com/litehtml/litehtml) v0.9 (BSD-3-Clause), a lightweight C++ HTML/CSS engine with no JavaScript, no network access, and no plugins. It is fetched via CMake FetchContent and statically linked.

### Component Overview

```
TextMessage.qml
    └── LitehtmlItem (QQuickPaintedItem, QML_ELEMENT)
            ├── LitehtmlContainer (litehtml::document_container)
            │     ├── Font management (QFont + QFontMetrics)
            │     ├── Drawing (QPainter: text, backgrounds, borders, list markers)
            │     ├── Image loading (MxcImageProvider for mxc:// URLs only)
            │     └── Link click/hover handling
            └── LitehtmlStylesheet (generates user CSS from QPalette + settings)
```

### CSS Cascade

litehtml's `document::createFromString()` takes two CSS parameters:

- **master_css** (3rd param): litehtml's built-in user-agent stylesheet defining block/inline display, default margins for `<p>`, `<body>`, etc. We pass `litehtml::master_css` (the library constant).
- **user_css** (4th param): our custom stylesheet from `generateMasterStylesheet()`, which overrides specific styles (body margin, link colors, blockquote borders, etc.).

The user CSS has higher priority than the master CSS in litehtml's cascade.

### Sizing Lifecycle

1. `setHtml()` creates a `litehtml::document` via `createFromString()`.
2. `relayout()` skips if `width() < 1` (no real width assigned yet).
3. `EventDelegateChooser::updatePolish()` sets the delegate width → `geometryChange()` fires.
4. `relayout()` runs: `document::render(width)` → `setImplicitWidth(content_width)` / `setImplicitHeight(doc_height)`.
5. `EventDelegateChooser` reads `implicitWidth` and constrains the final width.
6. On subsequent width changes, `geometryChange()` triggers re-layout.

### Link Hover Detection

litehtml's `set_cursor("pointer")` callback indicates the cursor is over a link but does not provide the URL. To obtain the URL during hover:

1. `hoverMoveEvent` calls `on_mouse_over()` which triggers `set_cursor`.
2. If cursor is "pointer", the container enters "hover mode" and a simulated `on_lbutton_down` + `on_lbutton_up` is issued at the same position.
3. In hover mode, `on_anchor_click` captures the URL instead of emitting `linkClicked`.
4. The URL is exposed via the `hoveredLink` property for QML ToolTip binding.

### Image Loading

Only `image://mxcImage/` URLs are accepted (security). The container's `load_image()` calls `MxcImageProvider::download()`, caches the resulting `QImage`, and emits `imageLoaded` to trigger re-layout and repaint.

### HiDPI Rendering

`LitehtmlItem` sets `setAntialiasing(true)` and adjusts `setTextureSize()` based on the window's `devicePixelRatio()` so the painted content matches physical pixel resolution.

### Compact Mode

The `compact` property (bound to `Komai.uiLayoutCompactMode` in QML) controls paragraph/block-element margins in the generated CSS. Normal mode uses `0.65em`, compact mode uses `0.15em`.

### Collapsible Long Messages

Messages whose `implicitHeight` exceeds 50% of the timeline viewport (min 150px, fallback 300px) are collapsed in `TextMessage.qml`. The viewport height is passed explicitly from the bubble style via a `Binding` on `timelineViewportHeight`.

When collapsed, QML `height` is capped at `maxCollapsedHeight`. The `QQuickPaintedItem` paint buffer naturally clips the HTML content to the item bounds — no QML `clip` is needed, which allows child overlays (gradient fade, action bar) to extend beyond with negative anchor margins to reach the bubble container edges.

A "Show more" / "Show less" action bar at the bottom toggles the `collapsed` state. When expanded, the item's `height` is `implicitHeight + barHeight` to place the bar below the content without overlapping.

Reply previews are excluded from this mechanism (`!isReply`). Instead, the bubble style caps reply preview height at 25% of the timeline viewport with its own gradient fade; clicking the reply area navigates to the original message.

`geometryChange()` calls `updateTextureSize()` on height changes (not just width) to keep the paint buffer correctly sized after expanding.

## Performance Impact

The switch from Qt RichText (`Text.RichText`) to litehtml was motivated by HTML/CSS compatibility, not performance. A quick check (2 app launches per engine, rendering the same room with the same profile) confirmed no meaningful regression:

- Qt RichText: `202ms`, `199ms` (avg `~200ms`) to `model_bound`.
- litehtml: `198ms`, `207ms` (avg `~203ms`) to `model_bound`.

The ~3ms average difference is within run-to-run variance. This was a quick directional check, not rigorous benchmarking.

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

For litehtml rendering:

- Only `image://mxcImage/` URLs are accepted by the container's `load_image()` — all other URL schemes are rejected.
- No external CSS loading (`import_css` is a no-op).
- No custom element creation (`create_element` returns nullptr).

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
