// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Top-level pipeline that wires sanitize → linkify → pill decoration together.

use crate::ffi::HtmlPillAvatar;

use super::linkify::linkify_html;
use super::pills::{MATRIX_TO_PREFIX, build_avatar_map, decorate_matrix_pills};
use super::sanitize::sanitize_html;
use super::util::html_escape;

fn plain_text_to_html(body: &str) -> String {
    if body.is_empty() {
        return String::new();
    }

    let escaped = html_escape(body);
    let mut html = String::with_capacity(escaped.len() + 7);
    html.push_str("<p>");
    html.push_str(
        &escaped
            .replace("\n\n", "</p><p>")
            .replace('\n', "<br>"),
    );
    html.push_str("</p>");
    html
}

/// Process a timeline message's HTML through the full pipeline.
///
/// 1. If `formatted_body` is present: sanitize → syntax-highlight → linkify → decorate pills
/// 2. If only `body`: convert plain text to HTML → linkify
///
/// The caller is responsible for emoji replacement (stays in C++).
pub(crate) fn format_body_html(
    body: &str,
    formatted_body: &str,
    pill_avatars: &[HtmlPillAvatar],
    pill_avatar_size: u32,
    code_background: &str,
    syntax_highlight: bool,
) -> String {
    if body.is_empty() && formatted_body.is_empty() {
        return String::new();
    }

    if !formatted_body.is_empty() {
        let html = sanitize_html(formatted_body);

        let html = if syntax_highlight {
            crate::syntax_highlight::highlight_formatted_code_blocks(&html, code_background)
        } else {
            html
        };

        let html = linkify_html(&html);

        // Build avatar map lazily — only when the HTML actually contains matrix.to links.
        if html.contains(MATRIX_TO_PREFIX) {
            let avatar_map = build_avatar_map(pill_avatars);
            decorate_matrix_pills(&html, &avatar_map, pill_avatar_size)
        } else {
            html
        }
    } else {
        let html = plain_text_to_html(body);
        linkify_html(&html)
    }
}

#[cfg(test)]
mod tests;
