// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Room classification heuristics: detect bot-only rooms, pick the
//! "hero" member for direct chats, and decide if a room is direct,
//! group, public, or otherwise.

use super::*;

pub(super) fn ci_contains(haystack: &str, needle: &str) -> bool {
    haystack.to_ascii_lowercase().contains(&needle.to_ascii_lowercase())
}

pub(super) fn ci_starts_with(s: &str, prefix: &str) -> bool {
    s.get(..prefix.len())
        .is_some_and(|candidate| candidate.eq_ignore_ascii_case(prefix))
}

pub(super) fn ci_ends_with(s: &str, suffix: &str) -> bool {
    if suffix.len() > s.len() {
        return false;
    }

    s.get(s.len() - suffix.len()..)
        .is_some_and(|candidate| candidate.eq_ignore_ascii_case(suffix))
}

pub(super) fn is_bot(user_id: &str, display_name: &str, service_members: &HashSet<String>) -> bool {
    service_members.contains(user_id) || is_likely_bot_user(user_id, display_name)
}

pub(super) fn is_likely_bot_user(user_id: &str, display_name: &str) -> bool {
    if ci_starts_with(user_id, "@bot") {
        return true;
    }

    if ci_contains(user_id, "bot:") {
        return true;
    }

    let localpart = user_id
        .split_once(':')
        .map(|(localpart, _)| localpart)
        .unwrap_or(user_id);
    if ci_contains(localpart, "puppet") {
        return false;
    }

    if ci_starts_with(user_id, "@_") {
        return true;
    }

    if ci_ends_with(localpart, "bridge") {
        return true;
    }

    if ci_contains(display_name, "bridge bot") {
        return true;
    }

    if ci_ends_with(display_name, "bot")
        && display_name
            .chars()
            .rev()
            .nth(3)
            .is_none_or(|c| !c.is_ascii_alphabetic())
    {
        return true;
    }

    if ci_starts_with(display_name, "bot")
        && display_name
            .chars()
            .nth(3)
            .is_none_or(|c| !c.is_ascii_alphabetic())
    {
        return true;
    }

    false
}

#[derive(Clone, Debug)]
pub(super) struct RoomHeroCandidate {
    pub(super) user_id: String,
    pub(super) display_name: String,
    pub(super) avatar_url: String,
}

pub(super) fn room_hero_candidates(room: &RoomListItem) -> Vec<RoomHeroCandidate> {
    let own_user_id = room.own_user_id();
    let mut candidates: Vec<RoomHeroCandidate> = room
        .heroes()
        .into_iter()
        .filter(|hero| hero.user_id != own_user_id)
        .map(|hero| RoomHeroCandidate {
            user_id: hero.user_id.to_string(),
            display_name: hero.display_name.unwrap_or_default(),
            avatar_url: hero
                .avatar_url
                .map(|url| normalize_mxc_uri(url.to_string()))
                .unwrap_or_default(),
        })
        .collect();

    candidates.sort_by(|left, right| left.user_id.cmp(&right.user_id));
    candidates.dedup_by(|left, right| left.user_id == right.user_id);
    candidates
}

pub(super) fn classify_room(
    room: &RoomListItem,
    hero_candidates: &[RoomHeroCandidate],
) -> MatrixRoomClassification {
    // MSC4171: rooms can publish a `m.member_hints` state event listing
    // user IDs the room considers "service members" (typically bridge bots
    // that mautrix-* bridges, hookshot, etc. set on themselves). Treat any
    // listed user as a bot regardless of textual heuristics — server-declared
    // truth wins over name guesses.
    let service_members: HashSet<String> = room
        .service_members()
        .map(|set| set.into_iter().map(|user_id| user_id.to_string()).collect())
        .unwrap_or_default();

    let mut direct_targets: Vec<String> = room
        .direct_targets()
        .into_iter()
        .filter_map(|target| target.into_user_id())
        .map(|user_id| user_id.to_string())
        .collect();
    direct_targets.sort();
    direct_targets.dedup();

    if let Some(partner_user_id) = direct_targets.first().cloned() {
        let partner_display_name = hero_candidates
            .iter()
            .find(|candidate| candidate.user_id == partner_user_id)
            .map(|candidate| candidate.display_name.as_str())
            .unwrap_or_default();

        return MatrixRoomClassification {
            is_direct: true,
            is_bot_room: is_bot(&partner_user_id, partner_display_name, &service_members),
            direct_chat_other_user_id: partner_user_id,
        };
    }

    match room.active_members_count() {
        2 => {
            if let Some(partner) = hero_candidates.first() {
                MatrixRoomClassification {
                    is_direct: true,
                    is_bot_room: is_bot(
                        &partner.user_id,
                        &partner.display_name,
                        &service_members,
                    ),
                    direct_chat_other_user_id: partner.user_id.clone(),
                }
            } else {
                MatrixRoomClassification {
                    is_direct: false,
                    is_bot_room: false,
                    direct_chat_other_user_id: String::new(),
                }
            }
        }
        3 => {
            if hero_candidates.len() < 2 {
                return MatrixRoomClassification {
                    is_direct: false,
                    is_bot_room: false,
                    direct_chat_other_user_id: String::new(),
                };
            }

            let first = &hero_candidates[0];
            let second = &hero_candidates[1];
            let first_is_bot = is_bot(&first.user_id, &first.display_name, &service_members);
            let second_is_bot =
                is_bot(&second.user_id, &second.display_name, &service_members);

            if first_is_bot && !second_is_bot {
                MatrixRoomClassification {
                    is_direct: true,
                    is_bot_room: false,
                    direct_chat_other_user_id: second.user_id.clone(),
                }
            } else if second_is_bot && !first_is_bot {
                MatrixRoomClassification {
                    is_direct: true,
                    is_bot_room: false,
                    direct_chat_other_user_id: first.user_id.clone(),
                }
            } else if first_is_bot && second_is_bot {
                MatrixRoomClassification {
                    is_direct: true,
                    is_bot_room: true,
                    direct_chat_other_user_id: first.user_id.clone(),
                }
            } else {
                MatrixRoomClassification {
                    is_direct: false,
                    is_bot_room: false,
                    direct_chat_other_user_id: String::new(),
                }
            }
        }
        _ => MatrixRoomClassification {
            is_direct: false,
            is_bot_room: false,
            direct_chat_other_user_id: String::new(),
        },
    }
}
