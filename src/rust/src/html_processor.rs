// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! HTML processing pipeline for Matrix timeline messages.
//!
//! Ports the C++ `HtmlProcessor` (sanitize, linkify, pill decoration,
//! plain-text-to-HTML) to Rust so processing happens before the FFI boundary.

use std::collections::HashMap;

use linkify::{LinkFinder, LinkKind};

use crate::ffi::HtmlPillAvatar;

// ---------------------------------------------------------------------------
// Constants (matching C++ HtmlProcessor limits)
// ---------------------------------------------------------------------------

const MAX_TAG_DEPTH: usize = 100;
const MAX_ATTRIBUTE_COUNT: usize = 64;
const MAX_ATTRIBUTE_VALUE_CHARS: usize = 4096;
const MAX_SPOILER_CHARS: usize = 512;
const MAX_MATH_CHARS: usize = 4096;
const MAX_TEXT_ATTRIBUTE_CHARS: usize = 1024;
const MAX_TARGET_CHARS: usize = 64;
const MAX_CLASS_CHARS: usize = 512;
const MAX_URL_CHARS: usize = 2048;

const MATRIX_TO_PREFIX: &str = "https://matrix.to/#/";

// ---------------------------------------------------------------------------
// HTML utilities
// ---------------------------------------------------------------------------

fn html_escape(s: &str) -> String {
    let mut out = String::with_capacity(s.len() + s.len() / 8);
    for c in s.chars() {
        match c {
            '&' => out.push_str("&amp;"),
            '<' => out.push_str("&lt;"),
            '>' => out.push_str("&gt;"),
            '"' => out.push_str("&quot;"),
            _ => out.push(c),
        }
    }
    out
}

fn percent_decode(input: &str) -> String {
    let bytes = input.as_bytes();
    let mut result = Vec::with_capacity(bytes.len());
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'%' && i + 2 < bytes.len() {
            let hi = bytes[i + 1];
            let lo = bytes[i + 2];
            if hi.is_ascii_hexdigit() && lo.is_ascii_hexdigit() {
                // unwrap safe: we just checked both are hex digits
                let byte = u8::from_str_radix(
                    std::str::from_utf8(&bytes[i + 1..i + 3]).unwrap(),
                    16,
                )
                .unwrap();
                result.push(byte);
                i += 3;
                continue;
            }
        }
        result.push(bytes[i]);
        i += 1;
    }
    String::from_utf8_lossy(&result).into_owned()
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
    attrs_end: usize,
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

    // Find closing '>', respecting quoted attribute values.
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

    // Trim whitespace from tag content.
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

    // Special tags (comments, processing instructions).
    if bytes[cs] == b'!' || bytes[cs] == b'?' {
        tag.valid = true;
        tag.special = true;
        return tag;
    }

    // Closing tag?
    let mut cursor = cs;
    if bytes[cursor] == b'/' {
        tag.is_end = true;
        cursor += 1;
        while cursor < ce && bytes[cursor].is_ascii_whitespace() {
            cursor += 1;
        }
    }

    // Tag name.
    let name_start = cursor;
    while cursor < ce && is_tag_name_char(bytes[cursor]) {
        cursor += 1;
    }
    if cursor == name_start {
        return tag;
    }

    tag.name_start = name_start;
    tag.name_len = cursor - name_start;

    // Attributes region (opening tags only).
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
        tag.attrs_end = attrs_end;
    }

    tag.valid = true;
    tag
}

fn tag_name_lower(html: &str, tag: &ParsedTag) -> String {
    if !tag.valid || tag.name_len == 0 {
        return String::new();
    }
    html[tag.name_start..tag.name_start + tag.name_len].to_ascii_lowercase()
}

// ---------------------------------------------------------------------------
// Attribute parsing
// ---------------------------------------------------------------------------

struct ParsedAttribute {
    name: String,
    value: String,
    has_value: bool,
}

fn parse_attributes(html: &str, tag: &ParsedTag) -> Vec<ParsedAttribute> {
    let mut attrs = Vec::new();
    if !tag.valid || tag.is_end || tag.attrs_end <= tag.attrs_start {
        return attrs;
    }

    let bytes = html.as_bytes();
    let mut cursor = tag.attrs_start;

    while cursor < tag.attrs_end && attrs.len() < MAX_ATTRIBUTE_COUNT {
        while cursor < tag.attrs_end && bytes[cursor].is_ascii_whitespace() {
            cursor += 1;
        }
        if cursor >= tag.attrs_end {
            break;
        }

        let name_start = cursor;
        while cursor < tag.attrs_end
            && !bytes[cursor].is_ascii_whitespace()
            && bytes[cursor] != b'='
        {
            cursor += 1;
        }
        if cursor == name_start {
            break;
        }

        let name = html[name_start..cursor].to_ascii_lowercase();

        while cursor < tag.attrs_end && bytes[cursor].is_ascii_whitespace() {
            cursor += 1;
        }

        if cursor < tag.attrs_end && bytes[cursor] == b'=' {
            cursor += 1;
            while cursor < tag.attrs_end && bytes[cursor].is_ascii_whitespace() {
                cursor += 1;
            }

            let value;
            if cursor < tag.attrs_end && (bytes[cursor] == b'"' || bytes[cursor] == b'\'') {
                let quote = bytes[cursor];
                cursor += 1;
                let value_start = cursor;
                while cursor < tag.attrs_end && bytes[cursor] != quote {
                    cursor += 1;
                }
                value = html[value_start..cursor].to_string();
                if cursor < tag.attrs_end {
                    cursor += 1;
                }
            } else {
                let value_start = cursor;
                while cursor < tag.attrs_end && !bytes[cursor].is_ascii_whitespace() {
                    cursor += 1;
                }
                value = html[value_start..cursor].to_string();
            }

            attrs.push(ParsedAttribute {
                name,
                value,
                has_value: true,
            });
        } else {
            attrs.push(ParsedAttribute {
                name,
                value: String::new(),
                has_value: false,
            });
        }
    }

    attrs
}

