// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Composer-side Markdown formatting transforms (bold / italic / inline code /
//! fenced code / quote / link). The QML composer keyboard shortcuts and the
//! selection-anchored formatting toolbar call into these functions; the C++
//! shim applies the returned `(replace_range, replacement)` atomically via
//! `QTextCursor::insertText` so the whole toggle is one undo step.
//!
//! ## Index conventions
//!
//! All `_utf16` indices on the API surface are UTF-16 code unit offsets — what
//! Qt's `TextArea.selectionStart` / `cursorPosition` / `positionToRectangle`
//! report. Internally we translate to UTF-8 byte offsets via the helpers at
//! the bottom of this file. The C++ side never has to think about UTF-8.
//!
//! ## Toggle semantics
//!
//! Every public `toggle_*` function returns a `ComposerTransformResult` that
//! describes the smallest range to replace and the replacement string, plus
//! the selection bounds to restore afterwards. When the operation is a no-op
//! (empty selection where one is required, malformed shape, etc.) the result
//! has `applied = false` and the caller falls back to the keyboard's default
//! behavior (literal char insertion).

use crate::ffi::ComposerTransformResult;

// ============================================================================
// Public API
// ============================================================================

/// Toggle an inline Markdown wrap (`**` bold, `*` italic, `` ` `` inline code)
/// around the selection. Detects three states:
///
/// 1. **Strip-inside** — the selection already contains the marker on both
///    sides (e.g. selection is `**foo**` for bold). Strip the markers.
/// 2. **Strip-by-context** — the markers sit immediately outside the
///    selection (e.g. selection is `foo` with `**` chars before and after).
///    Strip the markers, expanding the replace range to include them.
/// 3. **Wrap** — neither, so add markers around the selection.
///
/// Bold (`**`) takes precedence over italic (`*`) on nested constructs:
/// `***foo***` (bold+italic) selected and bold-toggled → `*foo*` (italic
/// stays). Italic-toggled → `**foo**` (bold stays). The parity rule
/// "italic strips iff the marker count on each side is odd" is what enforces
/// this; see `italic_strip_inside`.
pub fn toggle_inline_wrap(
    text: &str,
    sel_start_utf16: u32,
    sel_end_utf16: u32,
    marker: &str,
) -> ComposerTransformResult {
    if marker != "*" && marker != "**" && marker != "`" {
        return no_op();
    }

    let (utf16_s, utf16_e) = order_pair(sel_start_utf16, sel_end_utf16);
    let sel_start = utf16_to_byte_offset(text, utf16_s);
    let sel_end = utf16_to_byte_offset(text, utf16_e);

    if sel_start == sel_end {
        return no_op();
    }

    let sel_text = &text[sel_start..sel_end];

    // 1. Strip-inside.
    if let Some((left, right)) = strip_inside_inline(sel_text, marker) {
        let inner_start = sel_start + left;
        let inner_end = sel_end - right;
        let inner = &text[inner_start..inner_end];
        return make_result(
            text,
            sel_start,
            sel_end,
            inner.to_string(),
            sel_start,
            sel_start + inner.len(),
        );
    }

    // 2. Strip-by-context.
    if let Some((left, right)) = context_strip_inline(text, sel_start, sel_end, marker) {
        let new_start = sel_start - left;
        let new_end = sel_end + right;
        let inner = sel_text.to_string();
        let inner_len = inner.len();
        return make_result(text, new_start, new_end, inner, new_start, new_start + inner_len);
    }

    // 3. Wrap.
    let replacement = format!("{}{}{}", marker, sel_text, marker);
    let replacement_len = replacement.len();
    make_result(
        text,
        sel_start,
        sel_end,
        replacement,
        sel_start,
        sel_start + replacement_len,
    )
}

