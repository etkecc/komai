// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Low-level string utilities shared by sanitize, linkify, pill, and search.

pub(super) fn html_escape(s: &str) -> String {
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

pub(super) fn percent_decode(input: &str) -> String {
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

/// Percent-encode a string for safe inclusion as an opaque value inside a
/// URL query (`?key=...&...`). Only RFC 3986 unreserved characters pass
/// through verbatim — everything else, including `&`, `=`, `?`, `%`, `/`,
/// and `:`, is encoded as `%HH`. We need this for stuffing a full
/// `image://default-avatar/...` URL into the `fallback=` query of an
/// `image://mxcImage/...` URL: without encoding, the inner URL's own
/// `&` and `=` separators would bleed into the outer URL's query.
pub(super) fn percent_encode_query_value(s: &str) -> String {
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

pub(super) fn is_hex_color(s: &str) -> bool {
    s.len() == 7 && s.as_bytes()[0] == b'#' && s[1..].bytes().all(|b| b.is_ascii_hexdigit())
}

pub(super) fn sanitize_hex_color(raw: &str) -> Option<String> {
    let v = raw.trim();
    if is_hex_color(v) {
        Some(v.to_string())
    } else {
        None
    }
}

pub(super) fn sanitize_font_color(raw: &str) -> Option<String> {
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

pub(super) fn sanitize_integer_string(raw: &str) -> Option<String> {
    let v = raw.trim();
    if !v.is_empty() && v.len() <= 5 && v.bytes().all(|b| b.is_ascii_digit()) {
        Some(v.to_string())
    } else {
        None
    }
}
