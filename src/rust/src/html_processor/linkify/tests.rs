// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

fn count_occurrences(text: &str, needle: &str) -> usize {
    text.match_indices(needle).count()
}

#[test]
fn linkify_plain_text_and_matrix_uri() {
    let out = linkify_html("See https://example.org and matrix:r/example.org/abc");
    assert!(
        out.contains(r#"<a href="https://example.org">https://example.org</a>"#),
        "plain https URL is linkified"
    );
    assert!(
        out.contains(r#"<a href="matrix:r/example.org/abc">matrix:r/example.org/abc</a>"#),
        "matrix URI is linkified"
    );
}

#[test]
fn linkify_skips_anchors_and_code_blocks() {
    let input = concat!(
        r#"<a href="https://already.example">https://text.example</a> "#,
        "<code>https://code.example</code> ",
        "<pre>https://pre.example</pre> ",
        "https://tail.example"
    );
    let out = linkify_html(input);
    assert_eq!(
        count_occurrences(&out, r#"<a href="https://already.example">"#),
        1,
        "existing anchor tag remains unchanged"
    );
    assert!(
        out.contains("<code>https://code.example</code>"),
        "code block text is not linkified"
    );
    assert!(
        out.contains("<pre>https://pre.example</pre>"),
        "pre block text is not linkified"
    );
    assert!(
        out.contains(r#"<a href="https://tail.example">https://tail.example</a>"#),
        "trailing plain URL is linkified"
    );
}
