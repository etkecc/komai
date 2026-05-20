// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Hand-rolled HTML tag scanner: locates and classifies opening/closing
//! tag spans without owning the input. (Functionally close to
//! `html_processor::parser`; left untouched here so the orchestration
//! layer above can call into it without changing call sites — a future
//! pass could de-dupe.)


// ---------------------------------------------------------------------------
// Tag parsing
// ---------------------------------------------------------------------------

#[derive(Default, Clone, Copy)]
pub(super) struct ParsedTag {
    pub(super) valid: bool,
    pub(super) special: bool,
    pub(super) is_end: bool,
    pub(super) self_closing: bool,
    pub(super) start: usize,
    pub(super) end: usize,
    pub(super) name_start: usize,
    pub(super) name_len: usize,
    pub(super) attrs_start: usize,
    pub(super) attrs_len: usize,
}

pub(super) fn is_tag_name_char(b: u8) -> bool {
    b.is_ascii_alphanumeric() || b == b'-' || b == b'_' || b == b':'
}

pub(super) fn parse_tag(html: &str, tag_start: usize) -> ParsedTag {
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

pub(super) fn tag_name_eq(html: &str, tag: &ParsedTag, wanted: &str) -> bool {
    if !tag.valid || tag.name_len == 0 {
        return false;
    }
    html[tag.name_start..tag.name_start + tag.name_len].eq_ignore_ascii_case(wanted)
}

pub(super) fn tag_attrs<'a>(html: &'a str, tag: &ParsedTag) -> &'a str {
    if tag.attrs_len == 0 {
        return "";
    }
    &html[tag.attrs_start..tag.attrs_start + tag.attrs_len]
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
