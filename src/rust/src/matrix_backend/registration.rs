// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    collections::HashMap,
    sync::{
        Mutex, OnceLock,
        atomic::{AtomicU64, Ordering},
    },
};

use matrix_sdk::{
    Client, ClientBuildError,
    ruma::{
        OwnedClientSecret, OwnedSessionId, UInt,
        api::client::{
            account::{
                get_username_availability,
                register::v3::Request as RegisterRequest,
                request_registration_token_via_email,
            },
            uiaa::{
                self, AuthData, AuthFlow, AuthType, LoginTermsParams, UiaaInfo,
            },
        },
    },
};

static NEXT_REGISTRATION_ID: AtomicU64 = AtomicU64::new(1);

fn pending_registrations() -> &'static Mutex<HashMap<u64, PendingRegistration>> {
    static PENDING: OnceLock<Mutex<HashMap<u64, PendingRegistration>>> = OnceLock::new();
    PENDING.get_or_init(|| Mutex::new(HashMap::new()))
}

struct PendingRegistration {
    client: Client,
    homeserver_url: String,
    session: String,
    flows: Vec<AuthFlow>,
    completed: Vec<AuthType>,
    chosen_flow_index: usize,
    params: Option<Box<serde_json::value::RawValue>>,
}

// --- Public result types ---

pub struct RegistrationProbeResult {
    pub registration_id: u64,
    pub homeserver_url: String,
    pub session: String,
    pub chosen_flow_stages: Vec<String>,
    pub all_flows: Vec<Vec<String>>,
    pub terms_policies: Vec<RegistrationTermsPolicy>,
}

pub struct RegistrationTermsPolicy {
    pub id: String,
    pub version: String,
    pub name: String,
    pub url: String,
}

pub struct RegistrationUsernameResult {
    pub available: bool,
}

pub struct RegistrationSubmitResult {
    pub completed: bool,
    pub user_id: String,
    pub access_token: String,
    pub device_id: String,
    pub homeserver_url: String,
    pub session: String,
    pub remaining_stages: Vec<String>,
    pub completed_stages: Vec<String>,
    pub terms_policies: Vec<RegistrationTermsPolicy>,
}

pub struct RegistrationEmailTokenResult {
    pub sid: String,
}

// --- Flow selection ---

fn is_native_stage(stage: &AuthType) -> bool {
    matches!(
        stage,
        AuthType::Dummy
            | AuthType::Password
            | AuthType::EmailIdentity
            | AuthType::RegistrationToken
            | AuthType::Terms
    )
}

fn flow_score(flow: &AuthFlow) -> (usize, usize) {
    let fallback_count = flow.stages.iter().filter(|s| !is_native_stage(s)).count();
    (fallback_count, flow.stages.len())
}

fn choose_best_flow(flows: &[AuthFlow]) -> usize {
    flows
        .iter()
        .enumerate()
        .min_by_key(|(_, flow)| flow_score(flow))
        .map(|(index, _)| index)
        .unwrap_or(0)
}

fn auth_type_to_string(auth_type: &AuthType) -> String {
    match auth_type {
        AuthType::Password => "m.login.password".to_owned(),
        AuthType::ReCaptcha => "m.login.recaptcha".to_owned(),
        AuthType::EmailIdentity => "m.login.email.identity".to_owned(),
        AuthType::Msisdn => "m.login.msisdn".to_owned(),
        AuthType::Sso => "m.login.sso".to_owned(),
        AuthType::Dummy => "m.login.dummy".to_owned(),
        AuthType::RegistrationToken => "m.login.registration_token".to_owned(),
        AuthType::Terms => "m.login.terms".to_owned(),
        AuthType::OAuth => "m.oauth".to_owned(),
        _ => format!("{auth_type}"),
    }
}

fn extract_terms_policies(uiaa_info: &UiaaInfo) -> Vec<RegistrationTermsPolicy> {
    let Ok(Some(terms_params)) = uiaa_info.params::<LoginTermsParams>(&AuthType::Terms) else {
        return Vec::new();
    };

    terms_params
        .policies
        .into_iter()
        .map(|(id, definition)| {
            let (name, url) = definition
                .translations
                .get("en")
                .or_else(|| definition.translations.values().next())
                .map(|t| (t.name.clone(), t.url.clone()))
                .unwrap_or_default();
            RegistrationTermsPolicy {
                id,
                version: definition.version,
                name,
                url,
            }
        })
        .collect()
}

