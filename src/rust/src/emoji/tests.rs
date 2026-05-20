// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

#[test]
fn empty_string() {
    assert_eq!(emoji_only_visual_count(""), 0);
}

#[test]
fn plain_text() {
    assert_eq!(emoji_only_visual_count("hello"), 0);
}

#[test]
fn mixed_text_and_emoji() {
    assert_eq!(emoji_only_visual_count("hi 😀"), 0);
}

#[test]
fn single_simple_emoji() {
    assert_eq!(emoji_only_visual_count("😀"), 1);
}

#[test]
fn multiple_simple_emojis() {
    assert_eq!(emoji_only_visual_count("😀😂🎉"), 3);
}

#[test]
fn zwj_sequence_shrug_male() {
    // 🤷‍♂️ = U+1F937 U+200D U+2642 U+FE0F
    assert_eq!(emoji_only_visual_count("🤷\u{200D}\u{2642}\u{FE0F}"), 1);
}

#[test]
fn zwj_sequence_shrug_male_no_vs16() {
    // 🤷‍♂ = U+1F937 U+200D U+2642 (without trailing VS16)
    assert_eq!(emoji_only_visual_count("🤷\u{200D}\u{2642}"), 1);
}

#[test]
fn family_emoji_zwj_sequence() {
    // 👨‍👩‍👧‍👦 = U+1F468 U+200D U+1F469 U+200D U+1F467 U+200D U+1F466
    assert_eq!(emoji_only_visual_count("👨\u{200D}👩\u{200D}👧\u{200D}👦"), 1);
}

#[test]
fn emoji_with_skin_tone() {
    // 👋🏽 = U+1F44B U+1F3FD
    assert_eq!(emoji_only_visual_count("👋\u{1F3FD}"), 1);
}

#[test]
fn two_emojis_with_skin_tones() {
    // 👋🏽👋🏿
    assert_eq!(emoji_only_visual_count("👋\u{1F3FD}👋\u{1F3FF}"), 2);
}

#[test]
fn emoji_with_vs16() {
    // ♂️ = U+2642 U+FE0F
    assert_eq!(emoji_only_visual_count("\u{2642}\u{FE0F}"), 1);
}

#[test]
fn three_zwj_emojis_enlarged() {
    // Three ZWJ emoji — should still be within the enlarge threshold.
    assert_eq!(
        emoji_only_visual_count(
            "🤷\u{200D}\u{2642}\u{FE0F}🤷\u{200D}\u{2642}\u{FE0F}🤷\u{200D}\u{2642}\u{FE0F}"
        ),
        3
    );
}

#[test]
fn flag_sequence() {
    // 🇺🇸 = U+1F1FA U+1F1F8 (regional indicator symbols)
    // These are in 0x1F000..=0x1FAFF range.
    assert_eq!(emoji_only_visual_count("🇺🇸"), 2);
}