// ---------------------------------------------------------------------------
// Tag and attribute allow-lists / validators
// ---------------------------------------------------------------------------

fn is_void_tag(name: &str) -> bool {
    matches!(name, "br" | "hr" | "img")
}

fn is_allowed_tag(name: &str) -> bool {
    matches!(
        name,
        "font"
            | "del"
            | "h1"
            | "h2"
            | "h3"
            | "h4"
            | "h5"
            | "h6"
            | "blockquote"
            | "p"
            | "a"
            | "ul"
            | "ol"
            | "sup"
            | "sub"
            | "li"
            | "b"
            | "i"
            | "u"
            | "strong"
            | "em"
            | "s"
            | "strike"
            | "code"
            | "hr"
            | "br"
            | "div"
            | "table"
            | "thead"
            | "tbody"
            | "tr"
            | "th"
            | "td"
            | "caption"
            | "pre"
            | "span"
            | "img"
            | "details"
            | "summary"
    )
}

fn is_hex_color(s: &str) -> bool {
    s.len() == 7 && s.as_bytes()[0] == b'#' && s[1..].bytes().all(|b| b.is_ascii_hexdigit())
}

fn sanitize_hex_color(raw: &str) -> Option<String> {
    let v = raw.trim();
    if is_hex_color(v) {
        Some(v.to_string())
    } else {
        None
    }
}

fn sanitize_font_color(raw: &str) -> Option<String> {
    let v = raw.trim();
    if is_hex_color(v) {
        return Some(v.to_string());
    }
    let lower = v.to_ascii_lowercase();
    if matches!(
        lower.as_str(),
        "red" | "orange" | "yellow" | "green" | "warning" | "success" | "error"
    ) {
        Some(lower)
    } else {
        None
    }
}

fn sanitize_integer_string(raw: &str) -> Option<String> {
    let v = raw.trim();
    if !v.is_empty() && v.len() <= 5 && v.bytes().all(|b| b.is_ascii_digit()) {
        Some(v.to_string())
    } else {
        None
    }
}

fn is_allowed_href_scheme(scheme: &str) -> bool {
    let lower = scheme.to_ascii_lowercase();
    matches!(
        lower.as_str(),
        "https" | "http" | "ftp" | "mailto" | "magnet"
    )
}