/// Toggle a per-line prefix (currently `"> "` for blockquote) across every
/// line the selection touches. The first affected line is the line containing
/// `sel_start`; the last is the line containing the byte right before
/// `sel_end`, with the special rule that a selection ending at the position
/// just past a final newline does NOT promote a phantom trailing line.
///
/// All non-empty lines must already be prefixed for the toggle to UNquote;
/// otherwise every line gets a fresh prefix added (the "promote all" branch).
/// Empty lines participate too: they become `>` on quote and `""` on unquote,
/// round-trip-safe.
pub fn toggle_block_prefix(
    text: &str,
    sel_start_utf16: u32,
    sel_end_utf16: u32,
    prefix: &str,
) -> ComposerTransformResult {
    if prefix != "> " {
        return no_op();
    }

    let (utf16_s, utf16_e) = order_pair(sel_start_utf16, sel_end_utf16);
    let sel_start = utf16_to_byte_offset(text, utf16_s);
    let sel_end = utf16_to_byte_offset(text, utf16_e);

    if sel_start == sel_end {
        return no_op();
    }

    let (block_start, block_end) = match find_line_block(text, sel_start, sel_end) {
        Some(range) => range,
        None => return no_op(),
    };

    let block = &text[block_start..block_end];
    let lines: Vec<&str> = block.split('\n').collect();

    let any_quoted = lines.iter().any(|l| is_quoted_line(l));
    let any_non_empty = lines.iter().any(|l| !l.is_empty());
    let all_non_empty_quoted = lines.iter().filter(|l| !l.is_empty()).all(|l| is_quoted_line(l));

    let new_lines: Vec<String> = if any_quoted && all_non_empty_quoted && any_non_empty {
        lines.iter().map(|l| strip_quote_line(l)).collect()
    } else {
        lines.iter().map(|l| add_quote_line(l)).collect()
    };
    let replacement = new_lines.join("\n");
    let replacement_len = replacement.len();

    make_result(
        text,
        block_start,
        block_end,
        replacement,
        block_start,
        block_start + replacement_len,
    )
}

/// Inline backtick when the selection fits on one line, fenced ```` ``` ````
/// block when it spans multiple lines. Round-trip toggle in both directions
/// (including stripping a fenced block from its inner-text selection, via
/// context detection).
pub fn toggle_code(
    text: &str,
    sel_start_utf16: u32,
    sel_end_utf16: u32,
) -> ComposerTransformResult {
    let (utf16_s, utf16_e) = order_pair(sel_start_utf16, sel_end_utf16);
    let sel_start = utf16_to_byte_offset(text, utf16_s);
    let sel_end = utf16_to_byte_offset(text, utf16_e);

    if sel_start == sel_end {
        return no_op();
    }

    let sel_text = &text[sel_start..sel_end];
    let is_multiline = sel_text.contains('\n');

    // Fenced strip-by-context works in both single- and multi-line cases —
    // the user may have selected just the inner content of an existing
    // ```\n...\n``` block.
    if let Some(result) = fenced_context_strip(text, sel_start, sel_end) {
        return result;
    }

    if !is_multiline {
        return toggle_inline_wrap(text, sel_start_utf16, sel_end_utf16, "`");
    }

    // Multi-line: try fenced strip-inside, else fenced wrap.
    if sel_text.starts_with("```\n") && sel_text.ends_with("\n```") && sel_text.len() >= 8 {
        let inner_start = sel_start + 4;
        let inner_end = sel_end - 4;
        let inner = &text[inner_start..inner_end];
        return make_result(
            text,
            sel_start,
            sel_end,
            inner.to_string(),
            sel_start,
            sel_start + inner.len(),
        );
    }

    let replacement = format!("```\n{}\n```", sel_text);
    let replacement_len = replacement.len();
    make_result(
        text,
        sel_start,
        sel_end,
        replacement,
        sel_start,
        sel_start + replacement_len,
    )
}

