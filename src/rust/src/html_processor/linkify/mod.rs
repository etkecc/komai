// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Auto-link plain URLs (http/https/matrix:) in raw HTML.

use linkify::{LinkFinder, LinkKind};

use super::parser::{ParsedTag, is_void_tag, parse_tag, tag_name_lower};

/// Convert plain URLs in a text segment (outside HTML tags) into `<a>` links.
fn linkify_text_segment(text: &str) -> String {
    // Collect standard URL spans from linkify (http/https only).
    let mut finder = LinkFinder::new();
    finder.kinds(&[LinkKind::Url]);

    let mut spans: Vec<(usize, usize)> = Vec::new();

    for link in finder.links(text) {
        let url = link.as_str();
        // Only linkify http/https URLs (matching C++ behavior).
        let colon = match url.find(':') {
            Some(c) => c,
            None => continue,
        };
        let scheme = &url[..colon];
        if !scheme.eq_ignore_ascii_case("http") && !scheme.eq_ignore_ascii_case("https") {
            continue;
        }
        spans.push((link.start(), link.end()));
    }

    // Collect matrix: URI spans.
    {
        let text_bytes = text.as_bytes();
        let mut search = 0;
        while search < text_bytes.len() {
            let idx = match text[search..].find("matrix:") {
                Some(i) => search + i,
                None => break,
            };

            // Word boundary: preceding char must not be alphanumeric or a quote.
            if idx > 0 {
                let prev = text_bytes[idx - 1];
                if prev.is_ascii_alphanumeric() || prev == b'"' || prev == b'\'' {
                    search = idx + 7;
                    continue;
                }
            }

            // Collect non-whitespace chars.
            let mut end = idx + 7;
            while end < text_bytes.len() && !text_bytes[end].is_ascii_whitespace() {
                end += 1;
            }

            // Need at least 5 chars after "matrix:" (total >= 12).
            if end - idx < 12 {
                search = end;
                continue;
            }

            // Trim trailing quotes/punctuation that aren't part of the URI.
            while end > idx + 7 && matches!(text_bytes[end - 1], b'"' | b'\'') {
                end -= 1;
            }

            if end - idx >= 12 {
                // Check no overlap with existing spans.
                let overlaps = spans.iter().any(|(s, e)| idx < *e && end > *s);
                if !overlaps {
                    spans.push((idx, end));
                }
            }

            search = end;
        }
    }

    if spans.is_empty() {
        return text.to_string();
    }

    spans.sort_by_key(|(start, _)| *start);

    let mut out = String::with_capacity(text.len() + text.len() / 4);
    let mut last = 0;
    for (start, end) in &spans {
        out.push_str(&text[last..*start]);
        let url = &text[*start..*end];
        out.push_str("<a href=\"");
        out.push_str(url);
        out.push_str("\">");
        out.push_str(url);
        out.push_str("</a>");
        last = *end;
    }
    out.push_str(&text[last..]);
    out
}

pub(crate) fn linkify_html(html: &str) -> String {
    if html.is_empty() {
        return html.to_string();
    }

    let mut out = String::with_capacity(html.len() + html.len() / 4);
    let bytes = html.as_bytes();
    let mut pos = 0;
    let mut anchor_depth: usize = 0;
    let mut pre_depth: usize = 0;
    let mut code_depth: usize = 0;

    while pos < bytes.len() {
        let next_lt = match html[pos..].find('<') {
            Some(idx) => pos + idx,
            None => {
                let text = &html[pos..];
                if anchor_depth == 0 && pre_depth == 0 && code_depth == 0 {
                    out.push_str(&linkify_text_segment(text));
                } else {
                    out.push_str(text);
                }
                break;
            }
        };

        let text = &html[pos..next_lt];
        if anchor_depth == 0 && pre_depth == 0 && code_depth == 0 {
            out.push_str(&linkify_text_segment(text));
        } else {
            out.push_str(text);
        }

        let tag = parse_tag(html, next_lt);
        if !tag.valid {
            out.push('<');
            pos = next_lt + 1;
            continue;
        }

        let tag_name = tag_name_lower(html, &tag);
        out.push_str(&html[tag.start..tag.end]);

        if !tag.special {
            let update_depth =
                |tag: &ParsedTag, name: &str, counter: &mut usize| {
                    if tag.self_closing || is_void_tag(name) {
                        return;
                    }
                    if tag.is_end {
                        if *counter > 0 {
                            *counter -= 1;
                        }
                    } else {
                        *counter += 1;
                    }
                };

            match tag_name.as_str() {
                "a" => update_depth(&tag, &tag_name, &mut anchor_depth),
                "pre" => update_depth(&tag, &tag_name, &mut pre_depth),
                "code" => update_depth(&tag, &tag_name, &mut code_depth),
                _ => {}
            }
        }

        pos = tag.end;
    }

    out
}

#[cfg(test)]
mod tests;
