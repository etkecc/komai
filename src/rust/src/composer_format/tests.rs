// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

/// Apply a `ComposerTransformResult` to `text` and return the new text +
/// observed selection. Lets tests assert against a single "what does the
/// document look like after?" expectation, not three separate ones.
fn apply(text: &str, r: &ComposerTransformResult) -> (String, u32, u32) {
    if !r.applied {
        return (text.to_string(), 0, 0);
    }
    let s_b = utf16_to_byte_offset(text, r.replace_start_utf16);
    let e_b = utf16_to_byte_offset(text, r.replace_end_utf16);
    let mut new_text = String::with_capacity(text.len() - (e_b - s_b) + r.replacement_text.len());
    new_text.push_str(&text[..s_b]);
    new_text.push_str(&r.replacement_text);
    new_text.push_str(&text[e_b..]);
    (new_text, r.new_sel_start_utf16, r.new_sel_end_utf16)
}

// ----------------------------------------------------------------------
// toggle_inline_wrap — bold (**)
// ----------------------------------------------------------------------

mod bold {
    use super::*;

    fn bold(text: &str, s: u32, e: u32) -> ComposerTransformResult {
        toggle_inline_wrap(text, s, e, "**")
    }

    #[test]
    fn empty_text_empty_sel_is_noop() {
        let r = bold("", 0, 0);
        assert!(!r.applied);
    }

    #[test]
    fn collapsed_cursor_is_noop() {
        let r = bold("hello", 2, 2);
        assert!(!r.applied);
    }

    #[test]
    fn wraps_simple_selection() {
        let r = bold("hi foo bar", 3, 6);
        let (new_text, s, e) = apply("hi foo bar", &r);
        assert_eq!(new_text, "hi **foo** bar");
        assert_eq!((s, e), (3, 10));
    }

    #[test]
    fn strips_inside_markers() {
        let r = bold("**foo**", 0, 7);
        let (new_text, s, e) = apply("**foo**", &r);
        assert_eq!(new_text, "foo");
        assert_eq!((s, e), (0, 3));
    }

    #[test]
    fn strips_by_context() {
        let r = bold("**foo**", 2, 5);
        let (new_text, s, e) = apply("**foo**", &r);
        assert_eq!(new_text, "foo");
        assert_eq!((s, e), (0, 3));
    }

    #[test]
    fn strips_by_context_mid_text() {
        let r = bold("x**foo**y", 3, 6);
        let (new_text, s, e) = apply("x**foo**y", &r);
        assert_eq!(new_text, "xfooy");
        assert_eq!((s, e), (1, 4));
    }

    #[test]
    fn strips_outer_of_bold_italic() {
        // ***foo*** = bold+italic. Bold strips outer ** leaving italic.
        let r = bold("***foo***", 0, 9);
        let (new_text, s, e) = apply("***foo***", &r);
        assert_eq!(new_text, "*foo*");
        assert_eq!((s, e), (0, 5));
    }

    #[test]
    fn wraps_inner_of_bold_italic_via_context() {
        // sel = inner "foo" of ***foo***. Chars before/after are `***`,
        // which ENDS with `**` and STARTS with `**`. Bold context-strip
        // matches → strip the OUTER **. Net effect: `***foo***` → `*foo*`.
        let r = bold("***foo***", 3, 6);
        let (new_text, s, e) = apply("***foo***", &r);
        assert_eq!(new_text, "*foo*");
        assert_eq!((s, e), (1, 4));
    }

    #[test]
    fn marker_only_selection_wraps() {
        // Selection is the markers themselves; nl=2,nr=2,total=2 → no
        // strip (nl+nr > total). Falls through to wrap → "******".
        let r = bold("**", 0, 2);
        let (new_text, _s, _e) = apply("**", &r);
        assert_eq!(new_text, "******");
    }

    #[test]
    fn wraps_multiline_selection() {
        let r = bold("a\nb", 0, 3);
        let (new_text, s, e) = apply("a\nb", &r);
        assert_eq!(new_text, "**a\nb**");
        assert_eq!((s, e), (0, 7));
    }

    #[test]
    fn handles_emoji_in_wrap() {
        // "a🚜b" — emoji is one Rust char but 2 UTF-16 code units. Total
        // UTF-16 length = 4.
        let r = bold("a🚜b", 1, 3); // selects just the emoji
        let (new_text, s, e) = apply("a🚜b", &r);
        assert_eq!(new_text, "a**🚜**b");
        assert_eq!((s, e), (1, 7));
    }