fn sanitize_href(raw: &str) -> Option<String> {
    let v = raw.trim();
    if v.is_empty() || v.len() > MAX_URL_CHARS {
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
    if !is_allowed_href_scheme(scheme) {
        return None;
    }
    let rest = &v[colon + 1..];
    if rest.is_empty() {
        return None;
    }
    // http/https/ftp require ://
    let scheme_lower = scheme.to_ascii_lowercase();
    if matches!(scheme_lower.as_str(), "http" | "https" | "ftp") && !rest.starts_with("//") {
        return None;
    }
    Some(v.to_string())
}

fn sanitize_mxc_url(raw: &str) -> Option<String> {
    let v = raw.trim();
    if v.is_empty() || v.len() > MAX_URL_CHARS || v.len() < 7 {
        return None;
    }
    if !v[..3].eq_ignore_ascii_case("mxc") || !v[3..].starts_with("://") {
        return None;
    }
    let rest = &v[6..];
    if rest.is_empty() {
        return None;
    }
    // Rewrite mxc:// to the image://mxcImage/ scheme that litehtml can load.
    Some(format!("image://mxcImage/{rest}"))
}

fn sanitize_target(raw: &str) -> Option<String> {
    let v = raw.trim();
    if v.is_empty() || v.len() > MAX_TARGET_CHARS {
        return None;
    }
    if v.bytes()
        .all(|b| b.is_ascii_alphanumeric() || b == b'_' || b == b'-')
    {
        Some(v.to_string())
    } else {
        None
    }
}

fn sanitize_code_class(raw: &str) -> Option<String> {
    if raw.is_empty() || raw.len() > MAX_CLASS_CHARS {
        return None;
    }
    let kept: Vec<&str> = raw
        .split_ascii_whitespace()
        .filter(|token| {
            if let Some(rest) = token.strip_prefix("language-") {
                !rest.is_empty()
                    && rest.len() <= 64
                    && rest.bytes().all(|b| {
                        b.is_ascii_alphanumeric()
                            || b == b'#'
                            || b == b'+'
                            || b == b'.'
                            || b == b'_'
                            || b == b'-'
                    })
            } else {
                false
            }
        })
        .collect();
    if kept.is_empty() {
        None
    } else {
        Some(kept.join(" "))
    }
}

fn sanitize_attribute_value(tag_name: &str, attr: &ParsedAttribute) -> Option<String> {
    if !attr.has_value || attr.value.len() > MAX_ATTRIBUTE_VALUE_CHARS {
        return None;
    }

    match tag_name {
        "span" => match attr.name.as_str() {
            "data-mx-bg-color" => sanitize_hex_color(&attr.value),
            "data-mx-color" => sanitize_font_color(&attr.value),
            "data-mx-spoiler" => {
                if attr.value.len() <= MAX_SPOILER_CHARS {
                    Some(attr.value.clone())
                } else {
                    None
                }
            }
            "data-mx-maths" => {
                if attr.value.len() <= MAX_MATH_CHARS {
                    Some(attr.value.clone())
                } else {
                    None
                }
            }
            _ => None,
        },
        "a" => match attr.name.as_str() {
            "href" => sanitize_href(&attr.value),
            "target" => sanitize_target(&attr.value),
            _ => None,
        },
        "img" => match attr.name.as_str() {
            "src" => sanitize_mxc_url(&attr.value),
            "width" | "height" => sanitize_integer_string(&attr.value),
            "alt" | "title" => {
                if attr.value.len() <= MAX_TEXT_ATTRIBUTE_CHARS {
                    Some(attr.value.clone())
                } else {
                    None
                }
            }
            _ => None,
        },
        "ol" => {
            if attr.name == "start" {
                sanitize_integer_string(&attr.value)
            } else {
                None
            }
        }
        "code" => {
            if attr.name == "class" {
                sanitize_code_class(&attr.value)
            } else {
                None
            }
        }
        "div" => {
            if attr.name == "data-mx-maths" && attr.value.len() <= MAX_MATH_CHARS {
                Some(attr.value.clone())
            } else {
                None
            }
        }
        "font" => match attr.name.as_str() {
            "color" => sanitize_font_color(&attr.value),
            "data-mx-bg-color" | "data-mx-color" => sanitize_hex_color(&attr.value),
            _ => None,
        },
        _ => None,
    }
}

fn sanitize_tag_attributes(html: &str, tag: &ParsedTag, tag_name: &str) -> String {
    let attrs = parse_attributes(html, tag);
    let mut result = String::new();
    for attr in &attrs {
        if let Some(sanitized) = sanitize_attribute_value(tag_name, attr) {
            result.push(' ');
            result.push_str(&attr.name);
            result.push_str("=\"");
            result.push_str(&html_escape(&sanitized));
            result.push('"');
        }
    }
    result
}

// ---------------------------------------------------------------------------
// sanitize_html
// ---------------------------------------------------------------------------

pub(crate) fn sanitize_html(raw_html: &str) -> String {
    if raw_html.is_empty() {
        return raw_html.to_string();
    }

    let mut out = String::with_capacity(raw_html.len());
    let bytes = raw_html.as_bytes();
    let mut pos = 0;
    let mut depth: usize = 0;
    let mut mx_reply_depth: usize = 0;

    while pos < bytes.len() {
        let next_lt = match raw_html[pos..].find('<') {
            Some(idx) => pos + idx,
            None => {
                if mx_reply_depth == 0 {
                    out.push_str(&raw_html[pos..]);
                }
                break;
            }
        };

        if mx_reply_depth == 0 {
            out.push_str(&raw_html[pos..next_lt]);
        }

        let tag = parse_tag(raw_html, next_lt);
        if !tag.valid {
            if mx_reply_depth == 0 {
                out.push_str("&lt;");
            }
            pos = next_lt + 1;
            continue;
        }

        let tag_name = tag_name_lower(raw_html, &tag);

        // Strip <mx-reply> fallback blocks.
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

        // Escape disallowed or special tags.
        if tag.special || !is_allowed_tag(&tag_name) {
            out.push_str("&lt;");
            pos = next_lt + 1;
            continue;
        }

        // Closing tag.
        if tag.is_end {
            out.push_str("</");
            out.push_str(&tag_name);
            out.push('>');
            if depth > 0 && !is_void_tag(&tag_name) {
                depth -= 1;
            }
            pos = tag.end;
            continue;
        }

        // Depth limit for non-void opening tags.
        if !tag.self_closing && !is_void_tag(&tag_name) && depth >= MAX_TAG_DEPTH {
            out.push_str("&lt;");
            pos = next_lt + 1;
            continue;
        }

        // Emit sanitized opening tag.
        let attrs = sanitize_tag_attributes(raw_html, &tag, &tag_name);
        out.push('<');
        out.push_str(&tag_name);
        out.push_str(&attrs);
        if tag.self_closing || is_void_tag(&tag_name) {
            out.push_str("/>");
        } else {
            out.push('>');
        }

        if !tag.self_closing && !is_void_tag(&tag_name) {
            depth += 1;
        }

        pos = tag.end;
    }

    out
}

// ---------------------------------------------------------------------------
// linkify_html
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Matrix pill decoration
// ---------------------------------------------------------------------------

/// Extract the Matrix ID from a `matrix.to` href value.
fn matrix_id_from_href(href: &str) -> String {
    let fragment = match href.strip_prefix(MATRIX_TO_PREFIX) {
        Some(f) => f,
        None => return String::new(),
    };

    // Strip query string.
    let fragment = match fragment.find('?') {
        Some(pos) => &fragment[..pos],
        None => fragment,
    };

    // For event links like "!room:server/$event:server", keep only the first segment.
    let fragment = match fragment.find('/') {
        Some(pos) => &fragment[..pos],
        None => fragment,
    };

    percent_decode(fragment)
}

/// Determine the pill CSS class suffix from a Matrix ID sigil.
fn pill_class_for_id(matrix_id: &str) -> &'static str {
    match matrix_id.as_bytes().first() {
        Some(b'@') => "user",
        Some(b'#') | Some(b'!') => "room",
        _ => "",
    }
}

/// Percent-encode a string for safe inclusion as an opaque value inside a
/// URL query (`?key=...&...`). Only RFC 3986 unreserved characters pass
/// through verbatim — everything else, including `&`, `=`, `?`, `%`, `/`,
/// and `:`, is encoded as `%HH`. We need this for stuffing a full
/// `image://default-avatar/...` URL into the `fallback=` query of an
/// `image://mxcImage/...` URL: without encoding, the inner URL's own
/// `&` and `=` separators would bleed into the outer URL's query.
fn percent_encode_query_value(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for b in s.bytes() {
        match b {
            b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'-' | b'_' | b'.' | b'~' => {
                out.push(b as char);
            }
            _ => {
                out.push('%');
                out.push_str(&format!("{:02X}", b));
            }
        }
    }
    out
}

