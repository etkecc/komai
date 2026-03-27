// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::sync::{Arc, Mutex};

use matrix_sdk::{
    encryption::verification::{CancelInfo, SasState, VerificationRequestState},
    event_handler::EventHandlerDropGuard,
    ruma::events::key::verification::{VerificationMethod, cancel::CancelCode},
    ruma::{UserId, events::AnyToDeviceEvent, serde::Raw},
};
use serde_json::Value;

use super::*;

fn cancel_code_name(cancel_info: &CancelInfo) -> String {
    match cancel_info.cancel_code() {
        CancelCode::MismatchedCommitment => "MismatchedCommitment",
        CancelCode::MismatchedSas => "MismatchedSAS",
        CancelCode::KeyMismatch => "KeyMismatch",
        CancelCode::Timeout => "Timeout",
        CancelCode::User => "User",
        CancelCode::Accepted => "AcceptedOnOtherDevice",
        CancelCode::UnexpectedMessage => "OutOfOrder",
        CancelCode::UnknownMethod => "UnknownMethod",
        _ => "UnknownMethod",
    }
    .to_owned()
}

fn request_snapshot(
    flow_id: &str,
    request: &matrix_sdk::encryption::verification::VerificationRequest,
    error: String,
) -> MatrixVerificationSession {
    let mut device_id = String::new();
    let state = match request.state() {
        VerificationRequestState::Created { .. } => "PromptStartVerification",
        VerificationRequestState::Requested {
            ref other_device_data,
            ..
        } => {
            device_id = other_device_data.device_id().as_str().to_owned();
            "PromptStartVerification"
        }
        VerificationRequestState::Ready {
            ref other_device_data,
            ..
        } => {
            device_id = other_device_data.device_id().as_str().to_owned();
            "PromptStartVerification"
        }
        VerificationRequestState::Transitioned { .. } => "WaitingForKeys",
        VerificationRequestState::Done => "Success",
        VerificationRequestState::Cancelled(_) => "Failed",
    };

    MatrixVerificationSession {
        flow_id: flow_id.to_owned(),
        user_id: request.other_user_id().to_string(),
        device_id: device_id.clone(),
        state: state.to_owned(),
        error,
        sender: request.we_started(),
        is_self_verification: request.is_self_verification(),
        is_multi_device_verification: request.is_self_verification() && device_id.is_empty(),
        sas_numbers: Vec::new(),
    }
}

fn sas_snapshot(
    flow_id: &str,
    _request: &matrix_sdk::encryption::verification::VerificationRequest,
    sas: &matrix_sdk::encryption::verification::SasVerification,
) -> MatrixVerificationSession {
    let (state, error, sas_numbers) = match sas.state() {
        SasState::Created { .. } => (
            if sas.we_started() {
                "WaitingForOtherToAccept"
            } else {
                "WaitingForKeys"
            }
            .to_owned(),
            String::new(),
            Vec::new(),
        ),
        SasState::Started { .. } | SasState::Accepted { .. } => {
            ("WaitingForKeys".to_owned(), String::new(), Vec::new())
        }
        SasState::KeysExchanged {
            ref emojis,
            decimals,
        } => {
            if let Some(emojis) = emojis {
                (
                    "CompareEmoji".to_owned(),
                    String::new(),
                    emojis.indices.iter().map(|index| u16::from(*index)).collect(),
                )
            } else {
                (
                    "CompareNumber".to_owned(),
                    String::new(),
                    [decimals.0, decimals.1, decimals.2].to_vec(),
                )
            }
        }
        SasState::Confirmed => ("WaitingForMac".to_owned(), String::new(), Vec::new()),
        SasState::Done { .. } => ("Success".to_owned(), String::new(), Vec::new()),
        SasState::Cancelled(ref cancel_info) => {
            ("Failed".to_owned(), cancel_code_name(cancel_info), Vec::new())
        }
    };

    MatrixVerificationSession {
        flow_id: flow_id.to_owned(),
        user_id: sas.other_user_id().to_string(),
        device_id: sas.other_device().device_id().as_str().to_owned(),
        state,
        error,
        sender: sas.we_started(),
        is_self_verification: sas.is_self_verification(),
        is_multi_device_verification: sas.is_self_verification()
            && sas.other_device().device_id().as_str().is_empty(),
        sas_numbers,
    }
}