    #[test]
    fn strips_emoji_inside_bold() {
        let r = bold("**🚜**", 0, 6);
        let (new_text, s, e) = apply("**🚜**", &r);
        assert_eq!(new_text, "🚜");
        assert_eq!((s, e), (0, 2));
    }

    #[test]
    fn strips_at_start_of_text() {
        let r = bold("**hi**", 0, 6);
        let (new_text, _, _) = apply("**hi**", &r);
        assert_eq!(new_text, "hi");
    }

    #[test]
    fn round_trip() {
        let r1 = bold("hello", 0, 5);
        let (after_wrap, s, e) = apply("hello", &r1);
        assert_eq!(after_wrap, "**hello**");
        let r2 = bold(&after_wrap, s, e);
        let (after_unwrap, s2, e2) = apply(&after_wrap, &r2);
        assert_eq!(after_unwrap, "hello");
        assert_eq!((s2, e2), (0, 5));
    }
}

// ----------------------------------------------------------------------
// toggle_inline_wrap — italic (*)
// ----------------------------------------------------------------------

mod italic {
    use super::*;

    fn italic(text: &str, s: u32, e: u32) -> ComposerTransformResult {
        toggle_inline_wrap(text, s, e, "*")
    }

    #[test]
    fn wraps_simple() {
        let r = italic("foo", 0, 3);
        let (new_text, s, e) = apply("foo", &r);
        assert_eq!(new_text, "*foo*");
        assert_eq!((s, e), (0, 5));
    }

    #[test]
    fn strips_inside() {
        let r = italic("*foo*", 0, 5);
        let (new_text, s, e) = apply("*foo*", &r);
        assert_eq!(new_text, "foo");
        assert_eq!((s, e), (0, 3));
    }

    #[test]
    fn strips_by_context() {
        let r = italic("*foo*", 1, 4);
        let (new_text, s, e) = apply("*foo*", &r);
        assert_eq!(new_text, "foo");
        assert_eq!((s, e), (0, 3));
    }

    #[test]
    fn does_not_strip_bold_inside() {
        // **foo** fully selected, italic toggle. nl=2 (even) → fail.
        // Falls through to wrap → ***foo***.
        let r = italic("**foo**", 0, 7);
        let (new_text, s, e) = apply("**foo**", &r);
        assert_eq!(new_text, "***foo***");
        assert_eq!((s, e), (0, 9));
    }

    #[test]
    fn does_not_strip_bold_via_context() {
        // selection "foo" inside **foo**. Italic context check sees `*`
        // immediately adjacent but `**` two-back/forward → guard fails.
        let r = italic("**foo**", 2, 5);
        let (new_text, _, _) = apply("**foo**", &r);
        assert_eq!(new_text, "***foo***");
    }

    #[test]
    fn strips_italic_layer_of_bold_italic() {
        // ***foo*** with italic toggle: nl=3,nr=3 both odd, total=9,
        // nl+nr=6 <= 9. Strip 1 → **foo**.
        let r = italic("***foo***", 0, 9);
        let (new_text, s, e) = apply("***foo***", &r);
        assert_eq!(new_text, "**foo**");
        assert_eq!((s, e), (0, 7));
    }

    #[test]
    fn three_star_only_no_strip() {
        // "***" — nl=3, nr=3, total=3. nl+nr=6 > 3 (overlap). No strip.
        let r = italic("***", 0, 3);
        let (new_text, _, _) = apply("***", &r);
        assert_eq!(new_text, "*****");
    }

    #[test]
    fn asymmetric_does_not_strip() {
        // "**foo*" — left has `**`, right has `*`. starts_with `*` true,
        // ends_with `*` true. Italic context-strip uses both ends.
        // Strip-inside: nl=2 (even). Fail. Falls through to wrap.
        let r = italic("**foo*", 0, 6);
        let (new_text, _, _) = apply("**foo*", &r);
        assert_eq!(new_text, "***foo**");
    }

    #[test]
    fn emoji_round_trip() {
        let r1 = italic("🚜", 0, 2);
        let (after_wrap, _, _) = apply("🚜", &r1);
        assert_eq!(after_wrap, "*🚜*");
        let r2 = italic(&after_wrap, 0, 4);
        let (after_unwrap, _, _) = apply(&after_wrap, &r2);
        assert_eq!(after_unwrap, "🚜");
    }
}

