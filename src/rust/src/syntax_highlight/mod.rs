// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Syntax highlighting for `<pre><code>` blocks in HTML messages.
//!
//! Replaces the former C++/KF6SyntaxHighlighting implementation with a pure-Rust
//! pipeline built on [syntect](https://docs.rs/syntect).

use std::sync::LazyLock;

use syntect::highlighting::ThemeSet;
use syntect::html::highlighted_html_for_string;
use syntect::parsing::{SyntaxReference, SyntaxSet};

// ---------------------------------------------------------------------------
// Limits (mirroring the previous C++ values)
// ---------------------------------------------------------------------------

const MAX_ENCODED_CODE_BLOCK_CHARS: usize = 120_000;
const MAX_DECODED_CODE_BLOCK_CHARS: usize = 20_000;
const MAX_DECODED_CODE_BLOCK_LINES: usize = 800;
const MAX_HIGHLIGHTED_CODE_BLOCKS: usize = 64;
const MAX_HTML_CHARS: usize = 1_000_000;
const MAX_PARSED_TAGS: usize = 50_000;
const MAX_ATTRIBUTE_CHARS: usize = 4096;
const MAX_LANGUAGE_TOKEN_CHARS: usize = 64;

// ---------------------------------------------------------------------------
// Lazy-initialized syntax and theme sets
// ---------------------------------------------------------------------------

static SYNTAX_SET: LazyLock<SyntaxSet> = LazyLock::new(SyntaxSet::load_defaults_newlines);
static THEME_SET: LazyLock<ThemeSet> = LazyLock::new(ThemeSet::load_defaults);

// ---------------------------------------------------------------------------
// Public entry point (called from C++ via CXX)
// ---------------------------------------------------------------------------

