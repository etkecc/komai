// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Wrap occurrences of a search query inside rendered HTML.

use super::parser::{ParsedTag, parse_attributes, parse_tag, tag_name_lower};

/// Find every ASCII-case-insensitive occurrence of `needle` inside `haystack`,
/// returning byte spans. Skips matches that don't land on UTF-8 char
/// boundaries, so multi-byte text is preserved intact.
fn find_search_match_spans(haystack: &str, needle: &str) -> Vec<(usize, usize)> {
    let mut out = Vec::new();
    let nlen = needle.len();
    if nlen == 0 || haystack.len() < nlen {
        return out;
    }
    let h = haystack.as_bytes();
    let n = needle.as_bytes();
    let mut i = 0;
    while i + nlen <= h.len() {
        let mut matched = true;
        for j in 0..nlen {
            if !h[i + j].eq_ignore_ascii_case(&n[j]) {
                matched = false;
                break;
            }
        }
        if matched
            && haystack.is_char_boundary(i)
            && haystack.is_char_boundary(i + nlen)
        {
            out.push((i, i + nlen));
            i += nlen;
        } else {
            i += 1;
        }
    }
    out
}

/// Wrap each occurrence of `query` inside `text` with `<span class="komai-search-match">…</span>`.
/// If no occurrence is found and `wrap_all_when_no_match` is true,
/// the whole non-empty text is wrapped instead (used to flag link-target
/// matches whose visible text doesn't contain the query).
fn mark_text_segment(text: &str, query: &str, wrap_all_when_no_match: bool) -> String {
    let spans = find_search_match_spans(text, query);
    if spans.is_empty() {
        if wrap_all_when_no_match && !text.trim().is_empty() {
            let mut out = String::with_capacity(text.len() + 13);
            out.push_str("<span class=\"komai-search-match\">");
            out.push_str(text);
            out.push_str("</span>");
            return out;
        }
        return text.to_string();
    }

    let mut out = String::with_capacity(text.len() + spans.len() * 13);
    let mut last = 0;
    for (s, e) in &spans {
        out.push_str(&text[last..*s]);
        out.push_str("<span class=\"komai-search-match\">");
        out.push_str(&text[*s..*e]);
        out.push_str("</span>");
        last = *e;
    }
    out.push_str(&text[last..]);
    out
}

fn anchor_href_value(html: &str, tag: &ParsedTag) -> Option<String> {
    parse_attributes(html, tag)
        .into_iter()
        .find(|a| a.has_value && a.name == "href")
        .map(|a| a.value)
}

/// Wrap occurrences of the search `query` inside the visible text of `html`
/// with `<span class="komai-search-match">…</span>`. Tag bytes (names, attributes, quoted values) are
/// passed through untouched, so the HTML structure is preserved.
///
/// `<a href>` matches are surfaced specially: when an anchor's href contains
/// the query but none of its visible text does, the entire anchor's text is
/// wrapped so the user sees *why* the result row matched.
pub(crate) fn mark_search_matches(html: &str, query: &str) -> String {
    if html.is_empty() || query.is_empty() {
        return html.to_string();
    }

    let mut out = String::with_capacity(html.len() + html.len() / 8);
    let bytes = html.as_bytes();
    let mut pos = 0;
    // Stack of "this anchor's href matches the query" flags, one entry per
    // currently open `<a>`. The innermost (top) entry decides whether to
    // wrap-all-on-no-match for text inside this anchor.
    let mut anchor_match_stack: Vec<bool> = Vec::new();

    let emit_text = |out: &mut String, text: &str, stack: &[bool]| {
        let wrap_all = stack.last().copied().unwrap_or(false);
        out.push_str(&mark_text_segment(text, query, wrap_all));
    };

    while pos < bytes.len() {
        let next_lt = match html[pos..].find('<') {
            Some(idx) => pos + idx,
            None => {
                emit_text(&mut out, &html[pos..], &anchor_match_stack);
                break;
            }
        };

        emit_text(&mut out, &html[pos..next_lt], &anchor_match_stack);

        let tag = parse_tag(html, next_lt);
        if !tag.valid {
            out.push('<');
            pos = next_lt + 1;
            continue;
        }

        out.push_str(&html[tag.start..tag.end]);

        if !tag.special && tag_name_lower(html, &tag) == "a" {
            if tag.is_end {
                anchor_match_stack.pop();
            } else if !tag.self_closing {
                let href_matches = anchor_href_value(html, &tag)
                    .map(|href| !find_search_match_spans(&href, query).is_empty())
                    .unwrap_or(false);
                anchor_match_stack.push(href_matches);
            }
        }

        pos = tag.end;
    }

    out
}

#[cfg(test)]
mod tests;