// ----------------------------------------------------------------------
// toggle_inline_wrap — inline code (`)
// ----------------------------------------------------------------------

mod inline_code {
    use super::*;

    fn code(text: &str, s: u32, e: u32) -> ComposerTransformResult {
        toggle_inline_wrap(text, s, e, "`")
    }

    #[test]
    fn wraps() {
        let r = code("foo", 0, 3);
        let (new_text, s, e) = apply("foo", &r);
        assert_eq!(new_text, "`foo`");
        assert_eq!((s, e), (0, 5));
    }

    #[test]
    fn strips_inside() {
        let r = code("`foo`", 0, 5);
        let (new_text, _, _) = apply("`foo`", &r);
        assert_eq!(new_text, "foo");
    }

    #[test]
    fn strips_by_context() {
        let r = code("`foo`", 1, 4);
        let (new_text, _, _) = apply("`foo`", &r);
        assert_eq!(new_text, "foo");
    }

    #[test]
    fn marker_only_no_strip() {
        let r = code("`", 0, 1);
        let (new_text, _, _) = apply("`", &r);
        assert_eq!(new_text, "```");
    }

    #[test]
    fn round_trip() {
        let r1 = code("hello", 0, 5);
        let (after, s, e) = apply("hello", &r1);
        assert_eq!(after, "`hello`");
        let r2 = code(&after, s, e);
        let (back, s2, e2) = apply(&after, &r2);
        assert_eq!(back, "hello");
        assert_eq!((s2, e2), (0, 5));
    }
}

// ----------------------------------------------------------------------
// toggle_block_prefix — quote ("> ")
// ----------------------------------------------------------------------

mod quote {
    use super::*;

    fn quote(text: &str, s: u32, e: u32) -> ComposerTransformResult {
        toggle_block_prefix(text, s, e, "> ")
    }

    #[test]
    fn empty_sel_is_noop() {
        assert!(!quote("hello", 2, 2).applied);
        assert!(!quote("", 0, 0).applied);
    }

    #[test]
    fn quotes_single_line() {
        let r = quote("hello", 0, 5);
        let (new_text, s, e) = apply("hello", &r);
        assert_eq!(new_text, "> hello");
        assert_eq!((s, e), (0, 7));
    }

    #[test]
    fn quotes_two_full_lines() {
        let r = quote("a\nb", 0, 3);
        let (new_text, s, e) = apply("a\nb", &r);
        assert_eq!(new_text, "> a\n> b");
        assert_eq!((s, e), (0, 7));
    }

    #[test]
    fn partial_selection_expands_to_full_lines() {
        let r = quote("hello", 1, 3);
        let (new_text, s, e) = apply("hello", &r);
        assert_eq!(new_text, "> hello");
        assert_eq!((s, e), (0, 7));
    }

    #[test]
    fn unquotes_when_all_lines_quoted() {
        let r = quote("> a\n> b", 0, 7);
        let (new_text, s, e) = apply("> a\n> b", &r);
        assert_eq!(new_text, "a\nb");
        assert_eq!((s, e), (0, 3));
    }

    #[test]
    fn unquotes_no_space_form() {
        let r = quote(">a\n>b", 0, 5);
        let (new_text, _, _) = apply(">a\n>b", &r);
        assert_eq!(new_text, "a\nb");
    }

    #[test]
    fn quotes_then_unquotes_round_trip() {
        let r1 = quote("hello\nworld", 0, 11);
        let (after, s, e) = apply("hello\nworld", &r1);
        assert_eq!(after, "> hello\n> world");
        let r2 = quote(&after, s, e);
        let (back, _, _) = apply(&after, &r2);
        assert_eq!(back, "hello\nworld");
    }

    #[test]
    fn mixed_quoted_unquoted_treats_as_add() {
        let r = quote("> a\nb", 0, 5);
        let (new_text, _, _) = apply("> a\nb", &r);
        assert_eq!(new_text, "> > a\n> b");
    }

    #[test]
    fn empty_lines_get_quoted() {
        let r = quote("a\n\nb", 0, 4);
        let (new_text, _, _) = apply("a\n\nb", &r);
        assert_eq!(new_text, "> a\n>\n> b");
    }