/// Process an HTML fragment, highlighting every eligible `<pre><code>` block.
///
/// * `is_dark_theme` – whether the current UI palette is dark (controls theme
///   selection).
pub(crate) fn highlight_formatted_code_blocks(html: &str, is_dark_theme: bool) -> String {
    if html.is_empty() || html.len() > MAX_HTML_CHARS {
        return html.to_owned();
    }

    let theme_name = if is_dark_theme {
        "base16-ocean.dark"
    } else {
        "base16-ocean.light"
    };
    let theme = &THEME_SET.themes[theme_name];
    let ss = &*SYNTAX_SET;

    let mut out = String::with_capacity(html.len());
    let mut flush_start: usize = 0;
    let mut parsed_tags: usize = 0;
    let mut highlighted_blocks: usize = 0;
    let mut pos: usize = 0;

    let bytes = html.as_bytes();

    #[derive(Clone, Copy)]
    enum State {
        Outside,
        AfterPreOpen,
        InCode,
        AfterCodeClose,
    }

    let mut state = State::Outside;
    let mut code_depth: usize = 0;
    let mut pre_tag = ParsedTag::default();
    let mut code_tag = ParsedTag::default();
    let mut code_close_tag = ParsedTag::default();

    while pos < bytes.len() {
        if bytes[pos] != b'<' {
            pos += 1;
            continue;
        }

        parsed_tags += 1;
        if parsed_tags > MAX_PARSED_TAGS {
            break;
        }

        let tag = parse_tag(html, pos);
        if !tag.valid {
            break;
        }

        let mut reevaluate = false;

        match state {
            State::Outside => {
                if !tag.special
                    && !tag.is_end
                    && !tag.self_closing
                    && tag_name_eq(html, &tag, "pre")
                {
                    pre_tag = tag;
                    state = State::AfterPreOpen;
                }
            }
            State::AfterPreOpen => {
                if !is_whitespace_only(html, pre_tag.end, tag.start) {
                    state = State::Outside;
                    reevaluate = true;
                } else if !tag.special
                    && !tag.is_end
                    && !tag.self_closing
                    && tag_name_eq(html, &tag, "code")
                {
                    code_tag = tag;
                    code_depth = 1;
                    state = State::InCode;
                } else {
                    state = State::Outside;
                    reevaluate = true;
                }
            }
            State::InCode => {
                if !tag.special && tag_name_eq(html, &tag, "code") {
                    if !tag.is_end && !tag.self_closing {
                        code_depth += 1;
                    } else if tag.is_end {
                        code_depth -= 1;
                        if code_depth == 0 {
                            code_close_tag = tag;
                            state = State::AfterCodeClose;
                        }
                    }
                }
            }
            State::AfterCodeClose => {
                if !is_whitespace_only(html, code_close_tag.end, tag.start) {
                    state = State::Outside;
                    reevaluate = true;
                } else if !tag.special && tag.is_end && tag_name_eq(html, &tag, "pre") {
                    // Flush text before this <pre> block.
                    out.push_str(&html[flush_start..pre_tag.start]);

                    let original_block = &html[pre_tag.start..tag.end];
                    let pre_attrs = tag_attrs(html, &pre_tag);
                    let code_attrs = tag_attrs(html, &code_tag);
                    let code_body = &html[code_tag.end..code_close_tag.start];

                    let mut did_highlight = false;
                    if highlighted_blocks < MAX_HIGHLIGHTED_CODE_BLOCKS
                        && code_body.len() <= MAX_ENCODED_CODE_BLOCK_CHARS
                    {
                        let decoded = decode_html_entities(code_body);
                        if is_highlight_eligible(&decoded) {
                            let lang_token = extract_language_token(code_attrs);
                            let syntax = if lang_token.is_empty() {
                                detect_syntax_from_content(ss, &decoded)
                            } else {
                                resolve_syntax(ss, &lang_token)
                            };

                            if let Some(syn) = syntax {
                                if let Ok(highlighted) =
                                    highlighted_html_for_string(&decoded, ss, syn, theme)
                                {
                                    // syntect wraps in <pre style="..."><code>...</code></pre>;
                                    // strip that wrapper — we supply our own <pre><code> tags.
                                    let inner = strip_syntect_wrapper(&highlighted);
                                    out.push_str("<pre");
                                    out.push_str(pre_attrs);
                                    out.push_str("><code");
                                    out.push_str(code_attrs);
                                    out.push('>');
                                    out.push_str(inner);
                                    out.push_str("</code></pre>");
                                    highlighted_blocks += 1;
                                    did_highlight = true;
                                }
                            }
                        }
                    }

                    if !did_highlight {
                        out.push_str(original_block);
                    }

                    flush_start = tag.end;
                    state = State::Outside;
                } else {
                    state = State::Outside;
                    reevaluate = true;
                }
            }
        }

        if reevaluate {
            continue;
        }
        pos = tag.end;
    }

    out.push_str(&html[flush_start..]);
    out
}

/// Convenience wrapper: highlight raw JSON for the "View source" dialog.
pub(crate) fn highlight_raw_json(raw_json: &str, is_dark_theme: bool) -> String {
    let escaped = html_escape(raw_json);
    let wrapped = format!(
        "<pre><code class=\"language-json\">{escaped}</code></pre>"
    );
    highlight_formatted_code_blocks(&wrapped, is_dark_theme)
}

// ---------------------------------------------------------------------------
// Tag parsing
// ---------------------------------------------------------------------------

#[derive(Default, Clone, Copy)]
struct ParsedTag {
    valid: bool,
    special: bool,
    is_end: bool,
    self_closing: bool,
    start: usize,
    end: usize,
    name_start: usize,
    name_len: usize,
    attrs_start: usize,
    attrs_len: usize,
}

fn is_tag_name_char(b: u8) -> bool {
    b.is_ascii_alphanumeric() || b == b'-' || b == b'_' || b == b':'
}

