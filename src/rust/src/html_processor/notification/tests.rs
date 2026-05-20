// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::to_notification_markup;

#[test]
fn empty_input_returns_empty() {
    assert_eq!(to_notification_markup(""), "");
}

#[test]
fn plain_text_passes_through_unchanged() {
    assert_eq!(to_notification_markup("Hello, world!"), "Hello, world!");
}

#[test]
fn html_entities_preserved() {
    // Pango/body-markup parses entities itself; we must leave them encoded.
    assert_eq!(
        to_notification_markup("Tom &amp; Jerry &lt;3"),
        "Tom &amp; Jerry &lt;3"
    );
}

#[test]
fn bare_lt_becomes_entity() {
    // A `<` that isn't a valid tag opener gets escaped to keep output well-formed.
    assert_eq!(to_notification_markup("2 < 3"), "2 &lt; 3");
}

#[test]
fn keeps_b_i_u_tags() {
    assert_eq!(
        to_notification_markup("<b>bold</b> <i>italic</i> <u>under</u>"),
        "<b>bold</b> <i>italic</i> <u>under</u>"
    );
}

#[test]
fn rewrites_strong_to_b() {
    assert_eq!(to_notification_markup("<strong>hi</strong>"), "<b>hi</b>");
}

#[test]
fn rewrites_em_to_i() {
    assert_eq!(to_notification_markup("<em>hi</em>"), "<i>hi</i>");
}

#[test]
fn paragraphs_separated_by_blank_line() {
    // From issue #180: the reporter's failing case (HTML <p> wrappers around
    // newlines).
    assert_eq!(
        to_notification_markup("<p>Hello!</p>\n<p><strong>World</strong>!</p>\n"),
        "Hello!\n\n<b>World</b>!"
    );
}

#[test]
fn single_paragraph_trims_to_content() {
    assert_eq!(to_notification_markup("<p>Just one line</p>"), "Just one line");
}

#[test]
fn br_becomes_newline() {
    assert_eq!(to_notification_markup("line one<br/>line two"), "line one\nline two");
    assert_eq!(to_notification_markup("line one<br>line two"), "line one\nline two");
}

#[test]
fn unordered_list_gets_bullets() {
    assert_eq!(
        to_notification_markup("<ul>\n<li>test</li>\n<li>test 2</li>\n</ul>\n"),
        "• test\n\n• test 2"
    );
}

#[test]
fn ordered_list_also_gets_bullets() {
    // We don't preserve numbering; bullets are good enough for a notification.
    assert_eq!(
        to_notification_markup("<ol><li>one</li><li>two</li></ol>"),
        "• one\n• two"
    );
}

#[test]
fn blockquote_drops_wrapper() {
    assert_eq!(
        to_notification_markup("<blockquote>quoted</blockquote>after"),
        "quoted\n\nafter"
    );
}

#[test]
fn headings_act_as_paragraphs() {
    assert_eq!(
        to_notification_markup("<h1>Title</h1><p>body</p>"),
        "Title\n\nbody"
    );
}

#[test]
fn inline_code_strips_tag() {
    assert_eq!(
        to_notification_markup("call <code>foo()</code> please"),
        "call foo() please"
    );
}

#[test]
fn code_block_keeps_content_with_break() {
    assert_eq!(
        to_notification_markup("<pre><code>let x = 1;</code></pre>after"),
        "let x = 1;\n\nafter"
    );
}

#[test]
fn strikethrough_variants_lose_styling() {
    assert_eq!(to_notification_markup("<del>x</del>"), "x");
    assert_eq!(to_notification_markup("<s>x</s>"), "x");
    assert_eq!(to_notification_markup("<strike>x</strike>"), "x");
}

#[test]
fn span_with_color_drops_tag_keeps_content() {
    assert_eq!(
        to_notification_markup("<span data-mx-color=\"#ff0000\">red</span>"),
        "red"
    );
}

#[test]
fn font_with_color_drops_tag_keeps_content() {
    assert_eq!(
        to_notification_markup("<font color=\"#ff0000\">red</font>"),
        "red"
    );
}

#[test]
fn anchor_with_https_preserved() {
    assert_eq!(
        to_notification_markup("<a href=\"https://example.com/\">Example</a>"),
        "<a href=\"https://example.com/\">Example</a>"
    );
}

#[test]
fn anchor_with_http_preserved() {
    assert_eq!(
        to_notification_markup("<a href=\"http://example.com\">x</a>"),
        "<a href=\"http://example.com\">x</a>"
    );
}

#[test]
fn anchor_with_mailto_preserved() {
    assert_eq!(
        to_notification_markup("<a href=\"mailto:a@b.example\">mail</a>"),
        "<a href=\"mailto:a@b.example\">mail</a>"
    );
}