    #[test]
    fn unquotes_with_empty_lines() {
        let r = quote("> a\n>\n> b", 0, 9);
        let (new_text, _, _) = apply("> a\n>\n> b", &r);
        assert_eq!(new_text, "a\n\nb");
    }

    #[test]
    fn trailing_newline_does_not_promote_phantom_line() {
        // sel ends right after the final \n. The phantom empty trailing
        // line must NOT be quoted.
        let r = quote("a\nb\n", 0, 4);
        let (new_text, _, _) = apply("a\nb\n", &r);
        assert_eq!(new_text, "> a\n> b\n");
    }

    #[test]
    fn selection_at_eof_no_trailing_newline() {
        let r = quote("a\nb", 0, 3);
        let (new_text, _, _) = apply("a\nb", &r);
        assert_eq!(new_text, "> a\n> b");
    }

    #[test]
    fn multi_level_quote_strips_one_level() {
        let r = quote("> > foo", 0, 7);
        let (new_text, _, _) = apply("> > foo", &r);
        assert_eq!(new_text, "> foo");
    }

    #[test]
    fn add_on_already_quoted_promotes() {
        let r = quote("a\n> b", 0, 5);
        let (new_text, _, _) = apply("a\n> b", &r);
        assert_eq!(new_text, "> a\n> > b");
    }

    #[test]
    fn emoji_inside_quoted_line() {
        let r = quote("🚜 hello", 0, 9);
        let (new_text, _, _) = apply("🚜 hello", &r);
        assert_eq!(new_text, "> 🚜 hello");
    }
}

// ----------------------------------------------------------------------
// toggle_code
// ----------------------------------------------------------------------

mod code {
    use super::*;

    #[test]
    fn empty_sel_is_noop() {
        assert!(!toggle_code("hi", 1, 1).applied);
    }

    #[test]
    fn single_line_wraps_inline() {
        let r = toggle_code("hello", 0, 5);
        let (new_text, s, e) = apply("hello", &r);
        assert_eq!(new_text, "`hello`");
        assert_eq!((s, e), (0, 7));
    }

    #[test]
    fn multi_line_wraps_fenced() {
        let r = toggle_code("line1\nline2", 0, 11);
        let (new_text, s, e) = apply("line1\nline2", &r);
        assert_eq!(new_text, "```\nline1\nline2\n```");
        assert_eq!((s, e), (0, 19));
    }

    #[test]
    fn strips_fenced_inside() {
        let r = toggle_code("```\nfoo\n```", 0, 11);
        let (new_text, s, e) = apply("```\nfoo\n```", &r);
        assert_eq!(new_text, "foo");
        assert_eq!((s, e), (0, 3));
    }

    #[test]
    fn strips_fenced_context() {
        // sel just the inner content of an existing fenced block.
        let r = toggle_code("```\nfoo\n```", 4, 7);
        let (new_text, _, _) = apply("```\nfoo\n```", &r);
        assert_eq!(new_text, "foo");
    }

    #[test]
    fn strips_inline_round_trip() {
        let r1 = toggle_code("hello", 0, 5);
        let (after, _, _) = apply("hello", &r1);
        let r2 = toggle_code(&after, 0, 7);
        let (back, _, _) = apply(&after, &r2);
        assert_eq!(back, "hello");
    }

    #[test]
    fn strips_fenced_round_trip() {
        let r1 = toggle_code("line1\nline2", 0, 11);
        let (after, _, _) = apply("line1\nline2", &r1);
        assert_eq!(after, "```\nline1\nline2\n```");
        let r2 = toggle_code(&after, 0, 19);
        let (back, _, _) = apply(&after, &r2);
        assert_eq!(back, "line1\nline2");
    }
}

// ----------------------------------------------------------------------
// toggle_link
// ----------------------------------------------------------------------

mod link {
    use super::*;

    #[test]
    fn empty_sel_inserts_skeleton_with_cursor_in_brackets() {
        let r = toggle_link("hello ", 6, 6);
        let (new_text, s, e) = apply("hello ", &r);
        assert_eq!(new_text, "hello []()");
        assert_eq!((s, e), (7, 7));
    }

    #[test]
    fn wraps_non_url_label_cursor_in_parens() {
        let r = toggle_link("hello world", 6, 11);
        let (new_text, s, e) = apply("hello world", &r);
        assert_eq!(new_text, "hello [world]()");
        assert_eq!((s, e), (14, 14));
    }

