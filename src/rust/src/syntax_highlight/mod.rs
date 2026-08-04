// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Syntax highlighting for `<pre><code>` blocks in HTML messages.
//!
//! Replaces the former C++/KF6SyntaxHighlighting implementation with a pure-Rust
//! pipeline built on [syntect](https://docs.rs/syntect).

use std::sync::LazyLock;

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

// ---------------------------------------------------------------------------
// Public entry point (called from C++ via CXX)
// ---------------------------------------------------------------------------

/// Process an HTML fragment, highlighting every eligible `<pre><code>` block.
///
/// * `code_background` – the `#rrggbb` color the rendered code block sits on.
///   Token colors are picked to stay legible against it, so callers must pass
///   the surface they will actually paint.
pub(crate) fn highlight_formatted_code_blocks(html: &str, code_background: &str) -> String {
    if html.is_empty() || html.len() > MAX_HTML_CHARS {
        return html.to_owned();
    }

    let theme = palette::theme_for_background(palette::parse_background(code_background));
    let theme = &theme;
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
pub(crate) fn highlight_raw_json(raw_json: &str, code_background: &str) -> String {
    let escaped = html_escape(raw_json);
    let wrapped = format!(
        "<pre><code class=\"language-json\">{escaped}</code></pre>"
    );
    highlight_formatted_code_blocks(&wrapped, code_background)
}

// ---------------------------------------------------------------------------
// Tag parsing
// ---------------------------------------------------------------------------


mod language;
mod palette;
mod parser;

use language::{detect_syntax_from_content, extract_language_token, resolve_syntax};
use parser::{
    ParsedTag, decode_html_entities, html_escape, is_whitespace_only, parse_tag, tag_attrs,
    tag_name_eq,
};

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