/// Convert `mxc://server/media_id` to `image://mxcImage/server/media_id?avatarSize=N&radius=25`.
/// When `fallback_url` is non-empty, append `&fallback=<percent-encoded>` so
/// LitehtmlContainer can pre-cache the default avatar under the mxc URL key
/// (and keep showing it if the mxc fetch fails). The fallback string is the
/// fully-formed `image://default-avatar/...` URL prepared in C++.
fn mxc_to_pill_avatar_url(mxc_url: &str, avatar_size: u32, fallback_url: &str) -> String {
    let rest = match mxc_url.strip_prefix("mxc://") {
        Some(rest) => rest,
        None => return String::new(),
    };
    let mut url = format!("image://mxcImage/{rest}?avatarSize={avatar_size}&radius=25");
    if !fallback_url.is_empty() {
        let sep = if fallback_url.contains('?') { '&' } else { '?' };
        let full_fallback = format!("{fallback_url}{sep}avatarSize={avatar_size}");
        url.push_str("&fallback=");
        url.push_str(&percent_encode_query_value(&full_fallback));
    }
    url
}

/// Append the requested logical avatar size to a pre-formed
/// `image://default-avatar/...` URL produced by the C++ side.
fn fallback_to_pill_avatar_url(fallback_url: &str, avatar_size: u32) -> String {
    if fallback_url.is_empty() {
        return String::new();
    }
    let sep = if fallback_url.contains('?') { '&' } else { '?' };
    format!("{fallback_url}{sep}avatarSize={avatar_size}")
}

/// Pick the best avatar source for a pill: a real mxc URL when the user
/// currently has an avatar, otherwise the default-avatar fallback URL the
/// C++ side prepared for them. Returns `None` when neither is available
/// (e.g. for a non-sender mention we have no profile snapshot for).
///
/// When both are available, the mxc URL carries the fallback piggybacked as
/// a percent-encoded `&fallback=` query so LitehtmlContainer can render the
/// default avatar while the mxc download is in flight (and keep it on
/// failure), mirroring Avatar.qml's behaviour in the timeline body.
fn pill_avatar_src(entry: &HtmlPillAvatar, avatar_size: u32) -> Option<String> {
    if entry.mxc_url.starts_with("mxc://") {
        Some(mxc_to_pill_avatar_url(
            &entry.mxc_url,
            avatar_size,
            &entry.fallback_url,
        ))
    } else if !entry.fallback_url.is_empty() {
        Some(fallback_to_pill_avatar_url(&entry.fallback_url, avatar_size))
    } else {
        None
    }
}

fn build_avatar_map<'a>(avatars: &'a [HtmlPillAvatar]) -> HashMap<&'a str, &'a HtmlPillAvatar> {
    let mut map = HashMap::with_capacity(avatars.len());
    for a in avatars {
        if a.user_id.is_empty() {
            continue;
        }
        if a.mxc_url.is_empty() && a.fallback_url.is_empty() {
            continue;
        }
        map.insert(a.user_id.as_str(), a);
    }
    map
}

fn decorate_matrix_pills(
    html: &str,
    avatar_map: &HashMap<&str, &HtmlPillAvatar>,
    avatar_size: u32,
) -> String {
    if html.is_empty() {
        return html.to_string();
    }

    let mut out = String::with_capacity(html.len() + html.len() / 4);
    let bytes = html.as_bytes();
    let mut pos = 0;

    while pos < bytes.len() {
        let next_lt = match html[pos..].find('<') {
            Some(idx) => pos + idx,
            None => {
                out.push_str(&html[pos..]);
                break;
            }
        };

        out.push_str(&html[pos..next_lt]);

        let tag = parse_tag(html, next_lt);
        if !tag.valid {
            out.push('<');
            pos = next_lt + 1;
            continue;
        }

        let tag_name = tag_name_lower(html, &tag);

        // Only process opening <a> tags with matrix.to hrefs.
        if tag_name != "a" || tag.is_end || tag.self_closing {
            out.push_str(&html[tag.start..tag.end]);
            pos = tag.end;
            continue;
        }

        let attrs = parse_attributes(html, &tag);
        let href = attrs
            .iter()
            .find(|a| a.name == "href")
            .map(|a| a.value.as_str())
            .unwrap_or("");

        let matrix_id = matrix_id_from_href(href);
        let pill_type = pill_class_for_id(&matrix_id);

        if pill_type.is_empty() {
            // Not a pill-eligible link — emit as-is.
            out.push_str(&html[tag.start..tag.end]);
            pos = tag.end;
            continue;
        }

        // Rebuild the <a> tag with pill class.
        out.push_str("<a href=\"");
        out.push_str(&html_escape(href));
        out.push_str("\" class=\"pill pill-");
        out.push_str(pill_type);
        out.push_str("\">");

        // Inject an avatar image — real mxc when available, otherwise the
        // default-avatar fallback URL prepared on the C++ side. We avoid
        // emitting a bare pill (text only, no `<img>`) here because the
        // pill-avatar CSS reserves a square slot, and the user expects
        // parity with the timeline avatar where the fallback always renders.
        if let Some(entry) = avatar_map.get(matrix_id.as_str()) {
            if let Some(avatar_src) = pill_avatar_src(entry, avatar_size) {
                out.push_str("<img class=\"pill-avatar\" src=\"");
                out.push_str(&html_escape(&avatar_src));
                out.push_str("\"/>");
            }
        }

        pos = tag.end;
    }

    out
}

// ---------------------------------------------------------------------------
// plain_text_to_html
// ---------------------------------------------------------------------------

fn plain_text_to_html(body: &str) -> String {
    if body.is_empty() {
        return String::new();
    }

    let escaped = html_escape(body);
    let mut html = String::with_capacity(escaped.len() + 7);
    html.push_str("<p>");
    html.push_str(
        &escaped
            .replace("\n\n", "</p><p>")
            .replace('\n', "<br>"),
    );
    html.push_str("</p>");
    html
}

// ---------------------------------------------------------------------------
// Search-match marking
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