#[test]
fn anchor_with_matrix_scheme_preserved() {
    assert_eq!(
        to_notification_markup("<a href=\"matrix:r/room:example.org\">room</a>"),
        "<a href=\"matrix:r/room:example.org\">room</a>"
    );
}

#[test]
fn anchor_with_javascript_scheme_stripped() {
    assert_eq!(
        to_notification_markup("<a href=\"javascript:alert(1)\">click</a>"),
        "click"
    );
}

#[test]
fn anchor_without_href_stripped() {
    assert_eq!(
        to_notification_markup("<a>plain</a>"),
        "plain"
    );
}

#[test]
fn anchor_with_empty_href_stripped() {
    assert_eq!(
        to_notification_markup("<a href=\"\">x</a>"),
        "x"
    );
}

#[test]
fn anchor_href_attribute_gets_html_escaped() {
    // A href with a `&` (common in query strings) should be entity-escaped
    // inside the emitted attribute so Pango doesn't mis-parse it.
    assert_eq!(
        to_notification_markup("<a href=\"https://example.com/?a=1&b=2\">q</a>"),
        "<a href=\"https://example.com/?a=1&amp;b=2\">q</a>"
    );
}

#[test]
fn mx_reply_block_removed_entirely() {
    let input = "<mx-reply><blockquote><a href=\"https://matrix.to/#/!r:s/$e\">In reply to</a> <a href=\"https://matrix.to/#/@u:s\">@u:s</a><br/>original message</blockquote></mx-reply>my actual reply";
    assert_eq!(to_notification_markup(input), "my actual reply");
}

#[test]
fn nested_mx_reply_balanced() {
    let input = "<mx-reply>a<mx-reply>b</mx-reply>c</mx-reply>after";
    assert_eq!(to_notification_markup(input), "after");
}

#[test]
fn img_tag_dropped() {
    assert_eq!(
        to_notification_markup("see <img src=\"mxc://x/y\" alt=\"cat\"/> here"),
        "see  here"
    );
}

#[test]
fn unknown_tag_drops_keeps_content() {
    assert_eq!(
        to_notification_markup("<weird>content</weird>"),
        "content"
    );
}

#[test]
fn mixed_realistic_message() {
    let input = "<p><strong>Heads up</strong>: see <a href=\"https://example.com\">docs</a></p>\n<ul>\n<li>first</li>\n<li>second</li>\n</ul>";
    let expected = "<b>Heads up</b>: see <a href=\"https://example.com\">docs</a>\n\n• first\n\n• second";
    assert_eq!(to_notification_markup(input), expected);
}

#[test]
fn whitespace_collapsing_caps_at_two_newlines() {
    // Three or more consecutive newlines from inter-tag whitespace shrink to
    // a single paragraph gap; one newline stays as a line break.
    assert_eq!(
        to_notification_markup("<p>a</p>\n\n\n\n\n<p>b</p>"),
        "a\n\nb"
    );
}

#[test]
fn trailing_whitespace_trimmed() {
    assert_eq!(to_notification_markup("hi<br/><br/><br/>"), "hi");
}

#[test]
fn leading_whitespace_trimmed() {
    assert_eq!(to_notification_markup("<br/><br/>hi"), "hi");
}

#[test]
fn very_long_input_truncated_with_ellipsis() {
    let input = "x".repeat(2000);
    let out = to_notification_markup(&input);
    let char_count = out.chars().count();
    assert!(char_count <= 1024, "expected <=1024 chars, got {char_count}");
    assert!(out.ends_with('…'), "expected ellipsis suffix, got {out:?}");
}

#[test]
fn nested_anchor_with_bad_outer_keeps_inner() {
    // Defensive: Matrix doesn't normally produce nested anchors, but a bad
    // outer href shouldn't smother an otherwise-fine inner anchor's content.
    let input = "<a href=\"javascript:x\"><a href=\"https://ok.example\">in</a></a>";
    assert_eq!(
        to_notification_markup(input),
        "<a href=\"https://ok.example\">in</a>"
    );
}

#[test]
fn self_closing_br_inside_paragraph() {
    assert_eq!(
        to_notification_markup("<p>line one<br/>line two</p>"),
        "line one\nline two"
    );
}

#[test]
fn malformed_tag_left_unclosed() {
    // A `<` followed by non-tag bytes that never closes should not consume
    // the rest of the document. The parser returns invalid, we escape and
    // continue.
    assert_eq!(to_notification_markup("a < b and c"), "a &lt; b and c");
}

#[test]
fn details_summary_stripped() {
    assert_eq!(
        to_notification_markup("<details><summary>title</summary>body</details>"),
        "titlebody"
    );
}

#[test]
fn sup_sub_stripped() {
    assert_eq!(
        to_notification_markup("E=mc<sup>2</sup> H<sub>2</sub>O"),
        "E=mc2 H2O"
    );
}