fn remaining_stages(flow: &AuthFlow, completed: &[AuthType]) -> Vec<String> {
    flow.stages
        .iter()
        .filter(|stage| !completed.contains(stage))
        .map(auth_type_to_string)
        .collect()
}

// --- Public API ---

pub async fn probe_registration_flows(
    server_name_or_url: &str,
    verify_certificates: bool,
) -> Result<RegistrationProbeResult, String> {
    tracing::info!(
        server_name_or_url,
        verify_certificates,
        "Probing Matrix registration flows"
    );

    let client = build_discovery_client(server_name_or_url, verify_certificates)
        .await
        .map_err(|e| format_client_build_error(&e))?;

    let homeserver_url = client.homeserver().to_string();

    // Send empty register request to provoke 401 + UiaaInfo
    let request = RegisterRequest::new();
    let uiaa_info = match client.matrix_auth().register(request).await {
        Ok(_) => {
            // Registration succeeded without UIAA — no stages required (unlikely but possible)
            return Err("Server does not require any authentication for registration. This is unexpected.".to_owned());
        }
        Err(error) => {
            if let Some(info) = error.as_uiaa_response() {
                info.clone()
            } else {
                return Err(format_registration_error(&error));
            }
        }
    };

    if uiaa_info.flows.is_empty() {
        return Err("Server returned no registration flows. Registration may be disabled.".to_owned());
    }

    let session = uiaa_info
        .session
        .clone()
        .unwrap_or_default();

    let chosen_flow_index = choose_best_flow(&uiaa_info.flows);
    let chosen_stages: Vec<String> = uiaa_info.flows[chosen_flow_index]
        .stages
        .iter()
        .map(auth_type_to_string)
        .collect();

    let all_flows: Vec<Vec<String>> = uiaa_info
        .flows
        .iter()
        .map(|flow| flow.stages.iter().map(auth_type_to_string).collect())
        .collect();

    let terms_policies = extract_terms_policies(&uiaa_info);

    let registration_id = NEXT_REGISTRATION_ID.fetch_add(1, Ordering::Relaxed);

    tracing::info!(
        registration_id,
        homeserver_url = %homeserver_url,
        session = %session,
        flow_count = uiaa_info.flows.len(),
        chosen_flow = chosen_flow_index,
        chosen_stages = ?chosen_stages,
        "Probed Matrix registration flows"
    );

    pending_registrations()
        .lock()
        .expect("poisoned registration mutex")
        .insert(
            registration_id,
            PendingRegistration {
                client,
                homeserver_url: homeserver_url.clone(),
                session: session.clone(),
                flows: uiaa_info.flows,
                completed: uiaa_info.completed,
                chosen_flow_index,
                params: uiaa_info.params,
            },
        );

    Ok(RegistrationProbeResult {
        registration_id,
        homeserver_url,
        session,
        chosen_flow_stages: chosen_stages,
        all_flows,
        terms_policies,
    })
}

pub async fn check_username_available(
    registration_id: u64,
    username: &str,
) -> Result<RegistrationUsernameResult, String> {
    let client = {
        let pending = pending_registrations()
            .lock()
            .expect("poisoned registration mutex");
        let reg = pending
            .get(&registration_id)
            .ok_or_else(|| format!("unknown registration session: {registration_id}"))?;
        reg.client.clone()
    };

    let request = get_username_availability::v3::Request::new(username.to_owned());
    match client.send(request).await {
        Ok(response) => Ok(RegistrationUsernameResult {
            available: response.available,
        }),
        Err(error) => {
            // 400 M_USER_IN_USE means unavailable, not a fatal error
            if let Some(api_error) = error.as_client_api_error() {
                if api_error.status_code.as_u16() == 400 {
                    return Ok(RegistrationUsernameResult { available: false });
                }
            }
            Err(format!("Failed to check username availability: {error}"))
        }
    }
}

