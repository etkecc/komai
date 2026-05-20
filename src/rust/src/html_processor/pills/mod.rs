// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Matrix `matrix.to` pill decoration (user/room avatars on mentions).

use std::collections::HashMap;

use crate::ffi::HtmlPillAvatar;

use super::parser::{parse_attributes, parse_tag, tag_name_lower};
use super::util::{html_escape, percent_decode, percent_encode_query_value};

pub(super) const MATRIX_TO_PREFIX: &str = "https://matrix.to/#/";

/// Extract the Matrix ID from a `matrix.to` href value.
fn matrix_id_from_href(href: &str) -> String {
    let fragment = match href.strip_prefix(MATRIX_TO_PREFIX) {
        Some(f) => f,
        None => return String::new(),
    };

    // Strip query string.
    let fragment = match fragment.find('?') {
        Some(pos) => &fragment[..pos],
        None => fragment,
    };

    // For event links like "!room:server/$event:server", keep only the first segment.
    let fragment = match fragment.find('/') {
        Some(pos) => &fragment[..pos],
        None => fragment,
    };

    percent_decode(fragment)
}

/// Determine the pill CSS class suffix from a Matrix ID sigil.
fn pill_class_for_id(matrix_id: &str) -> &'static str {
    match matrix_id.as_bytes().first() {
        Some(b'@') => "user",
        Some(b'#') | Some(b'!') => "room",
        _ => "",
    }
}

/// Convert `mxc://server/media_id` to `image://mxcImage/server/media_id?avatarSize=N&radius=25`.
/// When `fallback_url` is non-empty, append `&fallback=<percent-encoded>` so
/// LitehtmlContainer can pre-cache the default avatar under the mxc URL key
/// (and keep showing it if the mxc fetch fails). The fallback string is the
/// fully-formed `image://default-avatar/...` URL prepared in C++.
fn mxc_to_pill_avatar_url(mxc_url: &str, avatar_size: u32, fallback_url: &str) -> String {
    let rest = match mxc_url.strip_prefix("mxc://") {
        Some(rest) => rest,
        None => return String::new(),
    };
    let mut url = format!("image://mxcImage/{rest}?avatarSize={avatar_size}&radius=25");
    if !fallback_url.is_empty() {
        let sep = if fallback_url.contains('?') { '&' } else { '?' };
        let full_fallback = format!("{fallback_url}{sep}avatarSize={avatar_size}");
        url.push_str("&fallback=");
        url.push_str(&percent_encode_query_value(&full_fallback));
    }
    url
}

/// Append the requested logical avatar size to a pre-formed
/// `image://default-avatar/...` URL produced by the C++ side.
fn fallback_to_pill_avatar_url(fallback_url: &str, avatar_size: u32) -> String {
    if fallback_url.is_empty() {
        return String::new();
    }
    let sep = if fallback_url.contains('?') { '&' } else { '?' };
    format!("{fallback_url}{sep}avatarSize={avatar_size}")
}

/// Pick the best avatar source for a pill: a real mxc URL when the user
/// currently has an avatar, otherwise the default-avatar fallback URL the
/// C++ side prepared for them. Returns `None` when neither is available
/// (e.g. for a non-sender mention we have no profile snapshot for).
///
/// When both are available, the mxc URL carries the fallback piggybacked as
/// a percent-encoded `&fallback=` query so LitehtmlContainer can render the
/// default avatar while the mxc download is in flight (and keep it on
/// failure), mirroring Avatar.qml's behaviour in the timeline body.
fn pill_avatar_src(entry: &HtmlPillAvatar, avatar_size: u32) -> Option<String> {
    if entry.mxc_url.starts_with("mxc://") {
        Some(mxc_to_pill_avatar_url(
            &entry.mxc_url,
            avatar_size,
            &entry.fallback_url,
        ))
    } else if !entry.fallback_url.is_empty() {
        Some(fallback_to_pill_avatar_url(&entry.fallback_url, avatar_size))
    } else {
        None
    }
}

pub(super) fn build_avatar_map<'a>(
    avatars: &'a [HtmlPillAvatar],
) -> HashMap<&'a str, &'a HtmlPillAvatar> {
    let mut map = HashMap::with_capacity(avatars.len());
    for a in avatars {
        if a.user_id.is_empty() {
            continue;
        }
        if a.mxc_url.is_empty() && a.fallback_url.is_empty() {
            continue;
        }
        map.insert(a.user_id.as_str(), a);
    }
    map
}

pub(super) fn decorate_matrix_pills(
    html: &str,
    avatar_map: &HashMap<&str, &HtmlPillAvatar>,
    avatar_size: u32,
) -> String {
    if html.is_empty() {
        return html.to_string();
    }

    let mut out = String::with_capacity(html.len() + html.len() / 4);
    let bytes = html.as_bytes();
    let mut pos = 0;

    while pos < bytes.len() {
        let next_lt = match html[pos..].find('<') {
            Some(idx) => pos + idx,
            None => {
                out.push_str(&html[pos..]);
                break;
            }
        };

        out.push_str(&html[pos..next_lt]);

        let tag = parse_tag(html, next_lt);
        if !tag.valid {
            out.push('<');
            pos = next_lt + 1;
            continue;
        }

        let tag_name = tag_name_lower(html, &tag);

        // Only process opening <a> tags with matrix.to hrefs.
        if tag_name != "a" || tag.is_end || tag.self_closing {
            out.push_str(&html[tag.start..tag.end]);
            pos = tag.end;
            continue;
        }

        let attrs = parse_attributes(html, &tag);
        let href = attrs
            .iter()
            .find(|a| a.name == "href")
            .map(|a| a.value.as_str())
            .unwrap_or("");

        let matrix_id = matrix_id_from_href(href);
        let pill_type = pill_class_for_id(&matrix_id);

        if pill_type.is_empty() {
            // Not a pill-eligible link — emit as-is.
            out.push_str(&html[tag.start..tag.end]);
            pos = tag.end;
            continue;
        }

        // Rebuild the <a> tag with pill class.
        out.push_str("<a href=\"");
        out.push_str(&html_escape(href));
        out.push_str("\" class=\"pill pill-");
        out.push_str(pill_type);
        out.push_str("\">");

        // Inject an avatar image — real mxc when available, otherwise the
        // default-avatar fallback URL prepared on the C++ side. We avoid
        // emitting a bare pill (text only, no `<img>`) here because the
        // pill-avatar CSS reserves a square slot, and the user expects
        // parity with the timeline avatar where the fallback always renders.
        if let Some(entry) = avatar_map.get(matrix_id.as_str()) {
            if let Some(avatar_src) = pill_avatar_src(entry, avatar_size) {
                out.push_str("<img class=\"pill-avatar\" src=\"");
                out.push_str(&html_escape(&avatar_src));
                out.push_str("\"/>");
            }
        }

        pos = tag.end;
    }

    out
}

#[cfg(test)]
mod tests;