    #[test]
    fn url_selection_reverse_wraps() {
        let r = toggle_link("https://example.com", 0, 19);
        let (new_text, s, e) = apply("https://example.com", &r);
        assert_eq!(new_text, "[](https://example.com)");
        assert_eq!((s, e), (1, 1));
    }

    #[test]
    fn http_is_url() {
        assert!(is_url_shape("http://x"));
        assert!(is_url_shape("https://x"));
        assert!(is_url_shape("matrix://x"));
    }

    #[test]
    fn non_url_shapes() {
        assert!(!is_url_shape(""));
        assert!(!is_url_shape("www.example.com"));
        assert!(!is_url_shape("foo bar"));
        assert!(!is_url_shape("#room:example.com"));
        assert!(!is_url_shape("mailto:foo@bar.com")); // no `://`
    }

    #[test]
    fn unwraps_inner_text_of_link() {
        let r = toggle_link("[click](https://x)", 1, 6);
        let (new_text, s, e) = apply("[click](https://x)", &r);
        assert_eq!(new_text, "click");
        assert_eq!((s, e), (0, 5));
    }

    #[test]
    fn unwraps_full_shape() {
        let r = toggle_link("[click](https://x)", 0, 18);
        let (new_text, s, e) = apply("[click](https://x)", &r);
        assert_eq!(new_text, "click");
        assert_eq!((s, e), (0, 5));
    }

    #[test]
    fn unwraps_empty_url() {
        let r = toggle_link("[click]()", 1, 6);
        let (new_text, _, _) = apply("[click]()", &r);
        assert_eq!(new_text, "click");
    }

    #[test]
    fn unclosed_does_not_unwrap() {
        // "[click" — no closing `]` in inner-text context.
        let r = toggle_link("[click", 1, 6);
        let (new_text, _, _) = apply("[click", &r);
        // Wraps normally (before-char `[`, after-char nothing → context-strip fails).
        assert_eq!(new_text, "[[click]()");
    }

    #[test]
    fn wrap_url_round_trip() {
        let r1 = toggle_link("https://x", 0, 9);
        let (after, _, _) = apply("https://x", &r1);
        assert_eq!(after, "[](https://x)");
        // Now type a label by selecting and unwrapping... simulate.
        let r2 = toggle_link(&after, 0, 13);
        let (back, _, _) = apply(&after, &r2);
        assert_eq!(back, "");
    }

    #[test]
    fn emoji_selection_wraps() {
        let r = toggle_link("🚜", 0, 2);
        let (new_text, s, e) = apply("🚜", &r);
        assert_eq!(new_text, "[🚜]()");
        assert_eq!((s, e), (5, 5));
    }
}

// ----------------------------------------------------------------------
// UTF-16 helper tests
// ----------------------------------------------------------------------

mod utf16_helpers {
    use super::*;

    #[test]
    fn utf16_to_byte_basic() {
        assert_eq!(utf16_to_byte_offset("hello", 0), 0);
        assert_eq!(utf16_to_byte_offset("hello", 5), 5);
        assert_eq!(utf16_to_byte_offset("hello", 999), 5);
    }

    #[test]
    fn utf16_to_byte_with_emoji() {
        // "a🚜b" — 'a'(1) + '🚜'(2 UTF-16) + 'b'(1) = 4 UTF-16 units
        assert_eq!(utf16_to_byte_offset("a🚜b", 0), 0);
        assert_eq!(utf16_to_byte_offset("a🚜b", 1), 1); // after 'a'
        assert_eq!(utf16_to_byte_offset("a🚜b", 3), 5); // after 🚜
        assert_eq!(utf16_to_byte_offset("a🚜b", 4), 6); // after 'b'
        // Mid-surrogate (utf16 index 2) rounds up to the codepoint
        // boundary right after the emoji.
        assert_eq!(utf16_to_byte_offset("a🚜b", 2), 5);
    }

    #[test]
    fn byte_to_utf16_basic() {
        assert_eq!(byte_to_utf16_offset("hello", 0), 0);
        assert_eq!(byte_to_utf16_offset("hello", 5), 5);
        assert_eq!(byte_to_utf16_offset("a🚜b", 5), 3);
    }
}
