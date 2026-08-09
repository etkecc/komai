// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Local E2EE room-key export/import: the passphrase-encrypted
//! `MEGOLM SESSION DATA` file format shared with Element and Nheko.

use std::path::PathBuf;

use super::*;

pub struct RoomKeyImportCounts {
    pub imported: u64,
    pub total: u64,
}

/// Export every room key this session holds to `path`, encrypted with
/// `passphrase`. Returns the number of exported keys.
pub async fn export_room_keys(handle_id: u64, path: &str, passphrase: &str) -> Result<u64, String> {
    let client = client_for_handle(handle_id)?;
    client.encryption().wait_for_e2ee_initialization_tasks().await;

    let mut count: u64 = 0;
    client
        .encryption()
        .export_room_keys(PathBuf::from(path), passphrase, |_| {
            count += 1;
            true
        })
        .await
        .map_err(|e| format!("failed to export room keys: {e}"))?;

    Ok(count)
}

/// Import room keys from the file at `path`, decrypting it with `passphrase`.
/// Returns how many keys were imported alongside how many the file contains
/// (already-known keys are skipped by the SDK).
pub async fn import_room_keys(
    handle_id: u64,
    path: &str,
    passphrase: &str,
) -> Result<RoomKeyImportCounts, String> {
    let client = client_for_handle(handle_id)?;
    client.encryption().wait_for_e2ee_initialization_tasks().await;

    let result = client
        .encryption()
        .import_room_keys(PathBuf::from(path), passphrase)
        .await
        .map_err(|e| format!("failed to import room keys: {e}"))?;

    Ok(RoomKeyImportCounts {
        imported: result.imported_count as u64,
        total: result.total_count as u64,
    })
}
