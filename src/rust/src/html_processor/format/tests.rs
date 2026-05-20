// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

#[test]
fn plain_text_preserves_double_newlines() {
    let out = plain_text_to_html("Hello!\n\nWorld!\n\nAnother");
    assert_eq!(out, "<p>Hello!</p><p>World!</p><p>Another</p>");
}

#[test]
fn plain_text_preserves_single_newlines() {
    let out = plain_text_to_html("Line1\nLine2\nLine3");
    assert_eq!(out, "<p>Line1<br>Line2<br>Line3</p>");
}

#[test]
fn plain_text_mixed_newlines() {
    let out = plain_text_to_html("A\nB\n\nC\nD");
    assert_eq!(out, "<p>A<br>B</p><p>C<br>D</p>");
}

#[test]
fn plain_text_escapes_html() {
    let out = plain_text_to_html("<b>bold</b> & \"quoted\"");
    assert!(!out.contains("<b>"), "HTML tags are escaped");
    assert!(out.contains("&amp;"), "ampersand is escaped");
    assert!(out.contains("&lt;b&gt;"), "angle brackets are escaped");
}

#[test]
fn plain_text_empty() {
    assert!(plain_text_to_html("").is_empty(), "empty input produces empty output");
}
