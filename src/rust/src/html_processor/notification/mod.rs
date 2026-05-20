// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Normalize Matrix `formatted_body` into the freedesktop notification
//! `body-markup` subset (`<b>`, `<i>`, `<u>`, `<a href>`).
//!
//! Other tags (paragraphs, lists, headings, blockquotes, code, ...) are
//! converted to newlines and bullet markers where reasonable; everything
//! unknown drops the tag and keeps the content. The `<mx-reply>` subtree
//! is removed entirely.

use super::parser::{ParsedTag, parse_attributes, parse_tag, tag_name_lower};
use super::util::html_escape;

const MAX_NOTIFICATION_CHARS: usize = 1024;
const MAX_HREF_CHARS: usize = 2048;

pub(crate) fn to_notification_markup(formatted_body: &str) -> String {
    if formatted_body.is_empty() {
        return String::new();
    }

    let mut out = String::with_capacity(formatted_body.len());
    let bytes = formatted_body.as_bytes();
    let mut pos = 0;
    let mut mx_reply_depth: usize = 0;
    let mut suppressed_a_depth: usize = 0;

    while pos < bytes.len() {
        let next_lt = match formatted_body[pos..].find('<') {
            Some(idx) => pos + idx,
            None => {
                if mx_reply_depth == 0 {
                    out.push_str(&formatted_body[pos..]);
                }
                break;
            }
        };

        if mx_reply_depth == 0 {
            out.push_str(&formatted_body[pos..next_lt]);
        }

        let tag = parse_tag(formatted_body, next_lt);
        if !tag.valid {
            if mx_reply_depth == 0 {
                out.push_str("&lt;");
            }
            pos = next_lt + 1;
            continue;
        }

        let tag_name = tag_name_lower(formatted_body, &tag);

        if tag_name == "mx-reply" {
            if !tag.is_end {
                mx_reply_depth += 1;
            } else if mx_reply_depth > 0 {
                mx_reply_depth -= 1;
            }
            pos = tag.end;
            continue;
        }

        if mx_reply_depth > 0 {
            pos = tag.end;
            continue;
        }

        if tag.special {
            pos = tag.end;
            continue;
        }

        emit_tag(
            &mut out,
            formatted_body,
            &tag,
            &tag_name,
            &mut suppressed_a_depth,
        );
        pos = tag.end;
    }

    finalize(&out)
}

fn emit_tag(
    out: &mut String,
    html: &str,
    tag: &ParsedTag,
    name: &str,
    suppressed_a_depth: &mut usize,
) {
    match name {
        "b" | "i" | "u" => {
            out.push('<');
            if tag.is_end {
                out.push('/');
            }
            out.push_str(name);
            out.push('>');
        }
        "strong" => {
            out.push_str(if tag.is_end { "</b>" } else { "<b>" });
        }
        "em" => {
            out.push_str(if tag.is_end { "</i>" } else { "<i>" });
        }
        "a" => {
            if tag.is_end {
                if *suppressed_a_depth > 0 {
                    *suppressed_a_depth -= 1;
                } else {
                    out.push_str("</a>");
                }
                return;
            }
            let attrs = parse_attributes(html, tag);
            let href = attrs
                .iter()
                .find(|a| a.name == "href" && a.has_value)
                .and_then(|a| validate_notification_href(&a.value));
            match href {
                Some(href) => {
                    out.push_str("<a href=\"");
                    out.push_str(&html_escape(&href));
                    out.push_str("\">");
                }
                None => *suppressed_a_depth += 1,
            }
        }
        "br" => out.push('\n'),
        "p" if tag.is_end => out.push_str("\n\n"),
        "li" => {
            if tag.is_end {
                out.push('\n');
            } else {
                out.push_str("• ");
            }
        }
        "h1" | "h2" | "h3" | "h4" | "h5" | "h6" if tag.is_end => out.push_str("\n\n"),
        "blockquote" if tag.is_end => out.push_str("\n\n"),
        "pre" if tag.is_end => out.push_str("\n\n"),
        // Everything else: drop the tag, keep the inner content as it streams
        // through the outer loop. This covers `<ul>`, `<ol>`, `<code>` (inline),
        // `<del>`/`<s>`/`<strike>`, `<span>`, `<font>`, `<div>`, `<sup>`/`<sub>`,
        // `<details>`/`<summary>`, table machinery, headings/blockquotes/paragraphs
        // on the opening side, plus anything unknown.
        _ => {}
    }
}

fn validate_notification_href(href: &str) -> Option<String> {
    let v = href.trim();
    if v.is_empty() || v.len() > MAX_HREF_CHARS {
        return None;
    }
    let colon = v.find(':')?;
    if colon == 0 {
        return None;
    }
    let scheme = &v[..colon];
    if !scheme.bytes().all(|b| b.is_ascii_alphabetic()) {
        return None;
    }
    let scheme_lower = scheme.to_ascii_lowercase();
    if !matches!(
        scheme_lower.as_str(),
        "http" | "https" | "mailto" | "matrix"
    ) {
        return None;
    }
    let rest = &v[colon + 1..];
    if rest.is_empty() {
        return None;
    }
    if matches!(scheme_lower.as_str(), "http" | "https") && !rest.starts_with("//") {
        return None;
    }
    Some(v.to_string())
}

fn finalize(s: &str) -> String {
    // Collapse runs of 3+ newlines down to 2 (paragraph separators stay,
    // longer gaps don't). Trim trailing spaces before each newline so
    // wrap whitespace from intra-tag formatting doesn't survive.
    let mut out = String::with_capacity(s.len());
    let mut newline_run: usize = 0;
    for ch in s.chars() {
        if ch == '\n' {
            while out.ends_with(' ') || out.ends_with('\t') {
                out.pop();
            }
            newline_run += 1;
            if newline_run <= 2 {
                out.push('\n');
            }
        } else {
            newline_run = 0;
            out.push(ch);
        }
    }

    let trimmed = out.trim_matches(|c: char| c.is_whitespace()).to_string();

    // Cap by character count, not byte count. A trailing ellipsis signals
    // truncation to the user.
    let char_count = trimmed.chars().count();
    if char_count > MAX_NOTIFICATION_CHARS {
        let mut capped: String = trimmed.chars().take(MAX_NOTIFICATION_CHARS - 1).collect();
        capped.push('…');
        capped
    } else {
        trimmed
    }
}

#[cfg(test)]
mod tests;