fn parse_tag(html: &str, tag_start: usize) -> ParsedTag {
    let bytes = html.as_bytes();
    let mut tag = ParsedTag {
        start: tag_start,
        end: bytes.len(),
        ..Default::default()
    };

    if tag_start >= bytes.len() || bytes[tag_start] != b'<' {
        return tag;
    }

    // Find closing '>'.
    let mut i = tag_start + 1;
    let mut quote: u8 = 0;
    while i < bytes.len() {
        let c = bytes[i];
        if quote == 0 {
            if c == b'"' || c == b'\'' {
                quote = c;
            } else if c == b'>' {
                break;
            }
        } else if c == quote {
            quote = 0;
        }
        i += 1;
    }

    if i >= bytes.len() {
        return tag;
    }

    tag.end = i + 1;

    let mut cs = tag_start + 1;
    let mut ce = i;
    while cs < ce && bytes[cs].is_ascii_whitespace() {
        cs += 1;
    }
    while ce > cs && bytes[ce - 1].is_ascii_whitespace() {
        ce -= 1;
    }
    if cs >= ce {
        return tag;
    }

    if bytes[cs] == b'!' || bytes[cs] == b'?' {
        tag.valid = true;
        tag.special = true;
        return tag;
    }

    let mut cursor = cs;
    if bytes[cursor] == b'/' {
        tag.is_end = true;
        cursor += 1;
        while cursor < ce && bytes[cursor].is_ascii_whitespace() {
            cursor += 1;
        }
    }

    let name_start = cursor;
    while cursor < ce && is_tag_name_char(bytes[cursor]) {
        cursor += 1;
    }
    if cursor == name_start {
        return tag;
    }

    tag.name_start = name_start;
    tag.name_len = cursor - name_start;

    if !tag.is_end {
        let attrs_start = cursor;
        let mut attrs_end = ce;
        while attrs_end > attrs_start && bytes[attrs_end - 1].is_ascii_whitespace() {
            attrs_end -= 1;
        }
        if attrs_end > attrs_start && bytes[attrs_end - 1] == b'/' {
            tag.self_closing = true;
            attrs_end -= 1;
            while attrs_end > attrs_start && bytes[attrs_end - 1].is_ascii_whitespace() {
                attrs_end -= 1;
            }
        }
        tag.attrs_start = attrs_start;
        tag.attrs_len = attrs_end.saturating_sub(attrs_start);
    }

    tag.valid = true;
    tag
}

fn tag_name_eq(html: &str, tag: &ParsedTag, wanted: &str) -> bool {
    if !tag.valid || tag.name_len == 0 {
        return false;
    }
    html[tag.name_start..tag.name_start + tag.name_len].eq_ignore_ascii_case(wanted)
}

fn tag_attrs<'a>(html: &'a str, tag: &ParsedTag) -> &'a str {
    if tag.attrs_len == 0 {
        return "";
    }
    &html[tag.attrs_start..tag.attrs_start + tag.attrs_len]
}

fn is_whitespace_only(html: &str, start: usize, end: usize) -> bool {
    html[start..end].bytes().all(|b| b.is_ascii_whitespace())
}

// ---------------------------------------------------------------------------
// HTML entity decoding
// ---------------------------------------------------------------------------

fn decode_html_entities(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let mut pos = 0;

    while pos < s.len() {
        if s.as_bytes()[pos] != b'&' {
            // Advance by one full UTF-8 character, not one byte.
            let ch = &s[pos..];
            let c = ch.chars().next().unwrap();
            out.push(c);
            pos += c.len_utf8();
            continue;
        }
        // Find ';'
        let remaining = &s[pos..];
        if let Some(semi) = remaining.find(';') {
            if semi > 10 {
                // Too long for a valid entity — emit '&' and move on.
                out.push('&');
                pos += 1;
                continue;
            }
            let entity = &remaining[1..semi];
            if let Some(decoded) = decode_entity(entity) {
                out.push(decoded);
                pos += semi + 1;
            } else {
                out.push('&');
                pos += 1;
            }
        } else {
            out.push('&');
            pos += 1;
        }
    }
    out
}

fn decode_entity(entity: &str) -> Option<char> {
    match entity {
        "lt" => Some('<'),
        "gt" => Some('>'),
        "amp" => Some('&'),
        "quot" => Some('"'),
        "apos" => Some('\''),
        "nbsp" => Some('\u{00a0}'),
        _ if entity.starts_with('#') => {
            let num_str = &entity[1..];
            let code = if let Some(hex) = num_str.strip_prefix('x').or_else(|| num_str.strip_prefix('X')) {
                u32::from_str_radix(hex, 16).ok()?
            } else {
                num_str.parse::<u32>().ok()?
            };
            char::from_u32(code)
        }
        _ => None,
    }
}