fn snapshot_from_entry(
    flow_id: &str,
    entry: &mut MatrixVerificationSessionEntry,
) -> MatrixVerificationSession {
    if entry.sas.is_none() {
        if let VerificationRequestState::Transitioned { verification } = entry.request.state() {
            if let Some(sas) = verification.sas() {
                entry.sas = Some(sas);
            }
        }
    }

    if let Some(sas) = &entry.sas {
        sas_snapshot(flow_id, &entry.request, sas)
    } else {
        let error = match entry.request.state() {
            VerificationRequestState::Cancelled(ref cancel_info) => cancel_code_name(cancel_info),
            _ => String::new(),
        };
        request_snapshot(flow_id, &entry.request, error)
    }
}

fn extract_verification_ids_from_raw_to_device(
    raw: &Raw<AnyToDeviceEvent>,
) -> Option<(String, String)> {
    let value = serde_json::from_str::<Value>(raw.json().get()).ok()?;
    let event_type = value.get("type")?.as_str()?;
    if !matches!(
        event_type,
        "m.key.verification.request" | "m.key.verification.ready" | "m.key.verification.start"
    ) {
        return None;
    }

    let sender = value.get("sender")?.as_str()?.trim();
    let flow_id = value
        .get("content")?
        .get("transaction_id")?
        .as_str()?
        .trim();
    if sender.is_empty() || flow_id.is_empty() {
        return None;
    }

    Some((sender.to_owned(), flow_id.to_owned()))
}

async fn register_incoming_verification_session(
    client: matrix_sdk::Client,
    verification_sessions: Arc<Mutex<HashMap<String, MatrixVerificationSessionEntry>>>,
    pending_flow_ids: Arc<Mutex<Vec<String>>>,
    sender: &str,
    flow_id: &str,
) -> Result<(), String> {
    let user_id = UserId::parse(sender)
        .map_err(|e| format!("invalid verification sender '{sender}': {e}"))?;

    let Some(request) = client
        .encryption()
        .get_verification_request(&user_id, flow_id)
        .await
    else {
        return Ok(());
    };

    if request.we_started() {
        return Ok(());
    }

    let mut entry = MatrixVerificationSessionEntry { request, sas: None };
    let snapshot = snapshot_from_entry(flow_id, &mut entry);
    if matches!(snapshot.state.as_str(), "Success" | "Failed") {
        return Ok(());
    }

    {
        let mut sessions = verification_sessions
            .lock()
            .expect("poisoned matrix backend verification sessions mutex");
        if sessions.contains_key(flow_id) {
            return Ok(());
        }
        sessions.insert(flow_id.to_owned(), entry);
    }

    let mut pending = pending_flow_ids
        .lock()
        .expect("poisoned matrix backend pending verification flows mutex");
    if !pending.iter().any(|pending_flow_id| pending_flow_id == flow_id) {
        pending.push(flow_id.to_owned());
    }

    Ok(())
}

pub(crate) fn install_incoming_verification_event_handlers(
    handle_id: u64,
    client: matrix_sdk::Client,
    verification_sessions: Arc<Mutex<HashMap<String, MatrixVerificationSessionEntry>>>,
    pending_flow_ids: Arc<Mutex<Vec<String>>>,
) -> Vec<EventHandlerDropGuard> {
    let handle = client.add_event_handler({
        let verification_sessions = Arc::clone(&verification_sessions);
        let pending_flow_ids = Arc::clone(&pending_flow_ids);
        move |raw: Raw<AnyToDeviceEvent>, client: matrix_sdk::Client| {
            let verification_sessions = Arc::clone(&verification_sessions);
            let pending_flow_ids = Arc::clone(&pending_flow_ids);
            async move {
                let Some((sender, flow_id)) = extract_verification_ids_from_raw_to_device(&raw)
                else {
                    return;
                };

                if let Err(error) = register_incoming_verification_session(
                    client,
                    verification_sessions,
                    pending_flow_ids,
                    &sender,
                    &flow_id,
                )
                .await
                {
                    tracing::warn!(
                        handle_id,
                        sender,
                        flow_id,
                        "Failed to register incoming matrix-sdk verification flow: {error}"
                    );
                }
            }
        }
    });

    vec![client.event_handler_drop_guard(handle)]
}

pub fn take_pending_verification_flow_ids(handle_id: u64) -> Result<Vec<String>, String> {
    let pending_flow_ids = pending_verification_flow_ids_for_handle(handle_id)?;
    let mut pending_flow_ids = pending_flow_ids
        .lock()
        .expect("poisoned matrix backend pending verification flows mutex");
    Ok(std::mem::take(&mut *pending_flow_ids))
}