pub async fn submit_registration_stage(
    registration_id: u64,
    username: &str,
    password: &str,
    device_name: &str,
    stage_type: &str,
    token: &str,
    email_sid: &str,
    email_client_secret: &str,
) -> Result<RegistrationSubmitResult, String> {
    let (client, session, homeserver_url) = {
        let pending = pending_registrations()
            .lock()
            .expect("poisoned registration mutex");
        let reg = pending
            .get(&registration_id)
            .ok_or_else(|| format!("unknown registration session: {registration_id}"))?;
        (
            reg.client.clone(),
            reg.session.clone(),
            reg.homeserver_url.clone(),
        )
    };

    let auth = build_auth_data(stage_type, &session, token, email_sid, email_client_secret)?;

    let mut request = RegisterRequest::new();
    request.username = Some(username.to_owned());
    request.password = Some(password.to_owned());
    if !device_name.trim().is_empty() {
        request.initial_device_display_name = Some(device_name.to_owned());
    }
    request.auth = Some(auth);

    tracing::info!(
        registration_id,
        stage_type,
        username,
        "Submitting registration stage"
    );

    match client.matrix_auth().register(request).await {
        Ok(response) => {
            // Registration complete
            let user_id = response.user_id.to_string();
            let access_token = response.access_token.unwrap_or_default();
            let device_id = response
                .device_id
                .map(|d| d.to_string())
                .unwrap_or_default();

            // Clean up the pending registration
            pending_registrations()
                .lock()
                .expect("poisoned registration mutex")
                .remove(&registration_id);

            tracing::info!(
                registration_id,
                user_id = %user_id,
                device_id = %device_id,
                "Registration completed successfully"
            );

            Ok(RegistrationSubmitResult {
                completed: true,
                user_id,
                access_token,
                device_id,
                homeserver_url,
                session: String::new(),
                remaining_stages: Vec::new(),
                completed_stages: Vec::new(),
                terms_policies: Vec::new(),
            })
        }
        Err(error) => {
            if let Some(uiaa_info) = error.as_uiaa_response() {
                // Stage completed, more stages needed
                let updated_session = uiaa_info
                    .session
                    .clone()
                    .unwrap_or_else(|| session.clone());

                let terms_policies = extract_terms_policies(uiaa_info);

                // Update the pending registration state
                {
                    let mut pending = pending_registrations()
                        .lock()
                        .expect("poisoned registration mutex");
                    if let Some(reg) = pending.get_mut(&registration_id) {
                        reg.session = updated_session.clone();
                        reg.completed = uiaa_info.completed.clone();
                        reg.params = uiaa_info.params.clone();
                    }
                }

                let (remaining, completed_stages, chosen_flow_index) = {
                    let pending = pending_registrations()
                        .lock()
                        .expect("poisoned registration mutex");
                    let reg = pending
                        .get(&registration_id)
                        .ok_or_else(|| {
                            format!("registration session lost: {registration_id}")
                        })?;
                    let remaining = remaining_stages(
                        &reg.flows[reg.chosen_flow_index],
                        &uiaa_info.completed,
                    );
                    let completed: Vec<String> = uiaa_info
                        .completed
                        .iter()
                        .map(auth_type_to_string)
                        .collect();
                    (remaining, completed, reg.chosen_flow_index)
                };

                // If there's an auth_error, it means the stage submission failed
                if let Some(ref auth_error) = uiaa_info.auth_error {
                    let error_msg = &auth_error.message;
                    tracing::warn!(
                        registration_id,
                        stage_type,
                        error = %error_msg,
                        "Registration stage failed"
                    );
                    return Err(error_msg.clone());
                }

                tracing::info!(
                    registration_id,
                    stage_type,
                    remaining_count = remaining.len(),
                    chosen_flow = chosen_flow_index,
                    "Registration stage completed, more stages needed"
                );

                Ok(RegistrationSubmitResult {
                    completed: false,
                    user_id: String::new(),
                    access_token: String::new(),
                    device_id: String::new(),
                    homeserver_url,
                    session: updated_session,
                    remaining_stages: remaining,
                    completed_stages,
                    terms_policies,
                })
            } else {
                Err(format_registration_error(&error))
            }
        }
    }
}

