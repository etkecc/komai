// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Local HTML helpers used by the syntect pipeline: a thin accessor on
//! top of the shared [`crate::html_processor::parser`] tag scanner, plus
//! HTML entity decoding and HTML escaping geared toward inline source
//! rendering (escapes the apostrophe in addition to the four base chars).

pub(super) use crate::html_processor::parser::{ParsedTag, parse_tag};

pub(super) fn tag_name_eq(html: &str, tag: &ParsedTag, wanted: &str) -> bool {
    if !tag.valid || tag.name_len == 0 {
        return false;
    }
    html[tag.name_start..tag.name_start + tag.name_len].eq_ignore_ascii_case(wanted)
}

pub(super) fn tag_attrs<'a>(html: &'a str, tag: &ParsedTag) -> &'a str {
    if tag.attrs_end <= tag.attrs_start {
        return "";
    }
    &html[tag.attrs_start..tag.attrs_end]
}

pub(super) fn is_whitespace_only(html: &str, start: usize, end: usize) -> bool {
    html[start..end].bytes().all(|b| b.is_ascii_whitespace())
}

// ---------------------------------------------------------------------------
// HTML entity decoding
// ---------------------------------------------------------------------------

pub(super) fn decode_html_entities(s: &str) -> String {
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

pub(super) fn decode_entity(entity: &str) -> Option<char> {
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

pub(super) fn html_escape(s: &str) -> String {
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