pub async fn start_self_verification(handle_id: u64) -> Result<MatrixVerificationSession, String> {
    let client = client_for_handle(handle_id)?;
    client.encryption().wait_for_e2ee_initialization_tasks().await;

    if !client
        .encryption()
        .has_devices_to_verify_against()
        .await
        .map_err(|e| format!("failed to inspect available verification devices: {e}"))?
    {
        return Err("No other signed-in verifiable device is currently available.".to_owned());
    }

    let user_id = client
        .user_id()
        .ok_or_else(|| "matrix-sdk client has no authenticated user id".to_owned())?
        .to_owned();

    let identity = client
        .encryption()
        .get_user_identity(&user_id)
        .await
        .map_err(|e| format!("failed to fetch own encryption identity: {e}"))?
        .ok_or_else(|| "This account does not currently expose a cross-signing identity.".to_owned())?;

    let request = identity
        .request_verification_with_methods(vec![VerificationMethod::SasV1])
        .await
        .map_err(|e| format!("failed to request self-verification: {e}"))?;

    let flow_id = request.flow_id().to_owned();
    let mut entry = MatrixVerificationSessionEntry { request, sas: None };
    let snapshot = snapshot_from_entry(&flow_id, &mut entry);

    verification_sessions_for_handle(handle_id)?
        .lock()
        .expect("poisoned matrix backend verification sessions mutex")
        .insert(flow_id, entry);

    Ok(snapshot)
}

pub async fn fetch_verification_session(
    handle_id: u64,
    flow_id: &str,
) -> Result<MatrixVerificationSession, String> {
    let sessions = verification_sessions_for_handle(handle_id)?;
    let mut sessions = sessions
        .lock()
        .expect("poisoned matrix backend verification sessions mutex");
    let entry = sessions.get_mut(flow_id).ok_or_else(|| {
        format!("matrix-sdk backend runtime handle {handle_id} has no verification flow '{flow_id}'")
    })?;
    Ok(snapshot_from_entry(flow_id, entry))
}

pub async fn advance_verification_session(handle_id: u64, flow_id: &str) -> Result<(), String> {
    let sessions = verification_sessions_for_handle(handle_id)?;
    let mut entry = {
        let mut sessions = sessions
            .lock()
            .expect("poisoned matrix backend verification sessions mutex");
        sessions.remove(flow_id).ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no verification flow '{flow_id}'"
            )
        })?
    };

    if let Some(sas) = &entry.sas {
        match sas.state() {
            SasState::Started { .. } => {
                sas.accept()
                    .await
                    .map_err(|e| format!("failed to accept SAS verification: {e}"))?;
            }
            SasState::KeysExchanged { .. } => {
                sas.confirm()
                    .await
                    .map_err(|e| format!("failed to confirm SAS verification: {e}"))?;
            }
            _ => {}
        }
    } else {
        match entry.request.state() {
            VerificationRequestState::Requested { .. } => {
                entry.request
                    .accept_with_methods(vec![VerificationMethod::SasV1])
                    .await
                    .map_err(|e| format!("failed to accept verification request: {e}"))?;
            }
            VerificationRequestState::Ready { .. } => {
                if let Some(sas) = entry
                    .request
                    .start_sas()
                    .await
                    .map_err(|e| format!("failed to start SAS verification: {e}"))?
                {
                    entry.sas = Some(sas);
                }
            }
            VerificationRequestState::Transitioned { verification } => {
                if let Some(sas) = verification.sas() {
                    entry.sas = Some(sas);
                    if let Some(sas) = &entry.sas {
                        if matches!(sas.state(), SasState::Started { .. }) {
                            sas.accept()
                                .await
                                .map_err(|e| format!("failed to accept SAS verification: {e}"))?;
                        }
                    }
                }
            }
            _ => {}
        }
    }

    sessions
        .lock()
        .expect("poisoned matrix backend verification sessions mutex")
        .insert(flow_id.to_owned(), entry);
    Ok(())
}

pub async fn cancel_verification_session(
    handle_id: u64,
    flow_id: &str,
    mismatch: bool,
) -> Result<(), String> {
    let sessions = verification_sessions_for_handle(handle_id)?;
    let entry = {
        let mut sessions = sessions
            .lock()
            .expect("poisoned matrix backend verification sessions mutex");
        sessions.remove(flow_id).ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no verification flow '{flow_id}'"
            )
        })?
    };

    if let Some(sas) = entry.sas {
        if mismatch {
            sas.mismatch()
                .await
                .map_err(|e| format!("failed to reject SAS verification: {e}"))?;
        } else {
            sas.cancel()
                .await
                .map_err(|e| format!("failed to cancel SAS verification: {e}"))?;
        }
    } else {
        entry
            .request
            .cancel()
            .await
            .map_err(|e| format!("failed to cancel verification request: {e}"))?;
    }

    Ok(())
}
