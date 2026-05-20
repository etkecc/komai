// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Lexer for HTML tag and attribute spans.

pub(super) const MAX_TAG_DEPTH: usize = 100;
pub(super) const MAX_ATTRIBUTE_COUNT: usize = 64;
pub(super) const MAX_ATTRIBUTE_VALUE_CHARS: usize = 4096;
pub(super) const MAX_SPOILER_CHARS: usize = 512;
pub(super) const MAX_MATH_CHARS: usize = 4096;
pub(super) const MAX_TEXT_ATTRIBUTE_CHARS: usize = 1024;
pub(super) const MAX_TARGET_CHARS: usize = 64;
pub(super) const MAX_CLASS_CHARS: usize = 512;
pub(super) const MAX_URL_CHARS: usize = 2048;

#[derive(Default, Clone, Copy)]
pub(crate) struct ParsedTag {
    pub(crate) valid: bool,
    pub(crate) special: bool,
    pub(crate) is_end: bool,
    pub(crate) self_closing: bool,
    pub(crate) start: usize,
    pub(crate) end: usize,
    pub(crate) name_start: usize,
    pub(crate) name_len: usize,
    pub(crate) attrs_start: usize,
    pub(crate) attrs_end: usize,
}

pub(crate) fn is_tag_name_char(b: u8) -> bool {
    b.is_ascii_alphanumeric() || b == b'-' || b == b'_' || b == b':'
}

pub(crate) fn parse_tag(html: &str, tag_start: usize) -> ParsedTag {
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

pub(super) fn tag_name_lower(html: &str, tag: &ParsedTag) -> String {
    if !tag.valid || tag.name_len == 0 {
        return String::new();
    }
    html[tag.name_start..tag.name_start + tag.name_len].to_ascii_lowercase()
}

pub(super) fn is_void_tag(name: &str) -> bool {
    matches!(name, "br" | "hr" | "img")
}

pub(super) struct ParsedAttribute {
    pub(super) name: String,
    pub(super) value: String,
    pub(super) has_value: bool,
}

pub(super) fn parse_attributes(html: &str, tag: &ParsedTag) -> Vec<ParsedAttribute> {
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
