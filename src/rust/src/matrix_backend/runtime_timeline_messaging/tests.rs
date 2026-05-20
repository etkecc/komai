// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::send::{
    caption_text_content, formatted_html_from_markdown, html_uses_only_plain_text_wrappers,
    html_visible_text_is_empty,
};

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