/// Toggle a Markdown link wrap around the selection.
///
/// - Empty selection → insert `[]()` with cursor inside the `[]` (write the
///   label first).
/// - Selection looks like a URL (`^[a-z][a-z0-9+\-.]*://...`, no whitespace)
///   → reverse-wrap as `[](URL)` with cursor inside `[]`.
/// - Selection is the inner text `X` of a `[X](Y)` shape in the buffer →
///   strip the link.
/// - Selection is the full `[X](Y)` shape → strip the link.
/// - Otherwise → wrap as `[sel]()` with cursor between the parens (write the
///   URL).
pub fn toggle_link(
    text: &str,
    sel_start_utf16: u32,
    sel_end_utf16: u32,
) -> ComposerTransformResult {
    let (utf16_s, utf16_e) = order_pair(sel_start_utf16, sel_end_utf16);
    let sel_start = utf16_to_byte_offset(text, utf16_s);
    let sel_end = utf16_to_byte_offset(text, utf16_e);

    // Empty selection: cursor lands inside the brackets so the user types the
    // label first. Returning `applied = true` for an empty selection is the
    // exception to the general rule — the QML keyboard branch dispatches Link
    // even with no selection, on purpose.
    if sel_start == sel_end {
        let replacement = "[]()".to_string();
        let cursor = byte_to_utf16_offset(text, sel_start) + 1;
        return ComposerTransformResult {
            applied: true,
            replace_start_utf16: byte_to_utf16_offset(text, sel_start),
            replace_end_utf16: byte_to_utf16_offset(text, sel_end),
            replacement_text: replacement,
            new_sel_start_utf16: cursor,
            new_sel_end_utf16: cursor,
        };
    }

    let sel_text = &text[sel_start..sel_end];

    // Unwrap: selection is inner text `X` of a [X](Y) in the buffer.
    let before = &text[..sel_start];
    let after = &text[sel_end..];
    if before.ends_with('[') && after.starts_with("](") {
        if let Some(rel) = after[2..].find(')') {
            let close_byte = sel_end + 2 + rel + 1;
            let new_start = sel_start - 1;
            let new_end = close_byte;
            let inner = sel_text.to_string();
            let inner_len = inner.len();
            return make_result(text, new_start, new_end, inner, new_start, new_start + inner_len);
        }
    }

    // Unwrap: selection is the full [X](Y) shape.
    if let Some((label_start, label_end, _url_start, _url_end)) = match_full_link_shape(sel_text) {
        let inner = sel_text[label_start..label_end].to_string();
        let inner_len = inner.len();
        return make_result(
            text,
            sel_start,
            sel_end,
            inner,
            sel_start,
            sel_start + inner_len,
        );
    }

    // Wrap: URL-shaped selection → `[](url)` with cursor inside the brackets.
    if is_url_shape(sel_text) {
        let replacement = format!("[]({})", sel_text);
        let cursor = byte_to_utf16_offset(text, sel_start) + 1;
        return ComposerTransformResult {
            applied: true,
            replace_start_utf16: byte_to_utf16_offset(text, sel_start),
            replace_end_utf16: byte_to_utf16_offset(text, sel_end),
            replacement_text: replacement,
            new_sel_start_utf16: cursor,
            new_sel_end_utf16: cursor,
        };
    }

    // Default wrap: `[sel]()` with cursor inside the parens.
    let replacement = format!("[{}]()", sel_text);
    // Cursor between `(` and `)` = sel_start + 1 (for `[`) + utf16(sel_text) + 2 (for `](`).
    let cursor = byte_to_utf16_offset(text, sel_start) + 1 + utf16_len(sel_text) + 2;
    ComposerTransformResult {
        applied: true,
        replace_start_utf16: byte_to_utf16_offset(text, sel_start),
        replace_end_utf16: byte_to_utf16_offset(text, sel_end),
        replacement_text: replacement,
        new_sel_start_utf16: cursor,
        new_sel_end_utf16: cursor,
    }
}

// ============================================================================
// Inline-wrap helpers
// ============================================================================

fn strip_inside_inline(sel_text: &str, marker: &str) -> Option<(usize, usize)> {
    match marker {
        "*" => italic_strip_inside(sel_text),
        "**" => bold_strip_inside(sel_text),
        "`" => code_strip_inside(sel_text),
        _ => None,
    }
}

