// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

fn count_occurrences(text: &str, needle: &str) -> usize {
    text.match_indices(needle).count()
}

#[test]
fn mark_passes_through_when_query_or_html_is_empty() {
    assert_eq!(mark_search_matches("", "foo"), "");
    assert_eq!(mark_search_matches("<p>hello</p>", ""), "<p>hello</p>");
}

#[test]
fn mark_wraps_plain_text_match() {
    let out = mark_search_matches("<p>hello world</p>", "world");
    assert_eq!(out, r##"<p>hello <span class="komai-search-match">world</span></p>"##);
}

#[test]
fn mark_is_case_insensitive() {
    let out = mark_search_matches("<p>Hello WORLD</p>", "world");
    assert_eq!(out, r##"<p>Hello <span class="komai-search-match">WORLD</span></p>"##);
}

#[test]
fn mark_preserves_original_casing_of_match() {
    let out = mark_search_matches("<p>aBc xYz aBc</p>", "abc");
    assert_eq!(
        count_occurrences(&out, r##"<span class="komai-search-match">aBc</span>"##),
        2,
        "both occurrences keep their original casing inside the mark wrapper",
    );
}

#[test]
fn mark_skips_tag_names_and_attributes() {
    // Searching "a" should not wrap the <a> tag name or its href attribute name.
    let html = r#"<a href="https://example.com">click</a>"#;
    let out = mark_search_matches(html, "a");
    assert!(out.starts_with("<a href=\"https://example.com\">"),
            "the literal anchor tag is preserved verbatim, got: {out}");
    assert!(!out.contains(r##"<<span class="komai-search-match">"##),
            "nothing inside the tag bytes is marked");
}

#[test]
fn mark_wraps_inside_code_and_pre() {
    let html = "<pre><code>foo bar foo</code></pre>";
    let out = mark_search_matches(html, "foo");
    assert_eq!(
        count_occurrences(&out, r##"<span class="komai-search-match">foo</span>"##),
        2,
        "matches inside code/pre are marked",
    );
}

#[test]
fn mark_anchor_href_match_wraps_link_text_when_visible_text_has_no_match() {
    let html = r#"<a href="https://deadbeef.com/">Grey's Anatomy</a>"#;
    let out = mark_search_matches(html, "deadbeef");
    assert_eq!(
        out,
        r#"<a href="https://deadbeef.com/"><span class="komai-search-match">Grey's Anatomy</span></a>"#,
        "href-only match wraps the entire link text",
    );
}

#[test]
fn mark_anchor_href_match_with_visible_match_uses_substring_marks_only() {
    // When the visible text also contains the query, only the substring is
    // wrapped — no outer wrap-the-whole-anchor, which would be redundant.
    let html = r#"<a href="https://deadbeef.com/">deadbeef site</a>"#;
    let out = mark_search_matches(html, "deadbeef");
    assert_eq!(
        out,
        r#"<a href="https://deadbeef.com/"><span class="komai-search-match">deadbeef</span> site</a>"#,
    );
}

#[test]
fn mark_anchor_without_href_match_does_not_wrap_link_text() {
    let html = r#"<a href="https://example.com/">click here</a>"#;
    let out = mark_search_matches(html, "deadbeef");
    assert_eq!(out, html, "no match anywhere, html is unchanged");
}

#[test]
fn mark_multiple_matches_in_one_text_segment() {
    let html = "<p>abc abc abc</p>";
    let out = mark_search_matches(html, "abc");
    assert_eq!(
        out,
        r##"<p><span class="komai-search-match">abc</span> <span class="komai-search-match">abc</span> <span class="komai-search-match">abc</span></p>"##,
    );
}

#[test]
fn mark_does_not_match_across_tag_boundary() {
    // The 'fo' span straddles a <span>; we should match only "foo" inside one text node.
    let html = "<p>foo<span>bar</span>foo</p>";
    let out = mark_search_matches(html, "foo");
    assert_eq!(
        out,
        r##"<p><span class="komai-search-match">foo</span><span>bar</span><span class="komai-search-match">foo</span></p>"##,
    );
}

#[test]
fn mark_preserves_multibyte_text() {
    let html = "<p>café CAFÉ</p>";
    let out = mark_search_matches(html, "café");
    // Only the lowercase form matches byte-for-byte (ASCII-CI folds the
    // 'C' but the accented 'É' is a different byte sequence from 'é').
    assert!(out.contains(r##"<span class="komai-search-match">café</span>"##),
            "lowercase form is marked: {out}");
    assert!(!out.contains(r##"<span class="komai-search-match">CAFÉ</span>"##),
            "uppercase é is NOT marked under ASCII-CI: {out}");
}

#[test]
fn mark_query_longer_than_text_is_noop() {
    let html = "<p>hi</p>";
    let out = mark_search_matches(html, "looking for a longer query");
    assert_eq!(out, html);
}

#[test]
fn mark_anchor_href_match_inside_nested_anchors() {
    // Sanity: only the innermost anchor's href flag drives wrapping.
    let html = r#"<a href="https://outer.com/"><a href="https://inner.com/">text</a></a>"#;
    let out = mark_search_matches(html, "inner");
    assert!(
        out.contains(r##"<span class="komai-search-match">text</span>"##),
        "inner anchor href match wraps its text, got: {out}",
    );
}

#[test]
fn mark_anchor_close_pops_correctly() {
    // After </a>, subsequent text outside the matching anchor should
    // not be wrapped (no href context to inherit).
    let html = r#"<a href="https://deadbeef.com/">link</a> tail"#;
    let out = mark_search_matches(html, "deadbeef");
    assert_eq!(out, r#"<a href="https://deadbeef.com/"><span class="komai-search-match">link</span></a> tail"#);
}

#[test]
fn mark_query_equal_to_tag_name_is_safe() {
    // Searching "code" against text inside a <code> element must not
    // break HTML — the <code> bytes are skipped, only inner text is marked.
    let html = "<code>code is code</code>";
    let out = mark_search_matches(html, "code");
    assert_eq!(
        out,
        r##"<code><span class="komai-search-match">code</span> is <span class="komai-search-match">code</span></code>"##,
    );
}
