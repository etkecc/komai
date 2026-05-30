// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

/// Collect just the MXIDs, in order, for terse assertions.
fn ids(text: &str) -> Vec<String> {
    extract_mentions(text).into_iter().map(|m| m.user_id).collect()
}

#[test]
fn empty_and_plain_text_yield_nothing() {
    assert!(ids("").is_empty());
    assert!(ids("hello world, no links here").is_empty());
    assert!(ids("an email looks@like.this but is not a link").is_empty());
}

#[test]
fn markdown_pill_is_extracted() {
    assert_eq!(
        ids("hi [Alice](https://matrix.to/#/@alice:example.org)!"),
        vec!["@alice:example.org"]
    );
}

#[test]
fn bare_matrix_to_url_is_extracted() {
    assert_eq!(
        ids("see https://matrix.to/#/@bob:example.org for details"),
        vec!["@bob:example.org"]
    );
}

#[test]
fn percent_encoded_id_is_decoded_by_ruma() {
    assert_eq!(
        ids("[x](https://matrix.to/#/%40carol%3Aexample.org)"),
        vec!["@carol:example.org"]
    );
}

#[test]
fn via_query_parameters_are_ignored() {
    assert_eq!(
        ids("https://matrix.to/#/@dave:example.org?via=other.org"),
        vec!["@dave:example.org"]
    );
}

#[test]
fn matrix_uri_scheme_is_extracted() {
    assert_eq!(
        ids("ping matrix:u/erin:example.org now"),
        vec!["@erin:example.org"]
    );
}

#[test]
fn matrix_uri_with_action_query_is_extracted() {
    assert_eq!(
        ids("matrix:u/frank:example.org?action=chat"),
        vec!["@frank:example.org"]
    );
}

#[test]
fn trailing_sentence_punctuation_is_tolerated() {
    assert_eq!(
        ids("talk to https://matrix.to/#/@grace:example.org."),
        vec!["@grace:example.org"]
    );
    assert_eq!(
        ids("(see matrix:u/heidi:example.org),"),
        vec!["@heidi:example.org"]
    );
}

#[test]
fn room_and_alias_links_are_not_user_mentions() {
    assert!(ids("https://matrix.to/#/!someroom:example.org").is_empty());
    assert!(ids("https://matrix.to/#/%23general:example.org").is_empty());
    assert!(ids("matrix:r/general:example.org").is_empty());
    assert!(ids("matrix:roomid/someroom:example.org").is_empty());
}

#[test]
fn event_links_are_not_user_mentions() {
    assert!(
        ids("https://matrix.to/#/!someroom:example.org/$event:example.org").is_empty()
    );
}

#[test]
fn non_sigil_matrix_to_is_rejected() {
    // MSC4481 form: ambiguous between a user and a room alias, so ruma rejects
    // it and we surface nothing rather than guessing.
    assert!(ids("https://matrix.to/#/alice:example.org").is_empty());
}

#[test]
fn multiple_mentions_are_deduplicated_in_order() {
    let text = "[A](https://matrix.to/#/@a:example.org) and \
                matrix:u/b:example.org and again \
                [A2](https://matrix.to/#/@a:example.org)";
    assert_eq!(ids(text), vec!["@a:example.org", "@b:example.org"]);
}

#[test]
fn source_substring_is_present_in_the_original_text() {
    // The composer prunes a mention by looking for its `source` in the text, so
    // every reported source must be a literal substring of the input.
    let text = "hello [Alice](https://matrix.to/#/@alice:example.org) and \
                matrix:u/bob:example.org!";
    for found in extract_mentions(text) {
        assert!(
            text.contains(&found.source),
            "source {:?} is not a substring of the input",
            found.source
        );
    }
}

#[test]
fn invalid_user_ids_are_dropped() {
    // A sigil with no domain is not a valid MXID.
    assert!(ids("https://matrix.to/#/@nodomain").is_empty());
}