/// Process a timeline message's HTML through the full pipeline.
///
/// 1. If `formatted_body` is present: sanitize → syntax-highlight → linkify → decorate pills
/// 2. If only `body`: convert plain text to HTML → linkify
///
/// The caller is responsible for emoji replacement (stays in C++).
pub(crate) fn format_body_html(
    body: &str,
    formatted_body: &str,
    pill_avatars: &[HtmlPillAvatar],
    pill_avatar_size: u32,
    is_dark_theme: bool,
    syntax_highlight: bool,
) -> String {
    if body.is_empty() && formatted_body.is_empty() {
        return String::new();
    }

    if !formatted_body.is_empty() {
        let html = sanitize_html(formatted_body);

        let html = if syntax_highlight {
            crate::syntax_highlight::highlight_formatted_code_blocks(&html, is_dark_theme)
        } else {
            html
        };

        let html = linkify_html(&html);

        // Build avatar map lazily — only when the HTML actually contains matrix.to links.
        if html.contains(MATRIX_TO_PREFIX) {
            let avatar_map = build_avatar_map(pill_avatars);
            decorate_matrix_pills(&html, &avatar_map, pill_avatar_size)
        } else {
            html
        }
    } else {
        let html = plain_text_to_html(body);
        linkify_html(&html)
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    fn count_occurrences(text: &str, needle: &str) -> usize {
        text.match_indices(needle).count()
    }

    // -- sanitize_html --

    #[test]
    fn escapes_disallowed_tags() {
        let out = sanitize_html("<script>alert(1)</script><b>ok</b>");
        assert!(!out.contains("<script"), "script tag is not kept");
        assert!(out.contains("&lt;script"), "script opening is escaped");
        assert!(out.contains("<b>ok</b>"), "allowed tag remains");
    }

    #[test]
    fn strips_mx_reply_fallback() {
        let out = sanitize_html("<mx-reply><blockquote>old</blockquote></mx-reply><p>new</p>");
        assert!(!out.contains("mx-reply"), "mx-reply tags are stripped");
        assert!(!out.contains("old"), "mx-reply content is stripped");
        assert!(out.contains("<p>new</p>"), "non-reply content remains");
    }

    #[test]
    fn anchor_attribute_filtering() {
        let out = sanitize_html(
            r#"<a href="https://example.org" onclick="evil()" target="_blank">ok</a>"#,
        );
        assert!(
            out.contains(r#"href="https://example.org""#),
            "safe href is preserved"
        );
        assert!(
            out.contains(r#"target="_blank""#),
            "target is preserved"
        );
        assert!(!out.contains("onclick"), "unsafe onclick attribute is removed");
    }

    #[test]
    fn href_scheme_and_relative_filtering() {
        let js_out = sanitize_html(r#"<a href="javascript:alert(1)">x</a>"#);
        let rel_out = sanitize_html(r#"<a href="/relative">x</a>"#);
        assert!(!js_out.contains("href="), "javascript href is removed");
        assert!(!rel_out.contains("href="), "relative href is removed");
    }

    #[test]
    fn image_src_filtering() {
        let safe_out =
            sanitize_html(r#"<img src="mxc://example.org/id" height="24" width="32" alt="a">"#);
        let unsafe_out =
            sanitize_html(r#"<img src="https://example.org/x.png" onerror="e()">"#);
        assert!(
            safe_out.contains(r#"src="image://mxcImage/example.org/id""#),
            "mxc image source is rewritten to image://mxcImage/"
        );
        assert!(
            !unsafe_out.contains(r#"src="https://"#),
            "non-mxc image source is removed"
        );
        assert!(
            !unsafe_out.contains("onerror"),
            "unsafe image attribute is removed"
        );

        // Verify that a pre-crafted image:// URL cannot bypass the mxc:// gate.
        let injected =
            sanitize_html(r#"<img src="image://mxcImage/evil.org/payload">"#);
        assert!(
            !injected.contains("image://"),
            "image:// src must be rejected (only mxc:// is accepted)"
        );
    }

    #[test]
    fn code_class_filtering() {
        let out = sanitize_html(r#"<code class="foo language-cpp language-python bad">x</code>"#);
        assert!(
            out.contains(r#"class="language-cpp language-python""#),
            "only language-* code classes are preserved"
        );
        assert!(!out.contains("foo"), "non-language classes are removed");
    }

    #[test]
    fn span_color_validation() {
        let out = sanitize_html(
            r##"<span data-mx-color="#112233" data-mx-bg-color="#GGGGGG" data-mx-spoiler="spoiler" style="x">x</span>"##,
        );
        assert!(
            out.contains(r##"data-mx-color="#112233""##),
            "valid data-mx-color is preserved"
        );
        assert!(
            !out.contains(r##"data-mx-bg-color="#GGGGGG""##),
            "invalid color value is removed"
        );
        assert!(
            out.contains(r#"data-mx-spoiler="spoiler""#),
            "spoiler attribute is preserved"
        );
        assert!(!out.contains("style="), "unsupported style attribute is removed");
    }

    #[test]
    fn font_color_validation() {
        let out = sanitize_html(
            r##"<font color="#112233">x</font><font color="orange">x</font><font color="not-a-color">x</font>"##,
        );
        assert!(
            out.contains(r##"color="#112233""##),
            "valid hex color is preserved"
        );
        assert!(
            out.contains(r#"color="orange""#),
            "valid named color is preserved"
        );
        assert!(!out.contains("not-a-color"), "invalid color is removed");
    }

    #[test]
    fn depth_limit() {
        let mut input = String::new();
        for _ in 0..101 {
            input.push_str("<div>");
        }
        input.push_str("payload");
        for _ in 0..101 {
            input.push_str("</div>");
        }
        let out = sanitize_html(&input);
        assert!(
            count_occurrences(&out, "<div>") <= 100,
            "opening tag depth is capped at 100"
        );
        assert!(out.contains("payload"), "inner payload remains readable");
    }

    // -- linkify_html --

    #[test]
    fn linkify_plain_text_and_matrix_uri() {
        let out = linkify_html("See https://example.org and matrix:r/example.org/abc");
        assert!(
            out.contains(r#"<a href="https://example.org">https://example.org</a>"#),
            "plain https URL is linkified"
        );
        assert!(
            out.contains(r#"<a href="matrix:r/example.org/abc">matrix:r/example.org/abc</a>"#),
            "matrix URI is linkified"
        );
    }

    #[test]
    fn linkify_skips_anchors_and_code_blocks() {
        let input = concat!(
            r#"<a href="https://already.example">https://text.example</a> "#,
            "<code>https://code.example</code> ",
            "<pre>https://pre.example</pre> ",
            "https://tail.example"
        );
        let out = linkify_html(input);
        assert_eq!(
            count_occurrences(&out, r#"<a href="https://already.example">"#),
            1,
            "existing anchor tag remains unchanged"
        );
        assert!(
            out.contains("<code>https://code.example</code>"),
            "code block text is not linkified"
        );
        assert!(
            out.contains("<pre>https://pre.example</pre>"),
            "pre block text is not linkified"
        );
        assert!(
            out.contains(r#"<a href="https://tail.example">https://tail.example</a>"#),
            "trailing plain URL is linkified"
        );
    }

    // -- decorate_matrix_pills --

    fn make_pill_avatar(user_id: &str, mxc_url: &str) -> HtmlPillAvatar {
        HtmlPillAvatar {
            user_id: user_id.to_string(),
            mxc_url: mxc_url.to_string(),
            fallback_url: String::new(),
        }
    }

    fn make_pill_avatar_with_fallback(
        user_id: &str,
        mxc_url: &str,
        fallback_url: &str,
    ) -> HtmlPillAvatar {
        HtmlPillAvatar {
            user_id: user_id.to_string(),
            mxc_url: mxc_url.to_string(),
            fallback_url: fallback_url.to_string(),
        }
    }

    fn avatar_map_from<'a>(entries: &'a [HtmlPillAvatar]) -> HashMap<&'a str, &'a HtmlPillAvatar> {
        build_avatar_map(entries)
    }

    #[test]
    fn pill_decorates_user_mention() {
        let html = r#"hello <a href="https://matrix.to/#/%40slavi%3Adevture.com">Slavi</a> world"#;
        let avatars = vec![make_pill_avatar(
            "@slavi:devture.com",
            "mxc://devture.com/abc123",
        )];
        let map = avatar_map_from(&avatars);
        let out = decorate_matrix_pills(html, &map, 32);
        assert!(out.contains(r#"class="pill pill-user""#), "user pill class");
        assert!(
            out.contains(r#"<img class="pill-avatar""#),
            "user pill has avatar img"
        );
        assert!(
            out.contains("image://mxcImage/devture.com/abc123?avatarSize=32&amp;radius=25"),
            "avatar img has correct src (& escaped as &amp; in HTML attribute)"
        );
        assert!(out.contains("Slavi"), "display name text is preserved");
        assert!(out.contains("hello "), "text before pill is preserved");
        assert!(out.contains(" world"), "text after pill is preserved");
    }

    #[test]
    fn pill_decorates_room_mention() {
        let html =
            r#"<a href="https://matrix.to/#/%23room%3Aexample.org">#room:example.org</a>"#;
        let avatars = vec![make_pill_avatar(
            "#room:example.org",
            "mxc://example.org/roomavatar",
        )];
        let map = avatar_map_from(&avatars);
        let out = decorate_matrix_pills(html, &map, 32);
        assert!(
            out.contains(r#"class="pill pill-room""#),
            "room pill has pill-room class"
        );
        assert!(
            out.contains(r#"<img class="pill-avatar""#),
            "room pill has avatar img"
        );
        assert!(
            out.contains("#room:example.org"),
            "room name text is preserved"
        );
    }

    #[test]
    fn pill_decorates_room_id_mention() {
        let html =
            r#"<a href="https://matrix.to/#/!abc123%3Aexample.org">My Room</a>"#;
        let avatars: Vec<HtmlPillAvatar> = Vec::new();
        let map = avatar_map_from(&avatars);
        let out = decorate_matrix_pills(html, &map, 32);
        assert!(
            out.contains(r#"class="pill pill-room""#),
            "room ID pill has pill-room class"
        );
        assert!(
            !out.contains(r#"<img class="pill-avatar""#),
            "no avatar img when not in map"
        );
        assert!(out.contains("My Room"), "display text is preserved");
    }

    #[test]
    fn pill_skips_non_matrix_to_links() {
        let html = r#"<a href="https://example.org">Example</a>"#;
        let avatars = vec![make_pill_avatar("@any:server", "mxc://server/img")];
        let map = avatar_map_from(&avatars);
        let out = decorate_matrix_pills(html, &map, 32);
        assert!(!out.contains("pill"), "non-matrix.to link is not decorated");
        assert_eq!(out, html, "non-matrix.to link is unchanged");
    }

    #[test]
    fn pill_preserves_multiple_links() {
        let html = concat!(
            r#"<a href="https://matrix.to/#/%40alice%3Aexample.org">Alice</a> and "#,
            r#"<a href="https://matrix.to/#/%40bob%3Aexample.org">Bob</a>"#
        );
        let avatars = vec![
            make_pill_avatar("@alice:example.org", "mxc://example.org/alice"),
            make_pill_avatar("@bob:example.org", "mxc://example.org/bob"),
        ];
        let map = avatar_map_from(&avatars);
        let out = decorate_matrix_pills(html, &map, 32);
        assert_eq!(
            count_occurrences(&out, r#"class="pill pill-user""#),
            2,
            "both user links are decorated"
        );
        assert!(out.contains("example.org/alice"), "first avatar is present");
        assert!(out.contains("example.org/bob"), "second avatar is present");
        assert!(out.contains("Alice"), "first display name");
        assert!(out.contains(" and "), "text between pills");
        assert!(out.contains("Bob"), "second display name");
    }

    #[test]
    fn pill_with_empty_avatar_map() {
        let html =
            r#"<a href="https://matrix.to/#/%40user%3Aexample.org">User</a>"#;
        let avatars: Vec<HtmlPillAvatar> = Vec::new();
        let map = avatar_map_from(&avatars);
        let out = decorate_matrix_pills(html, &map, 32);
        assert!(
            out.contains(r#"class="pill pill-user""#),
            "pill class is added even without avatars"
        );
        assert!(!out.contains("<img"), "no img tag without avatars");
    }

    #[test]
    fn pill_with_event_link() {
        let html =
            r#"<a href="https://matrix.to/#/!room%3Aserver/%24event%3Aserver">link</a>"#;
        let avatars = vec![make_pill_avatar("!room:server", "mxc://server/roomavatar")];
        let map = avatar_map_from(&avatars);
        let out = decorate_matrix_pills(html, &map, 32);
        assert!(
            out.contains(r#"class="pill pill-room""#),
            "event link is decorated as room pill"
        );
        assert!(
            out.contains("image://mxcImage/server/roomavatar"),
            "room avatar is resolved from room ID portion"
        );
    }

    #[test]
    fn pill_uses_fallback_when_user_has_no_mxc_avatar() {
        let html =
            r#"<a href="https://matrix.to/#/%40user%3Aexample.org">User</a>"#;
        let avatars = vec![make_pill_avatar_with_fallback(
            "@user:example.org",
            "",
            "image://default-avatar/@user:example.org?radius=25&displayName=User&color=ab12cd&style=4&_v=4",
        )];
        let map = avatar_map_from(&avatars);
        let out = decorate_matrix_pills(html, &map, 32);
        assert!(
            out.contains(r#"<img class="pill-avatar""#),
            "fallback img is injected when no mxc URL is available"
        );
        assert!(
            out.contains("image://default-avatar/@user:example.org"),
            "default-avatar URL is emitted as the pill avatar source"
        );
        assert!(
            out.contains("avatarSize=32"),
            "avatarSize is appended for the default-avatar provider"
        );
    }

    #[test]
    fn pill_prefers_mxc_over_fallback_when_both_present() {
        let html =
            r#"<a href="https://matrix.to/#/%40user%3Aexample.org">User</a>"#;
        let avatars = vec![make_pill_avatar_with_fallback(
            "@user:example.org",
            "mxc://example.org/abc",
            "image://default-avatar/@user:example.org?radius=25",
        )];
        let map = avatar_map_from(&avatars);
        let out = decorate_matrix_pills(html, &map, 32);
        assert!(
            out.contains("image://mxcImage/example.org/abc"),
            "real mxc avatar is the primary src"
        );
    }

    #[test]
    fn pill_with_mxc_carries_percent_encoded_fallback_for_litehtml() {
        let html =
            r#"<a href="https://matrix.to/#/%40user%3Aexample.org">User</a>"#;
        let avatars = vec![make_pill_avatar_with_fallback(
            "@user:example.org",
            "mxc://example.org/abc",
            "image://default-avatar/@user:example.org?radius=25&color=ab12cd",
        )];
        let map = avatar_map_from(&avatars);
        let out = decorate_matrix_pills(html, &map, 32);
        // The outer mxc URL is the primary src. The fallback is tucked into
        // its query so LitehtmlContainer can pre-cache the default avatar
        // under the mxc URL key — `&` (HTML-escaped to `&amp;`), `=`, `%`,
        // `:` and `/` inside the inner URL all need percent-encoding so they
        // don't break out of the `fallback=` query value.
        assert!(
            out.contains("image://mxcImage/example.org/abc"),
            "mxc URL is the primary src"
        );
        assert!(
            out.contains("fallback=image%3A%2F%2Fdefault-avatar%2F"),
            "fallback URL is percent-encoded inside the mxc query"
        );
        assert!(
            out.contains("%26color%3Dab12cd"),
            "fallback URL's own `&color=...` is encoded so it doesn't bleed into the outer query"
        );
    }

    // -- plain_text_to_html --

    #[test]
    fn plain_text_preserves_double_newlines() {
        let out = plain_text_to_html("Hello!\n\nWorld!\n\nAnother");
        assert_eq!(out, "<p>Hello!</p><p>World!</p><p>Another</p>");
    }

    #[test]
    fn plain_text_preserves_single_newlines() {
        let out = plain_text_to_html("Line1\nLine2\nLine3");
        assert_eq!(out, "<p>Line1<br>Line2<br>Line3</p>");
    }

    #[test]
    fn plain_text_mixed_newlines() {
        let out = plain_text_to_html("A\nB\n\nC\nD");
        assert_eq!(out, "<p>A<br>B</p><p>C<br>D</p>");
    }

    #[test]
    fn plain_text_escapes_html() {
        let out = plain_text_to_html("<b>bold</b> & \"quoted\"");
        assert!(!out.contains("<b>"), "HTML tags are escaped");
        assert!(out.contains("&amp;"), "ampersand is escaped");
        assert!(out.contains("&lt;b&gt;"), "angle brackets are escaped");
    }

    #[test]
    fn plain_text_empty() {
        assert!(plain_text_to_html("").is_empty(), "empty input produces empty output");
    }

    // -- mark_search_matches --

    #[test]
    fn mark_passes_through_when_query_or_html_is_empty() {
        assert_eq!(mark_search_matches("", "foo"), "");
        assert_eq!(mark_search_matches("<p>hello</p>", ""), "<p>hello</p>");
    }

    #[test]
    fn mark_wraps_plain_text_match() {
        let out = mark_search_matches("<p>hello world</p>", "world");
        assert_eq!(out, r##"<p>hello <span class="komai-search-match">world</span></p>"##);
    }

    #[test]
    fn mark_is_case_insensitive() {
        let out = mark_search_matches("<p>Hello WORLD</p>", "world");
        assert_eq!(out, r##"<p>Hello <span class="komai-search-match">WORLD</span></p>"##);
    }

    #[test]
    fn mark_preserves_original_casing_of_match() {
        let out = mark_search_matches("<p>aBc xYz aBc</p>", "abc");
        assert_eq!(
            count_occurrences(&out, r##"<span class="komai-search-match">aBc</span>"##),
            2,
            "both occurrences keep their original casing inside the mark wrapper",
        );
    }

    #[test]
    fn mark_skips_tag_names_and_attributes() {
        // Searching "a" should not wrap the <a> tag name or its href attribute name.
        let html = r#"<a href="https://example.com">click</a>"#;
        let out = mark_search_matches(html, "a");
        assert!(out.starts_with("<a href=\"https://example.com\">"),
                "the literal anchor tag is preserved verbatim, got: {out}");
        assert!(!out.contains(r##"<<span class="komai-search-match">"##),
                "nothing inside the tag bytes is marked");
    }

    #[test]
    fn mark_wraps_inside_code_and_pre() {
        let html = "<pre><code>foo bar foo</code></pre>";
        let out = mark_search_matches(html, "foo");
        assert_eq!(
            count_occurrences(&out, r##"<span class="komai-search-match">foo</span>"##),
            2,
            "matches inside code/pre are marked",
        );
    }

    #[test]
    fn mark_anchor_href_match_wraps_link_text_when_visible_text_has_no_match() {
        let html = r#"<a href="https://deadbeef.com/">Grey's Anatomy</a>"#;
        let out = mark_search_matches(html, "deadbeef");
        assert_eq!(
            out,
            r#"<a href="https://deadbeef.com/"><span class="komai-search-match">Grey's Anatomy</span></a>"#,
            "href-only match wraps the entire link text",
        );
    }

    #[test]
    fn mark_anchor_href_match_with_visible_match_uses_substring_marks_only() {
        // When the visible text also contains the query, only the substring is
        // wrapped — no outer wrap-the-whole-anchor, which would be redundant.
        let html = r#"<a href="https://deadbeef.com/">deadbeef site</a>"#;
        let out = mark_search_matches(html, "deadbeef");
        assert_eq!(
            out,
            r#"<a href="https://deadbeef.com/"><span class="komai-search-match">deadbeef</span> site</a>"#,
        );
    }

    #[test]
    fn mark_anchor_without_href_match_does_not_wrap_link_text() {
        let html = r#"<a href="https://example.com/">click here</a>"#;
        let out = mark_search_matches(html, "deadbeef");
        assert_eq!(out, html, "no match anywhere, html is unchanged");
    }

    #[test]
    fn mark_multiple_matches_in_one_text_segment() {
        let html = "<p>abc abc abc</p>";
        let out = mark_search_matches(html, "abc");
        assert_eq!(
            out,
            r##"<p><span class="komai-search-match">abc</span> <span class="komai-search-match">abc</span> <span class="komai-search-match">abc</span></p>"##,
        );
    }

    #[test]
    fn mark_does_not_match_across_tag_boundary() {
        // The 'fo' span straddles a <span>; we should match only "foo" inside one text node.
        let html = "<p>foo<span>bar</span>foo</p>";
        let out = mark_search_matches(html, "foo");
        assert_eq!(
            out,
            r##"<p><span class="komai-search-match">foo</span><span>bar</span><span class="komai-search-match">foo</span></p>"##,
        );
    }

    #[test]
    fn mark_preserves_multibyte_text() {
        let html = "<p>café CAFÉ</p>";
        let out = mark_search_matches(html, "café");
        // Only the lowercase form matches byte-for-byte (ASCII-CI folds the
        // 'C' but the accented 'É' is a different byte sequence from 'é').
        assert!(out.contains(r##"<span class="komai-search-match">café</span>"##),
                "lowercase form is marked: {out}");
        assert!(!out.contains(r##"<span class="komai-search-match">CAFÉ</span>"##),
                "uppercase é is NOT marked under ASCII-CI: {out}");
    }

    #[test]
    fn mark_query_longer_than_text_is_noop() {
        let html = "<p>hi</p>";
        let out = mark_search_matches(html, "looking for a longer query");
        assert_eq!(out, html);
    }

    #[test]
    fn mark_anchor_href_match_inside_nested_anchors() {
        // Sanity: only the innermost anchor's href flag drives wrapping.
        let html = r#"<a href="https://outer.com/"><a href="https://inner.com/">text</a></a>"#;
        let out = mark_search_matches(html, "inner");
        assert!(
            out.contains(r##"<span class="komai-search-match">text</span>"##),
            "inner anchor href match wraps its text, got: {out}",
        );
    }

    #[test]
    fn mark_anchor_close_pops_correctly() {
        // After </a>, subsequent text outside the matching anchor should
        // not be wrapped (no href context to inherit).
        let html = r#"<a href="https://deadbeef.com/">link</a> tail"#;
        let out = mark_search_matches(html, "deadbeef");
        assert_eq!(out, r#"<a href="https://deadbeef.com/"><span class="komai-search-match">link</span></a> tail"#);
    }

    #[test]
    fn mark_query_equal_to_tag_name_is_safe() {
        // Searching "code" against text inside a <code> element must not
        // break HTML — the <code> bytes are skipped, only inner text is marked.
        let html = "<code>code is code</code>";
        let out = mark_search_matches(html, "code");
        assert_eq!(
            out,
            r##"<code><span class="komai-search-match">code</span> is <span class="komai-search-match">code</span></code>"##,
        );
    }
}