fn context_strip_inline(
    text: &str,
    sel_start: usize,
    sel_end: usize,
    marker: &str,
) -> Option<(usize, usize)> {
    match marker {
        "*" => italic_context_strip(text, sel_start, sel_end),
        "**" => bold_context_strip(text, sel_start, sel_end),
        "`" => code_context_strip(text, sel_start, sel_end),
        _ => None,
    }
}

/// Strips italic only when both leading and trailing `*` runs have ODD count
/// AND don't overlap (`***` is all-marker → no strip). Parity is what
/// distinguishes italic from bold inside `***foo***` — bold sits in the even
/// positions (outer 2 stars), italic in the innermost odd.
fn italic_strip_inside(sel_text: &str) -> Option<(usize, usize)> {
    let nl = count_leading_char(sel_text, '*');
    let nr = count_trailing_char(sel_text, '*');
    let total = sel_text.chars().count();
    if nl >= 1 && nr >= 1 && nl % 2 == 1 && nr % 2 == 1 && nl + nr <= total {
        Some((1, 1))
    } else {
        None
    }
}

fn bold_strip_inside(sel_text: &str) -> Option<(usize, usize)> {
    let nl = count_leading_char(sel_text, '*');
    let nr = count_trailing_char(sel_text, '*');
    let total = sel_text.chars().count();
    if nl >= 2 && nr >= 2 && nl + nr <= total {
        Some((2, 2))
    } else {
        None
    }
}

fn code_strip_inside(sel_text: &str) -> Option<(usize, usize)> {
    let nl = count_leading_char(sel_text, '`');
    let nr = count_trailing_char(sel_text, '`');
    let total = sel_text.chars().count();
    if nl >= 1 && nr >= 1 && nl + nr <= total {
        Some((1, 1))
    } else {
        None
    }
}

fn italic_context_strip(
    text: &str,
    sel_start: usize,
    sel_end: usize,
) -> Option<(usize, usize)> {
    let before = &text[..sel_start];
    let after = &text[sel_end..];
    if !before.ends_with('*') || !after.starts_with('*') {
        return None;
    }
    // The char beyond the candidate `*` marker must not also be `*` —
    // otherwise we'd be stripping half of `**` (bold) or eroding part of
    // `***` (bold+italic, which italic shouldn't touch from the outside).
    if sel_start >= 2 && &text[sel_start - 2..sel_start - 1] == "*" {
        return None;
    }
    if sel_end + 1 < text.len() && &text[sel_end + 1..sel_end + 2] == "*" {
        return None;
    }
    Some((1, 1))
}

fn bold_context_strip(
    text: &str,
    sel_start: usize,
    sel_end: usize,
) -> Option<(usize, usize)> {
    if text[..sel_start].ends_with("**") && text[sel_end..].starts_with("**") {
        Some((2, 2))
    } else {
        None
    }
}

fn code_context_strip(
    text: &str,
    sel_start: usize,
    sel_end: usize,
) -> Option<(usize, usize)> {
    if text[..sel_start].ends_with('`') && text[sel_end..].starts_with('`') {
        Some((1, 1))
    } else {
        None
    }
}

// ============================================================================
// Block-prefix helpers
// ============================================================================

