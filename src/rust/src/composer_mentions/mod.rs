// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Composer-side helper that extracts intentional *user* mentions from draft
//! text. The QML composer calls this when it cannot attribute a pill to a
//! completer pick (edit/draft restore, paste): given the draft text, it
//! returns the MXID of every matrix.to / matrix: user link, alongside the
//! exact link substring so the composer can drop the mention if that text is
//! later edited away.
//!
//! Parsing and validation are delegated to ruma's [`MatrixToUri`] /
//! [`MatrixUri`], so only well-formed *user* links are recognized. `@room` is
//! intentionally not handled here: it is a keyword, not a link, and the
//! composer detects it directly. Non-sigil matrix.to (MSC4481) is rejected by
//! ruma today, which is the safe behavior — a bare `user:server` is ambiguous
//! with a room alias.

use crate::ffi::ComposerMentionMatch;
use matrix_sdk::ruma::{MatrixToUri, MatrixUri, matrix_uri::MatrixId};

const MATRIX_TO_MARKER: &str = "matrix.to/#/";

/// A user mention recovered from draft text.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MentionMatch {
    /// The mentioned user's MXID (e.g. `@alice:example.org`).
    pub user_id: String,
    /// The link substring as it appears in the text, used by the composer to
    /// prune the mention when the user edits that text away.
    pub source: String,
}

/// Extract user mentions from arbitrary draft text. Order-preserving and
/// deduplicated by MXID (the first occurrence's link is kept as the source).
pub fn extract_mentions(text: &str) -> Vec<MentionMatch> {
    let mut out: Vec<MentionMatch> = Vec::new();

    for token in tokenize(text) {
        if let Some(found) = mention_from_token(token) {
            if !out.iter().any(|existing| existing.user_id == found.user_id) {
                out.push(found);
            }
        }
    }
    out
}

/// Split text into maximal runs of non-delimiter characters. Delimiters cover
/// whitespace plus the bracketing / quoting characters that bound a URL inside
/// markdown (`[name](url)`), HTML, or prose, so an embedded link becomes its
/// own token.
fn tokenize(text: &str) -> impl Iterator<Item = &str> {
    text.split(|c: char| {
        c.is_whitespace()
            || matches!(
                c,
                '(' | ')' | '[' | ']' | '<' | '>' | '{' | '}' | '"' | '\'' | '`' | '|' | '\\'
            )
    })
    .filter(|token| !token.is_empty())
}

fn mention_from_token(token: &str) -> Option<MentionMatch> {
    // Trim trailing prose punctuation so a link at the end of a sentence still
    // parses. Matrix ids never end in these, so this cannot truncate a valid id.
    let candidate = token.trim_end_matches(|c| matches!(c, '.' | ',' | ';' | '!' | '?'));
    if candidate.is_empty() {
        return None;
    }

    if let Some(pos) = candidate.find(MATRIX_TO_MARKER) {
        let id_and_query = &candidate[pos + MATRIX_TO_MARKER.len()..];
        let normalized = format!("https://{MATRIX_TO_MARKER}{id_and_query}");
        let uri = MatrixToUri::parse(&normalized).ok()?;
        return user_id_from(uri.id()).map(|user_id| MentionMatch {
            user_id,
            source: candidate[pos..].to_owned(),
        });
    }

    if candidate.starts_with("matrix:") {
        let uri = MatrixUri::parse(candidate).ok()?;
        return user_id_from(uri.id()).map(|user_id| MentionMatch {
            user_id,
            source: candidate.to_owned(),
        });
    }

    None
}

fn user_id_from(id: &MatrixId) -> Option<String> {
    match id {
        MatrixId::User(user_id) => Some(user_id.to_string()),
        _ => None,
    }
}

/// cxx entry point: map the native matches into the bridge struct.
pub(crate) fn composer_extract_mentions(text: &str) -> Vec<ComposerMentionMatch> {
    extract_mentions(text)
        .into_iter()
        .map(|found| ComposerMentionMatch {
            user_id: found.user_id,
            source: found.source,
        })
        .collect()
}

#[cfg(test)]
mod tests;
