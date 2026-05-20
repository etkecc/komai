// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! HTML allow-list sanitizer.

use super::parser::{
    MAX_ATTRIBUTE_VALUE_CHARS, MAX_CLASS_CHARS, MAX_MATH_CHARS, MAX_SPOILER_CHARS, MAX_TAG_DEPTH,
    MAX_TARGET_CHARS, MAX_TEXT_ATTRIBUTE_CHARS, MAX_URL_CHARS, ParsedAttribute, ParsedTag,
    is_void_tag, parse_attributes, parse_tag, tag_name_lower,
};
use super::util::{
    html_escape, sanitize_font_color, sanitize_hex_color, sanitize_integer_string,
};

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

#[cfg(test)]
mod tests;
