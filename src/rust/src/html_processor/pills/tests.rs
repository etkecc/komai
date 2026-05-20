// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::HashMap;

use super::*;
use crate::ffi::HtmlPillAvatar;

fn count_occurrences(text: &str, needle: &str) -> usize {
    text.match_indices(needle).count()
}

fn make_pill_avatar(user_id: &str, mxc_url: &str) -> HtmlPillAvatar {
    HtmlPillAvatar {
        user_id: user_id.to_string(),
        mxc_url: mxc_url.to_string(),
        fallback_url: String::new(),
    }
}

fn make_pill_avatar_with_fallback(
    user_id: &str,
    mxc_url: &str,
    fallback_url: &str,
) -> HtmlPillAvatar {
    HtmlPillAvatar {
        user_id: user_id.to_string(),
        mxc_url: mxc_url.to_string(),
        fallback_url: fallback_url.to_string(),
    }
}

fn avatar_map_from<'a>(entries: &'a [HtmlPillAvatar]) -> HashMap<&'a str, &'a HtmlPillAvatar> {
    build_avatar_map(entries)
}

#[test]
fn pill_decorates_user_mention() {
    let html = r#"hello <a href="https://matrix.to/#/%40slavi%3Adevture.com">Slavi</a> world"#;
    let avatars = vec![make_pill_avatar(
        "@slavi:devture.com",
        "mxc://devture.com/abc123",
    )];
    let map = avatar_map_from(&avatars);
    let out = decorate_matrix_pills(html, &map, 32);
    assert!(out.contains(r#"class="pill pill-user""#), "user pill class");
    assert!(
        out.contains(r#"<img class="pill-avatar""#),
        "user pill has avatar img"
    );
    assert!(
        out.contains("image://mxcImage/devture.com/abc123?avatarSize=32&amp;radius=25"),
        "avatar img has correct src (& escaped as &amp; in HTML attribute)"
    );
    assert!(out.contains("Slavi"), "display name text is preserved");
    assert!(out.contains("hello "), "text before pill is preserved");
    assert!(out.contains(" world"), "text after pill is preserved");
}

#[test]
fn pill_decorates_room_mention() {
    let html =
        r#"<a href="https://matrix.to/#/%23room%3Aexample.org">#room:example.org</a>"#;
    let avatars = vec![make_pill_avatar(
        "#room:example.org",
        "mxc://example.org/roomavatar",
    )];
    let map = avatar_map_from(&avatars);
    let out = decorate_matrix_pills(html, &map, 32);
    assert!(
        out.contains(r#"class="pill pill-room""#),
        "room pill has pill-room class"
    );
    assert!(
        out.contains(r#"<img class="pill-avatar""#),
        "room pill has avatar img"
    );
    assert!(
        out.contains("#room:example.org"),
        "room name text is preserved"
    );
}

#[test]
fn pill_decorates_room_id_mention() {
    let html =
        r#"<a href="https://matrix.to/#/!abc123%3Aexample.org">My Room</a>"#;
    let avatars: Vec<HtmlPillAvatar> = Vec::new();
    let map = avatar_map_from(&avatars);
    let out = decorate_matrix_pills(html, &map, 32);
    assert!(
        out.contains(r#"class="pill pill-room""#),
        "room ID pill has pill-room class"
    );
    assert!(
        !out.contains(r#"<img class="pill-avatar""#),
        "no avatar img when not in map"
    );
    assert!(out.contains("My Room"), "display text is preserved");
}

#[test]
fn pill_skips_non_matrix_to_links() {
    let html = r#"<a href="https://example.org">Example</a>"#;
    let avatars = vec![make_pill_avatar("@any:server", "mxc://server/img")];
    let map = avatar_map_from(&avatars);
    let out = decorate_matrix_pills(html, &map, 32);
    assert!(!out.contains("pill"), "non-matrix.to link is not decorated");
    assert_eq!(out, html, "non-matrix.to link is unchanged");
}

#[test]
fn pill_preserves_multiple_links() {
    let html = concat!(
        r#"<a href="https://matrix.to/#/%40alice%3Aexample.org">Alice</a> and "#,
        r#"<a href="https://matrix.to/#/%40bob%3Aexample.org">Bob</a>"#
    );
    let avatars = vec![
        make_pill_avatar("@alice:example.org", "mxc://example.org/alice"),
        make_pill_avatar("@bob:example.org", "mxc://example.org/bob"),
    ];
    let map = avatar_map_from(&avatars);
    let out = decorate_matrix_pills(html, &map, 32);
    assert_eq!(
        count_occurrences(&out, r#"class="pill pill-user""#),
        2,
        "both user links are decorated"
    );
    assert!(out.contains("example.org/alice"), "first avatar is present");
    assert!(out.contains("example.org/bob"), "second avatar is present");
    assert!(out.contains("Alice"), "first display name");
    assert!(out.contains(" and "), "text between pills");
    assert!(out.contains("Bob"), "second display name");
}

#[test]
fn pill_with_empty_avatar_map() {
    let html =
        r#"<a href="https://matrix.to/#/%40user%3Aexample.org">User</a>"#;
    let avatars: Vec<HtmlPillAvatar> = Vec::new();
    let map = avatar_map_from(&avatars);
    let out = decorate_matrix_pills(html, &map, 32);
    assert!(
        out.contains(r#"class="pill pill-user""#),
        "pill class is added even without avatars"
    );
    assert!(!out.contains("<img"), "no img tag without avatars");
}

#[test]
fn pill_with_event_link() {
    let html =
        r#"<a href="https://matrix.to/#/!room%3Aserver/%24event%3Aserver">link</a>"#;
    let avatars = vec![make_pill_avatar("!room:server", "mxc://server/roomavatar")];
    let map = avatar_map_from(&avatars);
    let out = decorate_matrix_pills(html, &map, 32);
    assert!(
        out.contains(r#"class="pill pill-room""#),
        "event link is decorated as room pill"
    );
    assert!(
        out.contains("image://mxcImage/server/roomavatar"),
        "room avatar is resolved from room ID portion"
    );
}

#[test]
fn pill_uses_fallback_when_user_has_no_mxc_avatar() {
    let html =
        r#"<a href="https://matrix.to/#/%40user%3Aexample.org">User</a>"#;
    let avatars = vec![make_pill_avatar_with_fallback(
        "@user:example.org",
        "",
        "image://default-avatar/@user:example.org?radius=25&displayName=User&color=ab12cd&style=4&_v=4",
    )];
    let map = avatar_map_from(&avatars);
    let out = decorate_matrix_pills(html, &map, 32);
    assert!(
        out.contains(r#"<img class="pill-avatar""#),
        "fallback img is injected when no mxc URL is available"
    );
    assert!(
        out.contains("image://default-avatar/@user:example.org"),
        "default-avatar URL is emitted as the pill avatar source"
    );
    assert!(
        out.contains("avatarSize=32"),
        "avatarSize is appended for the default-avatar provider"
    );
}

#[test]
fn pill_prefers_mxc_over_fallback_when_both_present() {
    let html =
        r#"<a href="https://matrix.to/#/%40user%3Aexample.org">User</a>"#;
    let avatars = vec![make_pill_avatar_with_fallback(
        "@user:example.org",
        "mxc://example.org/abc",
        "image://default-avatar/@user:example.org?radius=25",
    )];
    let map = avatar_map_from(&avatars);
    let out = decorate_matrix_pills(html, &map, 32);
    assert!(
        out.contains("image://mxcImage/example.org/abc"),
        "real mxc avatar is the primary src"
    );
}

#[test]
fn pill_with_mxc_carries_percent_encoded_fallback_for_litehtml() {
    let html =
        r#"<a href="https://matrix.to/#/%40user%3Aexample.org">User</a>"#;
    let avatars = vec![make_pill_avatar_with_fallback(
        "@user:example.org",
        "mxc://example.org/abc",
        "image://default-avatar/@user:example.org?radius=25&color=ab12cd",
    )];
    let map = avatar_map_from(&avatars);
    let out = decorate_matrix_pills(html, &map, 32);
    // The outer mxc URL is the primary src. The fallback is tucked into
    // its query so LitehtmlContainer can pre-cache the default avatar
    // under the mxc URL key — `&` (HTML-escaped to `&amp;`), `=`, `%`,
    // `:` and `/` inside the inner URL all need percent-encoding so they
    // don't break out of the `fallback=` query value.
    assert!(
        out.contains("image://mxcImage/example.org/abc"),
        "mxc URL is the primary src"
    );
    assert!(
        out.contains("fallback=image%3A%2F%2Fdefault-avatar%2F"),
        "fallback URL is percent-encoded inside the mxc query"
    );
    assert!(
        out.contains("%26color%3Dab12cd"),
        "fallback URL's own `&color=...` is encoded so it doesn't bleed into the outer query"
    );
}
