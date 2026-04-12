// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Emoji detection for "emoji-only" message enlargement.

/// Returns whether `code` is an emoji-related Unicode code point.
///
/// This is intentionally broad: it includes presentation selectors, ZWJ,
/// skin-tone modifiers, and other combining/joining characters that participate
/// in emoji sequences.
fn is_emoji_codepoint(code: u32) -> bool {
    matches!(code,
        0x200D          // ZWJ (Zero Width Joiner)
        | 0xFE0F        // VS16 (emoji presentation selector)
        | 0xFE0E        // VS15 (text presentation selector)
        | 0x231A | 0x231B
        | 0x23E9..=0x23FF
        | 0x25AA | 0x25AB | 0x25B6 | 0x25C0
        | 0x25FB..=0x25FE
        | 0x2460..=0x24FF
        | 0x2600..=0x27BF
        | 0x2B00..=0x2BFF
        | 0x1F000..=0x1FAFF
    )
}

/// Count the number of **visual** emoji clusters in `body`.
///
/// Returns 0 if the message contains any non-emoji character (i.e. it is not
/// an "emoji-only" message).
///
/// Unlike a naïve code-point count, this treats ZWJ sequences (e.g.
/// 🤷‍♂️ = U+1F937 U+200D U+2642 U+FE0F), skin-tone modifiers, and
/// variation selectors as part of a single visual emoji rather than counting
/// each code point separately.
pub(crate) fn emoji_only_visual_count(body: &str) -> i32 {
    if body.is_empty() {
        return 0;
    }

    let mut visual_count: i32 = 0;
    let mut after_zwj = false;

    for c in body.chars() {
        let code = c as u32;

        if !is_emoji_codepoint(code) {
            return 0;
        }

        match code {
            // Joiners and modifiers never start a new visual emoji.
            0x200D => {
                after_zwj = true;
                continue;
            }
            0xFE0F | 0xFE0E => continue,
            0x1F3FB..=0x1F3FF => continue, // skin-tone modifiers
            _ => {}
        }

        if after_zwj {
            // This code point is joined to the previous one — same cluster.
            after_zwj = false;
            continue;
        }

        visual_count += 1;
    }

    visual_count
}

#[cfg(test)]
mod tests {
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
}