pub async fn request_email_token(
    registration_id: u64,
    email: &str,
    client_secret: &str,
    send_attempt: u64,
) -> Result<RegistrationEmailTokenResult, String> {
    let client = {
        let pending = pending_registrations()
            .lock()
            .expect("poisoned registration mutex");
        let reg = pending
            .get(&registration_id)
            .ok_or_else(|| format!("unknown registration session: {registration_id}"))?;
        reg.client.clone()
    };

    let parsed_secret: OwnedClientSecret = client_secret
        .parse()
        .map_err(|e| format!("invalid client_secret: {e}"))?;

    let send_attempt_uint = UInt::try_from(send_attempt)
        .map_err(|e| format!("invalid send_attempt: {e}"))?;

    let request = request_registration_token_via_email::v3::Request::new(
        parsed_secret,
        email.to_owned(),
        send_attempt_uint,
    );

    tracing::info!(
        registration_id,
        email,
        send_attempt,
        "Requesting registration email token"
    );

    let response = client
        .send(request)
        .await
        .map_err(|e| format!("Failed to request email verification: {e}"))?;

    Ok(RegistrationEmailTokenResult {
        sid: response.sid.to_string(),
    })
}

pub fn cancel_registration(registration_id: u64) -> Result<(), String> {
    let _rt_guard = crate::ffi::runtime().enter();

    pending_registrations()
        .lock()
        .expect("poisoned registration mutex")
        .remove(&registration_id)
        .map(|_| ())
        .ok_or_else(|| format!("unknown registration session: {registration_id}"))
}

// --- Helpers ---

fn build_auth_data(
    stage_type: &str,
    session: &str,
    token: &str,
    email_sid: &str,
    email_client_secret: &str,
) -> Result<AuthData, String> {
    match stage_type {
        "m.login.dummy" => {
            let mut dummy = uiaa::Dummy::new();
            dummy.session = Some(session.to_owned());
            Ok(AuthData::Dummy(dummy))
        }
        "m.login.terms" => {
            let mut terms = uiaa::Terms::new();
            terms.session = Some(session.to_owned());
            Ok(AuthData::Terms(terms))
        }
        "m.login.registration_token" => {
            if token.trim().is_empty() {
                return Err("Registration token cannot be empty".to_owned());
            }
            let mut reg_token = uiaa::RegistrationToken::new(token.to_owned());
            reg_token.session = Some(session.to_owned());
            Ok(AuthData::RegistrationToken(reg_token))
        }
        "m.login.email.identity" => {
            if email_sid.trim().is_empty() || email_client_secret.trim().is_empty() {
                return Err(
                    "Email session ID and client secret are required for email verification"
                        .to_owned(),
                );
            }
            let sid: OwnedSessionId = email_sid
                .parse()
                .map_err(|e| format!("invalid email session ID: {e}"))?;
            let secret: OwnedClientSecret = email_client_secret
                .parse()
                .map_err(|e| format!("invalid client secret: {e}"))?;
            let creds = uiaa::ThirdpartyIdCredentials::new(sid, secret);
            let mut obj = serde_json::Map::new();
            obj.insert(
                "threepid_creds".into(),
                serde_json::to_value(&creds)
                    .map_err(|e| format!("failed to serialize email credentials: {e}"))?,
            );
            AuthData::new("m.login.email.identity", Some(session.to_owned()), obj)
                .map_err(|e| format!("failed to build email identity auth data: {e}"))
        }
        // ReCaptcha, SSO, and unknown stages use browser fallback acknowledgement
        _ => Ok(AuthData::fallback_acknowledgement(session.to_owned())),
    }
}

async fn build_discovery_client(
    server_name_or_url: &str,
    verify_certificates: bool,
) -> Result<Client, ClientBuildError> {
    let mut builder = Client::builder().server_name_or_homeserver_url(server_name_or_url);
    if !verify_certificates {
        builder = builder.disable_ssl_verification();
    }
    builder.build().await
}

fn format_client_build_error(error: &ClientBuildError) -> String {
    match error {
        ClientBuildError::InvalidServerName => {
            "Received malformed response. Make sure the homeserver domain is valid.".to_owned()
        }
        ClientBuildError::Http(error) => {
            format!("Failed to contact the homeserver: {error}")
        }
        _ => format!("Failed to connect to the homeserver: {error}"),
    }
}

/// Translated to user-visible text in C++ StateEventText::translateAuthError().
/// When adding or changing constant error strings here, update that function too.
fn format_registration_error(error: &matrix_sdk::Error) -> String {
    if let Some(api_error) = error.as_client_api_error() {
        if api_error.status_code.as_u16() == 403 {
            return "Registration is disabled on this server.".to_owned();
        }
        if let matrix_sdk::ruma::api::error::ErrorBody::Standard(body) = &api_error.body {
            if !body.message.is_empty() {
                return body.message.clone();
            }
        }
    }
    format!("Registration failed: {error}")
}
