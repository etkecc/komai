// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

fn count_occurrences(text: &str, needle: &str) -> usize {
    text.match_indices(needle).count()
}

#[test]
fn escapes_disallowed_tags() {
    let out = sanitize_html("<script>alert(1)</script><b>ok</b>");
    assert!(!out.contains("<script"), "script tag is not kept");
    assert!(out.contains("&lt;script"), "script opening is escaped");
    assert!(out.contains("<b>ok</b>"), "allowed tag remains");
}

#[test]
fn strips_mx_reply_fallback() {
    let out = sanitize_html("<mx-reply><blockquote>old</blockquote></mx-reply><p>new</p>");
    assert!(!out.contains("mx-reply"), "mx-reply tags are stripped");
    assert!(!out.contains("old"), "mx-reply content is stripped");
    assert!(out.contains("<p>new</p>"), "non-reply content remains");
}

#[test]
fn anchor_attribute_filtering() {
    let out = sanitize_html(
        r#"<a href="https://example.org" onclick="evil()" target="_blank">ok</a>"#,
    );
    assert!(
        out.contains(r#"href="https://example.org""#),
        "safe href is preserved"
    );
    assert!(
        out.contains(r#"target="_blank""#),
        "target is preserved"
    );
    assert!(!out.contains("onclick"), "unsafe onclick attribute is removed");
}

#[test]
fn href_scheme_and_relative_filtering() {
    let js_out = sanitize_html(r#"<a href="javascript:alert(1)">x</a>"#);
    let rel_out = sanitize_html(r#"<a href="/relative">x</a>"#);
    assert!(!js_out.contains("href="), "javascript href is removed");
    assert!(!rel_out.contains("href="), "relative href is removed");
}

#[test]
fn image_src_filtering() {
    let safe_out =
        sanitize_html(r#"<img src="mxc://example.org/id" height="24" width="32" alt="a">"#);
    let unsafe_out =
        sanitize_html(r#"<img src="https://example.org/x.png" onerror="e()">"#);
    assert!(
        safe_out.contains(r#"src="image://mxcImage/example.org/id""#),
        "mxc image source is rewritten to image://mxcImage/"
    );
    assert!(
        !unsafe_out.contains(r#"src="https://"#),
        "non-mxc image source is removed"
    );
    assert!(
        !unsafe_out.contains("onerror"),
        "unsafe image attribute is removed"
    );

    // Verify that a pre-crafted image:// URL cannot bypass the mxc:// gate.
    let injected =
        sanitize_html(r#"<img src="image://mxcImage/evil.org/payload">"#);
    assert!(
        !injected.contains("image://"),
        "image:// src must be rejected (only mxc:// is accepted)"
    );
}

#[test]
fn code_class_filtering() {
    let out = sanitize_html(r#"<code class="foo language-cpp language-python bad">x</code>"#);
    assert!(
        out.contains(r#"class="language-cpp language-python""#),
        "only language-* code classes are preserved"
    );
    assert!(!out.contains("foo"), "non-language classes are removed");
}

#[test]
fn span_color_validation() {
    let out = sanitize_html(
        r##"<span data-mx-color="#112233" data-mx-bg-color="#GGGGGG" data-mx-spoiler="spoiler" style="x">x</span>"##,
    );
    assert!(
        out.contains(r##"data-mx-color="#112233""##),
        "valid data-mx-color is preserved"
    );
    assert!(
        !out.contains(r##"data-mx-bg-color="#GGGGGG""##),
        "invalid color value is removed"
    );
    assert!(
        out.contains(r#"data-mx-spoiler="spoiler""#),
        "spoiler attribute is preserved"
    );
    assert!(!out.contains("style="), "unsupported style attribute is removed");
}

#[test]
fn font_color_validation() {
    let out = sanitize_html(
        r##"<font color="#112233">x</font><font color="orange">x</font><font color="not-a-color">x</font>"##,
    );
    assert!(
        out.contains(r##"color="#112233""##),
        "valid hex color is preserved"
    );
    assert!(
        out.contains(r#"color="orange""#),
        "valid named color is preserved"
    );
    assert!(!out.contains("not-a-color"), "invalid color is removed");
}

#[test]
fn depth_limit() {
    let mut input = String::new();
    for _ in 0..101 {
        input.push_str("<div>");
    }
    input.push_str("payload");
    for _ in 0..101 {
        input.push_str("</div>");
    }
    let out = sanitize_html(&input);
    assert!(
        count_occurrences(&out, "<div>") <= 100,
        "opening tag depth is capped at 100"
    );
    assert!(out.contains("payload"), "inner payload remains readable");
}
