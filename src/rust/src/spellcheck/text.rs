// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Pure text-processing helpers: deciding whether a word is checkable,
//! computing skip ranges (URLs, mentions, code fences, etc.), splitting
//! a block into word spans, and UTF-16 length math for QML.

use super::*;

pub(super) fn looks_checkable(word: &str) -> bool {
    let mut chars = word.chars();
    let Some(first) = chars.next() else {
        return false;
    };
    if word.chars().count() < 2 || word.chars().count() > MAX_WORD_CHARS {
        return false;
    }
    if !first.is_alphabetic() && first != '\'' && first != '\u{2019}' {
        return false;
    }
    let mut has_lower = first.is_lowercase();
    let mut has_inner_upper = false;
    for (i, c) in word.char_indices() {
        // Halfwidth/Fullwidth Forms — Japanese IMEs emit fullwidth Latin (Ａ-Ｚ,
        // ａ-ｚ) in "wide Latin / Zenkaku" mode. Unicode tags those characters
        // Script=Latin, so without this guard they'd be run against the en_US
        // dictionary and flagged as misspellings with no useful suggestions.
        if matches!(c, '\u{FF00}'..='\u{FFEF}') {
            return false;
        }
        if c.is_numeric() {
            return false; // tokens with digits aren't words
        }
        if !(c.is_alphabetic() || c == '\'' || c == '\u{2019}' || c == '-') {
            return false; // some odd symbol slipped in
        }
        if c.is_lowercase() {
            has_lower = true;
        }
        if i > 0 && c.is_uppercase() {
            has_inner_upper = true;
        }
    }
    if has_inner_upper {
        return false; // camelCase / McName / iPhone — likely an identifier
    }
    if !has_lower {
        return false; // ALL CAPS — treat as an acronym, don't flag
    }
    true
}

/// Look the word up across the enabled, script-compatible dictionaries. Also
/// consults the personal and session-ignore lists. Returns true only if it is

pub(super) fn skip_ranges(text: &str) -> Vec<(usize, usize)> {
    let mut ranges: Vec<(usize, usize)> = Vec::new();

    // URLs + emails.
    let mut finder = LinkFinder::new();
    finder.kinds(&[LinkKind::Url, LinkKind::Email]);
    for link in finder.links(text) {
        ranges.push((link.start(), link.end()));
    }

    // Inline code spans: text between a `` ` `` and the next `` ` ``. Cheap and
    // good enough for a composer — we don't try to honour multi-backtick fences
    // here.
    {
        let bytes = text.as_bytes();
        let mut i = 0usize;
        while i < bytes.len() {
            if bytes[i] == b'`' {
                if let Some(rel) = text[i + 1..].find('`') {
                    let end = i + 1 + rel + 1; // include the closing backtick
                    ranges.push((i, end));
                    i = end;
                    continue;
                }
            }
            i += 1;
        }
    }

    // @mention / #tag / :emoji: — a run starting at one of those sigils up to
    // the next whitespace.
    {
        let bytes = text.as_bytes();
        let mut i = 0usize;
        while i < bytes.len() {
            let c = bytes[i];
            let at_word_start = i == 0 || bytes[i - 1].is_ascii_whitespace();
            if (c == b'@' || c == b'#' || c == b':') && at_word_start {
                let mut j = i + 1;
                while j < bytes.len() && !bytes[j].is_ascii_whitespace() {
                    j += 1;
                }
                if j > i + 1 {
                    ranges.push((i, j));
                }
                i = j;
                continue;
            }
            i += 1;
        }
    }

    ranges.sort_unstable();
    ranges
}

pub(super) fn overlaps_any(start: usize, end: usize, ranges: &[(usize, usize)]) -> bool {
    ranges.iter().any(|&(s, e)| start < e && s < end)
}

/// Iterator-ish helper: returns byte ranges of "word-like" runs in `text` —
/// maximal runs of alphabetic characters plus intra-word apostrophes and

pub(super) fn word_spans(text: &str) -> Vec<(usize, usize)> {
    let mut out = Vec::new();
    let mut start: Option<usize> = None;
    let is_word_char = |c: char| c.is_alphabetic() || c == '\'' || c == '\u{2019}' || c == '-';
    for (i, c) in text.char_indices() {
        if is_word_char(c) {
            if start.is_none() {
                start = Some(i);
            }
        } else if let Some(s) = start.take() {
            push_trimmed_span(text, s, i, &mut out);
        }
    }
    if let Some(s) = start.take() {
        push_trimmed_span(text, s, text.len(), &mut out);
    }
    out
}

pub(super) fn push_trimmed_span(text: &str, mut s: usize, mut e: usize, out: &mut Vec<(usize, usize)>) {
    let trim = |c: char| c == '\'' || c == '\u{2019}' || c == '-';
    while s < e {
        let c = text[s..].chars().next().unwrap();
        if trim(c) {
            s += c.len_utf8();
        } else {
            break;
        }
    }
    while e > s {
        let c = text[..e].chars().next_back().unwrap();
        if trim(c) {
            e -= c.len_utf8();
        } else {
            break;
        }
    }
    if e > s {
        out.push((s, e));
    }
}

pub(super) fn utf16_len(s: &str) -> u32 {
    s.encode_utf16().count() as u32
}