/// Lines included in a block-prefix toggle. Returns the byte range
/// `[first_line_start, last_line_end)` covering whole lines.
///
/// The "trailing newline guard" lives here: if the byte right before
/// `sel_end` is a `\n`, we treat `sel_end` as ending at the position OF that
/// `\n` (so the line that the `\n` separates from is excluded). This stops
/// us from quoting a phantom empty line when the user selected up to the
/// very end of the buffer past a final newline, while still including the
/// content line whose terminating `\n` happens to be the last selected
/// byte.
fn find_line_block(
    text: &str,
    sel_start: usize,
    sel_end: usize,
) -> Option<(usize, usize)> {
    let first_line_start = text[..sel_start].rfind('\n').map(|i| i + 1).unwrap_or(0);

    let last_inclusive = sel_end - 1;
    let last_line_end = if text.as_bytes().get(last_inclusive) == Some(&b'\n') {
        // The last byte in the selection IS a newline. Treat the selection
        // as ending at that newline's position — the line whose content
        // terminates here gets included, the next line does NOT.
        last_inclusive
    } else if let Some(rel) = text[last_inclusive..].find('\n') {
        last_inclusive + rel
    } else {
        text.len()
    };

    if last_line_end <= first_line_start {
        return None;
    }
    Some((first_line_start, last_line_end))
}

fn is_quoted_line(line: &str) -> bool {
    line.starts_with('>')
}

fn strip_quote_line(line: &str) -> String {
    if let Some(rest) = line.strip_prefix("> ") {
        return rest.to_string();
    }
    if let Some(rest) = line.strip_prefix('>') {
        return rest.to_string();
    }
    line.to_string()
}

fn add_quote_line(line: &str) -> String {
    if line.is_empty() {
        ">".to_string()
    } else {
        format!("> {}", line)
    }
}

// ============================================================================
// Code-fence helpers
// ============================================================================

fn fenced_context_strip(
    text: &str,
    sel_start: usize,
    sel_end: usize,
) -> Option<ComposerTransformResult> {
    let before = &text[..sel_start];
    let after = &text[sel_end..];
    if before.ends_with("```\n") && after.starts_with("\n```") {
        let new_start = sel_start - 4;
        let new_end = sel_end + 4;
        let inner = text[sel_start..sel_end].to_string();
        let inner_len = inner.len();
        return Some(make_result(text, new_start, new_end, inner, new_start, new_start + inner_len));
    }
    None
}

// ============================================================================
// Link helpers
// ============================================================================

/// Returns `(label_start, label_end, url_start, url_end)` relative to `s` if
/// `s` is exactly `[X](Y)` with the `)` as the last byte. The label and URL
/// halves are otherwise opaque (no further validation of their contents).
fn match_full_link_shape(s: &str) -> Option<(usize, usize, usize, usize)> {
    if !s.starts_with('[') || !s.ends_with(')') {
        return None;
    }
    let close_bracket = s.find(']')?;
    if s.as_bytes().get(close_bracket + 1) != Some(&b'(') {
        return None;
    }
    let url_start = close_bracket + 2;
    let url_end = s.len() - 1;
    if url_start > url_end {
        return None;
    }
    // The `)` we already know about must be the only one after `](` for the
    // shape to be a single self-contained link. A `)` inside the URL halve
    // would still be parsed by most Markdown renderers, but for our toggle
    // we accept the inclusive form: the rightmost `)` is the closer.
    Some((1, close_bracket, url_start, url_end))
}

/// True when `s` looks like a fully-qualified URL: starts with an ASCII
/// letter, followed by URL scheme chars, then `://`, with no whitespace
/// anywhere. Matrix-flavored schemes (`mxc:`, `matrix:`) without `://` are
/// intentionally NOT detected — Element treats them as labels too.
fn is_url_shape(s: &str) -> bool {
    if s.is_empty() || s.chars().any(|c| c.is_whitespace()) {
        return false;
    }
    let bytes = s.as_bytes();
    if !bytes[0].is_ascii_alphabetic() {
        return false;
    }
    let mut i = 1usize;
    while i < bytes.len() {
        let c = bytes[i];
        if c.is_ascii_alphanumeric() || c == b'+' || c == b'-' || c == b'.' {
            i += 1;
        } else {
            break;
        }
    }
    s[i..].starts_with("://")
}

// ============================================================================
// Generic helpers
// ============================================================================

fn no_op() -> ComposerTransformResult {
    ComposerTransformResult {
        applied: false,
        replace_start_utf16: 0,
        replace_end_utf16: 0,
        replacement_text: String::new(),
        new_sel_start_utf16: 0,
        new_sel_end_utf16: 0,
    }
}

