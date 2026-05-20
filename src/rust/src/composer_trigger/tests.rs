// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

/// Helper: place a trigger char at the end of `prefix` and ask whether
/// the picker should open there.
fn boundary_after(prefix: &str) -> bool {
    trigger_at_word_boundary(&format!("{prefix}:"), prefix.len())
}

#[test]
fn empty_prefix_is_boundary() {
    assert!(trigger_at_word_boundary(":", 0));
    assert!(trigger_at_word_boundary("", 0));
}

#[test]
fn whitespace_is_boundary() {
    assert!(boundary_after(" "));
    assert!(boundary_after("\t"));
    assert!(boundary_after("\n"));
    assert!(boundary_after("\r"));
    // Ideographic space (CJK full-width)
    assert!(boundary_after("\u{3000}"));
    assert!(boundary_after("hello "));
}

#[test]
fn letters_and_digits_block_open() {
    // ASCII letter / digit / underscore — mid-word, do not open.
    assert!(!boundary_after("hello"));
    assert!(!boundary_after("user"));
    assert!(!boundary_after("h"));
    assert!(!boundary_after("a1"));
    assert!(!boundary_after("5"));
    assert!(!boundary_after("_"));
    assert!(!boundary_after("name_"));
}

#[test]
fn non_ascii_letters_block_open() {
    // CJK ideograph
    assert!(!boundary_after("你好"));
    // Hiragana
    assert!(!boundary_after("こんにちは"));
    // Cyrillic
    assert!(!boundary_after("Привет"));
    // Arabic (RTL)
    assert!(!boundary_after("مرحبا"));
    // Greek
    assert!(!boundary_after("Γειά"));
}

#[test]
fn emoji_is_boundary() {
    // Single-codepoint BMP emoji
    assert!(boundary_after("\u{263A}")); // ☺
    // Surrogate-pair / supplementary-plane emoji
    assert!(boundary_after("\u{1F69C}")); // 🚜 (tractor)
    // Emoji with VS16
    assert!(boundary_after("\u{2764}\u{FE0F}")); // ❤️
    // ZWJ sequence: 🤷‍♂️
    assert!(boundary_after("\u{1F937}\u{200D}\u{2642}\u{FE0F}"));
    // Skin-tone modifier on a hand emoji
    assert!(boundary_after("\u{1F44B}\u{1F3FD}"));
    // Flag (regional indicator pair)
    assert!(boundary_after("\u{1F1FA}\u{1F1F8}")); // 🇺🇸
}

#[test]
fn punctuation_is_boundary() {
    assert!(boundary_after("."));
    assert!(boundary_after(","));
    assert!(boundary_after(";"));
    assert!(boundary_after("!"));
    assert!(boundary_after("?"));
    assert!(boundary_after("("));
    assert!(boundary_after(")"));
    assert!(boundary_after("["));
    assert!(boundary_after("\""));
}

#[test]
fn email_pattern_stays_blocked() {
    // The regression guard for the original intent of the boundary
    // check (commit 9e2b7c4cc): `user@example.com` must NOT pop the
    // user picker when `@` lands right after a letter.
    assert!(!boundary_after("user"));
}

#[test]
fn offset_past_end_is_clamped() {
    // Defensive: an out-of-range offset is clamped to text.len(), so the
    // last char is consulted. After "hi" the last char is `i` → letter
    // → mid-word. After "hi " (trailing space) it's a boundary.
    assert!(!trigger_at_word_boundary("hi", 999));
    assert!(trigger_at_word_boundary("hi ", 999));
    // Empty string with any offset is always a boundary.
    assert!(trigger_at_word_boundary("", 999));
}

#[test]
fn offset_off_char_boundary_is_boundary() {
    // 🚜 is 4 UTF-8 bytes. Offset 1 sits in the middle of it.
    // We fall back to "boundary" rather than panic.
    assert!(trigger_at_word_boundary("\u{1F69C}", 1));
}

#[test]
fn offset_inside_string_uses_preceding_char_only() {
    // text = "abc: xyz", trigger at byte 3 (the `:`). The char before
    // is `c` → letter → not a boundary. Chars *after* the trigger are
    // irrelevant.
    assert!(!trigger_at_word_boundary("abc: xyz", 3));
    // Same text, trigger at byte 4 (the space). Char before is `:` →
    // punctuation → boundary.
    assert!(trigger_at_word_boundary("abc: xyz", 4));
}