fn html_escape(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for c in s.chars() {
        match c {
            '<' => out.push_str("&lt;"),
            '>' => out.push_str("&gt;"),
            '&' => out.push_str("&amp;"),
            '"' => out.push_str("&quot;"),
            '\'' => out.push_str("&#39;"),
            _ => out.push(c),
        }
    }
    out
}

// ---------------------------------------------------------------------------
// Language detection & resolution
// ---------------------------------------------------------------------------

fn extract_language_token(code_attrs: &str) -> String {
    if code_attrs.is_empty() || code_attrs.len() > MAX_ATTRIBUTE_CHARS {
        return String::new();
    }

    let bytes = code_attrs.as_bytes();
    let mut cursor = 0;

    while cursor < bytes.len() {
        // Skip whitespace.
        while cursor < bytes.len() && bytes[cursor].is_ascii_whitespace() {
            cursor += 1;
        }
        if cursor >= bytes.len() {
            break;
        }

        // Read attribute name.
        let name_start = cursor;
        while cursor < bytes.len()
            && !bytes[cursor].is_ascii_whitespace()
            && bytes[cursor] != b'='
        {
            cursor += 1;
        }
        if cursor == name_start {
            break;
        }
        let name = &code_attrs[name_start..cursor];

        // Skip whitespace.
        while cursor < bytes.len() && bytes[cursor].is_ascii_whitespace() {
            cursor += 1;
        }

        let mut value = String::new();
        if cursor < bytes.len() && bytes[cursor] == b'=' {
            cursor += 1;
            while cursor < bytes.len() && bytes[cursor].is_ascii_whitespace() {
                cursor += 1;
            }

            if cursor < bytes.len() && (bytes[cursor] == b'"' || bytes[cursor] == b'\'') {
                let quote = bytes[cursor];
                cursor += 1;
                let val_start = cursor;
                while cursor < bytes.len() && bytes[cursor] != quote {
                    cursor += 1;
                }
                value = code_attrs[val_start..cursor].to_owned();
                if cursor < bytes.len() {
                    cursor += 1;
                }
            } else {
                let val_start = cursor;
                while cursor < bytes.len() && !bytes[cursor].is_ascii_whitespace() {
                    cursor += 1;
                }
                value = code_attrs[val_start..cursor].to_owned();
            }
        }

        if name.eq_ignore_ascii_case("class") {
            return extract_language_from_class_value(&value);
        }
    }

    String::new()
}

fn extract_language_from_class_value(class_value: &str) -> String {
    for token in class_value.split_whitespace() {
        let lang = token.to_ascii_lowercase();
        let lang = if let Some(rest) = lang.strip_prefix("language-") {
            rest
        } else if let Some(rest) = lang.strip_prefix("lang-") {
            rest
        } else {
            continue;
        };

        if lang.is_empty() || lang.len() > MAX_LANGUAGE_TOKEN_CHARS {
            continue;
        }

        let valid = lang
            .bytes()
            .all(|b| b.is_ascii_alphanumeric() || b == b'+' || b == b'#' || b == b'-' || b == b'_' || b == b'.');
        if valid {
            return lang.to_owned();
        }
    }
    String::new()
}

fn resolve_syntax<'a>(ss: &'a SyntaxSet, token: &str) -> Option<&'a SyntaxReference> {
    if token.is_empty() {
        return None;
    }

    let lowered = token.to_ascii_lowercase();

    // Try by name (case-insensitive scan).
    if let Some(s) = ss.syntaxes().iter().find(|s| s.name.eq_ignore_ascii_case(token)) {
        return Some(s);
    }

    // Alias mapping.
    let extension = match lowered.as_str() {
        "c++" => "cpp",
        "c#" => "cs",
        "shell" | "shell-session" => "sh",
        "yml" => "yaml",
        _ => &lowered,
    };

    // Try by extension.
    if let Some(s) = ss.find_syntax_by_extension(extension) {
        return Some(s);
    }

    // Try lowered name again.
    if let Some(s) = ss.syntaxes().iter().find(|s| s.name.eq_ignore_ascii_case(&lowered)) {
        return Some(s);
    }

    // Special diff/patch handling.
    if lowered == "diff" || lowered == "patch" {
        if let Some(s) = ss.find_syntax_by_extension("diff") {
            return Some(s);
        }
        // Also try by name "Diff".
        return ss.syntaxes().iter().find(|s| s.name == "Diff");
    }

    None
}

