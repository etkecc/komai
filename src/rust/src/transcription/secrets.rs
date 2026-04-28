// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Persistence of transcription api keys via the existing secrets backend.
//!
//! The C++ side picks the backend (OS keychain by default; `secrets.yml`
//! file fallback when the profile is configured for `secrets.provider:
//! file`). From the Rust side it's a single key/value store per profile.
//!
//! Keys live at:
//!
//! - `config.integrations.transcription.api_key` for the global key.
//! - `config.integrations.transcription.by_room.<room_hash>.api_key` for
//!   per-room keys. `<room_hash>` is the first 16 hex chars of
//!   `SHA-256(room_id_utf8)` to avoid the `!`/`:`/`.` punctuation in
//!   Matrix room ids clashing with the dot-separated keychain key
//!   convention used elsewhere in the app.

use sha2::{Digest, Sha256};

use crate::settings::storage;

const GLOBAL_KEY_NAME: &str = "config.integrations.transcription.api_key";
const ROOM_KEY_PREFIX: &str = "config.integrations.transcription.by_room.";
const ROOM_KEY_SUFFIX: &str = ".api_key";

fn room_hash(room_id: &str) -> String {
    let mut hasher = Sha256::new();
    hasher.update(room_id.as_bytes());
    let digest = hasher.finalize();
    // 16 hex chars = 64 bits of collision resistance, plenty for naming
    // a per-room secret in a single user's keychain.
    let mut out = String::with_capacity(16);
    for byte in &digest[..8] {
        use std::fmt::Write as _;
        let _ = write!(out, "{byte:02x}");
    }
    out
}

fn room_key_name(room_id: &str) -> String {
    let mut name = String::with_capacity(ROOM_KEY_PREFIX.len() + 16 + ROOM_KEY_SUFFIX.len());
    name.push_str(ROOM_KEY_PREFIX);
    name.push_str(&room_hash(room_id));
    name.push_str(ROOM_KEY_SUFFIX);
    name
}

pub fn load_global_api_key(profile_id: &str) -> Option<String> {
    let key = storage::secure_store_key(profile_id, GLOBAL_KEY_NAME);
    storage::read_secure_value(&key).filter(|s| !s.is_empty())
}

pub fn save_global_api_key(profile_id: &str, value: Option<&str>) {
    let key = storage::secure_store_key(profile_id, GLOBAL_KEY_NAME);
    match value {
        Some(value) if !value.is_empty() => storage::write_secure_value(&key, value),
        _ => storage::delete_secure_value(&key),
    }
}

pub fn load_room_api_key(profile_id: &str, room_id: &str) -> Option<String> {
    let name = room_key_name(room_id);
    let key = storage::secure_store_key(profile_id, &name);
    storage::read_secure_value(&key).filter(|s| !s.is_empty())
}

pub fn save_room_api_key(profile_id: &str, room_id: &str, value: Option<&str>) {
    let name = room_key_name(room_id);
    let key = storage::secure_store_key(profile_id, &name);
    match value {
        Some(value) if !value.is_empty() => storage::write_secure_value(&key, value),
        _ => storage::delete_secure_value(&key),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn room_hash_is_deterministic() {
        let a = room_hash("!foo:matrix.org");
        let b = room_hash("!foo:matrix.org");
        assert_eq!(a, b);
        assert_eq!(a.len(), 16);
        assert!(a.chars().all(|c| c.is_ascii_hexdigit()));
    }

    #[test]
    fn different_rooms_get_different_hashes() {
        assert_ne!(
            room_hash("!foo:matrix.org"),
            room_hash("!bar:matrix.org"),
        );
    }

    #[test]
    fn room_key_name_round_trip_format() {
        let key = room_key_name("!foo:matrix.org");
        assert!(key.starts_with("config.integrations.transcription.by_room."));
        assert!(key.ends_with(".api_key"));
        // Total: prefix (43) + 16 hex + suffix (8)
        assert_eq!(key.len(), ROOM_KEY_PREFIX.len() + 16 + ROOM_KEY_SUFFIX.len());
    }
}
