// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::send::{caption_text_content, formatted_html_from_markdown, html_visible_text_is_empty};

#[test]
fn html_visible_text_is_empty_detects_empty_list() {
    assert!(html_visible_text_is_empty("<ul>\n<li></li>\n</ul>\n"));
    assert!(html_visible_text_is_empty("<h1></h1>"));
    assert!(html_visible_text_is_empty("<hr />"));
    assert!(html_visible_text_is_empty("<blockquote>\n</blockquote>"));
}

#[test]
fn html_visible_text_is_empty_keeps_real_content() {
    assert!(!html_visible_text_is_empty("<ul><li>foo</li></ul>"));
    assert!(!html_visible_text_is_empty("<h1>title</h1>"));
    assert!(!html_visible_text_is_empty("<p><strong>bold</strong></p>"));
}

#[test]
fn html_visible_text_is_empty_decodes_entities() {
    assert!(!html_visible_text_is_empty("<p>&lt;</p>"));
    assert!(!html_visible_text_is_empty("<p>&amp;</p>"));
}

#[test]
fn formatted_html_skipped_for_lone_asterisk() {
    // Regression for https://github.com/etkecc/komai/issues/104:
    // pulldown-cmark turns `*` into `<ul><li></li></ul>`, which is
    // worthless as `formatted_body` and would hide the body on render.
    assert_eq!(formatted_html_from_markdown("*", true), None);
}

#[test]
fn formatted_html_skipped_for_other_visibly_empty_markdown() {
    assert_eq!(formatted_html_from_markdown("# ", true), None);
    assert_eq!(formatted_html_from_markdown("***", true), None);
    assert_eq!(formatted_html_from_markdown("- ", true), None);
}

#[test]
fn formatted_html_skipped_for_plain_text() {
    // Existing guard: a paragraph wrapper around plain text is no improvement.
    assert_eq!(formatted_html_from_markdown("hello world", true), None);
}

#[test]
fn formatted_html_kept_for_real_markdown() {
    assert!(formatted_html_from_markdown("**bold**", true).is_some());
    assert!(formatted_html_from_markdown("* foo", true).is_some());
    assert!(formatted_html_from_markdown("- a\n- b", true).is_some());
    assert!(formatted_html_from_markdown("## heading", true).is_some());
}

#[test]
fn formatted_html_disabled_when_markdown_off() {
    assert_eq!(formatted_html_from_markdown("**bold**", false), None);
}

#[test]
fn raw_html_alone_is_not_formatting() {
    // CommonMark would pass a typed `<pre>` through as a real HTML block
    // that swallows the rest of the message for every recipient. Tag-like
    // tokens in chat are almost always meant literally, so raw HTML is
    // demoted to text; with no other markdown present, the message sends
    // plain, matching Element.
    assert_eq!(
        formatted_html_from_markdown("Text with <pre> inside it.\nLet's see what happens!", true),
        None
    );
    assert_eq!(formatted_html_from_markdown("<pre>\nhello\n</pre>", true), None);
    assert_eq!(formatted_html_from_markdown("<b>bold?</b>", true), None);
}

#[test]
fn raw_html_is_escaped_when_markdown_is_present() {
    let html = formatted_html_from_markdown("**bold** and a <pre> tag", true).unwrap();
    assert!(html.contains("&lt;pre&gt;"), "tag should be escaped: {html}");
    assert!(!html.contains("<pre"), "tag must not become markup: {html}");
}

#[test]
fn raw_html_demotion_leaves_code_and_autolinks_alone() {
    let html = formatted_html_from_markdown("`a < b`", true).unwrap();
    assert!(html.contains("<code>a &lt; b</code>"), "unexpected: {html}");

    let html = formatted_html_from_markdown("see <https://example.com> now", true).unwrap();
    assert!(html.contains("<a "), "autolink should stay a link: {html}");
    assert!(html.contains("https://example.com"), "unexpected: {html}");
}

#[test]
fn multi_line_raw_html_keeps_line_breaks_as_hard_breaks() {
    // When raw HTML is demoted next to real markdown, its newlines must
    // survive as <br> so the literal lines don't collapse when rendered.
    let html =
        formatted_html_from_markdown("**bold**\n\n<pre>\nline two\n</pre>", true).unwrap();
    assert!(html.contains("&lt;pre&gt;"), "unexpected: {html}");
    assert!(html.contains("line two"), "unexpected: {html}");
    assert!(html.contains("<br"), "newlines should become breaks: {html}");
}

#[test]
fn caption_carries_formatted_body_for_real_markdown() {
    let content = caption_text_content("**bold** [link](https://example.com)", true);
    assert_eq!(content.body, "**bold** [link](https://example.com)");
    let formatted = content.formatted.expect("expected formatted_body");
    assert!(formatted.body.contains("<strong>bold</strong>"));
    assert!(formatted.body.contains("href=\"https://example.com\""));
}

#[test]
fn caption_omits_formatted_body_when_markdown_off() {
    let content = caption_text_content("**bold**", false);
    assert!(content.formatted.is_none());
}

#[test]
fn caption_omits_formatted_body_for_plain_text_when_markdown_on() {
    // Same guard as text messages: a paragraph wrapper around plain text
    // doesn't add value, so we don't pay the formatted_body cost.
    let content = caption_text_content("hello world", true);
    assert!(content.formatted.is_none());
}