fn detect_syntax_from_content<'a>(ss: &'a SyntaxSet, code: &str) -> Option<&'a SyntaxReference> {
    let token = detect_language_token(code);
    if !token.is_empty() {
        return resolve_syntax(ss, &token);
    }
    None
}

fn detect_language_token(code: &str) -> String {
    let trimmed = code.trim();
    if trimmed.is_empty() {
        return String::new();
    }

    if trimmed.starts_with("<?php") {
        return "php".to_owned();
    }

    if trimmed.starts_with("#!/") {
        let first_line = trimmed.lines().next().unwrap_or("").to_ascii_lowercase();
        if first_line.contains("python") {
            return "python".to_owned();
        }
        if first_line.contains("bash") || first_line.contains("zsh") || first_line.contains("/sh")
        {
            return "bash".to_owned();
        }
        if first_line.contains("node") {
            return "javascript".to_owned();
        }
    }

    // Diff detection.
    for line in trimmed.lines() {
        if line.starts_with("diff --git")
            || line.starts_with("@@ ")
            || line.starts_with("--- ")
            || line.starts_with("+++ ")
        {
            return "diff".to_owned();
        }
    }

    // JSON detection.
    if trimmed.starts_with('{') || trimmed.starts_with('[') {
        if serde_json::from_str::<serde_json::Value>(trimmed).is_ok() {
            return "json".to_owned();
        }
    }

    // XML detection.
    if trimmed.starts_with("<?xml")
        || (trimmed.starts_with('<') && trimmed.ends_with('>'))
    {
        return "xml".to_owned();
    }

    String::new()
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

fn is_highlight_eligible(decoded: &str) -> bool {
    if decoded.len() > MAX_DECODED_CODE_BLOCK_CHARS {
        return false;
    }
    let line_count = decoded.lines().count().max(1);
    line_count <= MAX_DECODED_CODE_BLOCK_LINES
}

/// syntect's `highlighted_html_for_string` wraps output in
/// `<pre style="background-color:#xxx;">CONTENT</pre>\n`.
///
/// Note: syntect's output has **no inner `<code>`** — it's just `<pre>` wrapping
/// the highlighted `<span>`s. We strip that wrapper because we supply our own
/// `<pre><code>` tags with the original attributes preserved. Leaving the
/// wrapper in place produces nested `<pre>` elements: our outer one paints the
/// bubble's alternate-base background, while syntect's inner one paints its
/// own theme background, giving the message bubble a stacked "double rectangle"
/// look — issue #155.
fn strip_syntect_wrapper(html: &str) -> &str {
    // Skip past the opening `<pre …>`.
    let Some(pre_start) = html.find("<pre") else {
        return html;
    };
    let Some(gt_off) = html[pre_start..].find('>') else {
        return html;
    };
    let mut inner_start = pre_start + gt_off + 1;

    // syntect emits a literal newline immediately after the opening tag (and
    // sometimes another one before the closing tag). Our outer `<pre>` is
    // whitespace-preserving, so those newlines would render as blank lines
    // at the top/bottom of the highlighted block.
    if html.as_bytes().get(inner_start) == Some(&b'\n') {
        inner_start += 1;
    }

    let Some(mut inner_end) = html.rfind("</pre>") else {
        return &html[inner_start..];
    };
    while inner_end > inner_start && html.as_bytes()[inner_end - 1] == b'\n' {
        inner_end -= 1;
    }

    if inner_start <= inner_end {
        &html[inner_start..inner_end]
    } else {
        html
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests;