fn make_result(
    text: &str,
    replace_start_b: usize,
    replace_end_b: usize,
    replacement: String,
    new_sel_start_b: usize,
    new_sel_end_b: usize,
) -> ComposerTransformResult {
    // Note: byte_to_utf16_offset measures into the *original* text. The new
    // selection bounds expressed in bytes refer to positions in the NEW text
    // (after the replacement is applied). We convert each new-text byte
    // position to a UTF-16 offset by piecing together unchanged prefix +
    // replacement.
    let prefix_utf16 = byte_to_utf16_offset(text, replace_start_b);
    let replace_start_u = prefix_utf16;
    let replace_end_u = byte_to_utf16_offset(text, replace_end_b);
    let replacement_utf16 = utf16_len(&replacement);
    // new_sel_start_b / new_sel_end_b refer to byte positions in the *new*
    // text. Either they sit in the prefix (< replace_start_b), in the
    // replacement (replace_start_b ..= replace_start_b + replacement.len()),
    // or in the suffix (> replace_start_b + replacement.len()).
    let new_text_len_before = replace_start_b;
    let new_text_repl_end = replace_start_b + replacement.len();

    let new_sel_start_u = if new_sel_start_b <= new_text_len_before {
        byte_to_utf16_offset(text, new_sel_start_b)
    } else if new_sel_start_b <= new_text_repl_end {
        let into_repl = new_sel_start_b - new_text_len_before;
        prefix_utf16 + utf16_len(&replacement[..into_repl])
    } else {
        let suffix_byte_in_orig = new_sel_start_b - new_text_repl_end + replace_end_b;
        prefix_utf16 + replacement_utf16
            + byte_to_utf16_offset(text, suffix_byte_in_orig)
            - byte_to_utf16_offset(text, replace_end_b)
    };
    let new_sel_end_u = if new_sel_end_b <= new_text_len_before {
        byte_to_utf16_offset(text, new_sel_end_b)
    } else if new_sel_end_b <= new_text_repl_end {
        let into_repl = new_sel_end_b - new_text_len_before;
        prefix_utf16 + utf16_len(&replacement[..into_repl])
    } else {
        let suffix_byte_in_orig = new_sel_end_b - new_text_repl_end + replace_end_b;
        prefix_utf16 + replacement_utf16
            + byte_to_utf16_offset(text, suffix_byte_in_orig)
            - byte_to_utf16_offset(text, replace_end_b)
    };

    ComposerTransformResult {
        applied: true,
        replace_start_utf16: replace_start_u,
        replace_end_utf16: replace_end_u,
        replacement_text: replacement,
        new_sel_start_utf16: new_sel_start_u,
        new_sel_end_utf16: new_sel_end_u,
    }
}

fn order_pair(a: u32, b: u32) -> (u32, u32) {
    if a <= b { (a, b) } else { (b, a) }
}

fn count_leading_char(s: &str, c: char) -> usize {
    s.chars().take_while(|x| *x == c).count()
}

fn count_trailing_char(s: &str, c: char) -> usize {
    s.chars().rev().take_while(|x| *x == c).count()
}

fn utf16_to_byte_offset(s: &str, utf16_units: u32) -> usize {
    let target = utf16_units as usize;
    if target == 0 {
        return 0;
    }
    let mut units = 0usize;
    for (byte_idx, c) in s.char_indices() {
        if units >= target {
            return byte_idx;
        }
        units += c.len_utf16();
    }
    s.len()
}

fn byte_to_utf16_offset(s: &str, byte_idx: usize) -> u32 {
    let mut adjusted = byte_idx.min(s.len());
    while adjusted > 0 && !s.is_char_boundary(adjusted) {
        adjusted -= 1;
    }
    s[..adjusted].encode_utf16().count() as u32
}

fn utf16_len(s: &str) -> u32 {
    s.encode_utf16().count() as u32
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests;
