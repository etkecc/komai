// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Element Call widget driver glue.
//!
//! Element Call runs as a Matrix widget inside a QtWebEngine view and talks to
//! its host over the Matrix Widget API (`postMessage`). We drive that protocol
//! with the native `matrix_sdk::widget` driver (the same path Element X uses on
//! iOS/Android): it negotiates capabilities, relays room events / to-device
//! traffic, answers OpenID and handles MSC4157 delayed events on behalf of the
//! embedded widget, all against our already-logged-in `Client`.
//!
//! Per session we:
//!   * build the EC widget URL with [`WidgetSettings::new_virtual_element_call_widget`]
//!     + [`WidgetSettings::generate_webview_url`] (never hand-rolled), and hand it
//!     back to C++ so the webview loads `komai-ec://app/...#?widgetId=…&roomId=…`;
//!   * spawn the driver (`WidgetDriver::run`) and a receive loop that forwards
//!     driver→widget messages to C++ (`matrix_notify_element_call_widget_message`);
//!   * forward widget→driver messages (pushed in by C++ via
//!     [`send_element_call_message`]) onto the driver handle, in order.
//!
//! The C++/QML side injects a `window.postMessage` bridge into the webview and
//! relays the raw JSON strings both ways (see `ElementCallWidgetSession`).

use super::*;

use language_tags::LanguageTag;
use matrix_sdk::ruma::api::client::rtc::{RtcTransport, transports::v1 as rtc_transports};
use matrix_sdk::widget::{
    Capabilities, CapabilitiesProvider, ClientProperties, EncryptionSystem, Filter,
    MessageLikeEventFilter, StateEventFilter, ToDeviceEventFilter, VirtualElementCallWidgetConfig,
    VirtualElementCallWidgetProperties, WidgetDriver, WidgetSettings,
};
use tokio_util::sync::CancellationToken;

/// Client identifier handed to Element Call so it can adapt to the host.
const CLIENT_ID: &str = "cc.etke.komai";

/// Widget API version string that advertises MSC4515 support (RTC transport
/// discovery over the widget API).
const MSC4515_API_VERSION: &str = "org.matrix.msc4515";

/// The MSC4515 widget action Element Call uses to ask its host where the
/// MatrixRTC backend lives.
const MSC4515_GET_RTC_TRANSPORTS: &str = "org.matrix.msc4515.get_rtc_transports";

/// The canonical Element Call capability set, ported from matrix-sdk-ffi's
/// `get_element_call_required_permissions`. Our `CapabilitiesProvider` returns
/// this verbatim (Element Call is a trusted first-party widget — no prompt),
/// ignoring whatever the widget requests.
///
/// Event types are spelled as string literals rather than `ruma` enum variants
/// because several of them (`org.matrix.msc4075.*`, `org.matrix.msc4310.*`,
/// `org.matrix.msc3401.call.member`) live behind `unstable-msc*` `ruma` features
/// we don't enable; the strings are the spec-correct on-the-wire identifiers.
fn element_call_required_permissions(own_user_id: &str, own_device_id: &str) -> Capabilities {
    // Filters that appear in both the read and the send list.
    let read_send = || {
        vec![
            // Read and send rageshake requests from other room members.
            Filter::MessageLike(MessageLikeEventFilter::WithType(
                "org.matrix.rageshake_request".into(),
            )),
            // Read and send encryption keys (to-device).
            Filter::ToDevice(ToDeviceEventFilter::new("io.element.call.encryption_keys".into())),
            // Legacy room-event encryption keys (kept until all MatrixRTC apps
            // support to-device encryption keys).
            Filter::MessageLike(MessageLikeEventFilter::WithType(
                "io.element.call.encryption_keys".into(),
            )),
            // EC's custom reaction (can be sent repeatedly to the same event).
            Filter::MessageLike(MessageLikeEventFilter::WithType("io.element.call.reaction".into())),
            // Raise-hand reactions.
            Filter::MessageLike(MessageLikeEventFilter::WithType("m.reaction".into())),
            // Detect hands being lowered again.
            Filter::MessageLike(MessageLikeEventFilter::WithType("m.room.redaction".into())),
            // Decline an incoming call / detect declines (MSC4310).
            Filter::MessageLike(MessageLikeEventFilter::WithType(
                "org.matrix.msc4310.rtc.decline".into(),
            )),
        ]
    };

    let mut read = vec![
        // Compute the current MatrixRTC session state.
        Filter::State(StateEventFilter::WithType("org.matrix.msc3401.call.member".into())),
        // Display the room name.
        Filter::State(StateEventFilter::WithType("m.room.name".into())),
        // Detect leaving/kicked members during a call.
        Filter::State(StateEventFilter::WithType("m.room.member".into())),
        // Decide whether to encrypt the call streams.
        Filter::State(StateEventFilter::WithType("m.room.encryption".into())),
        // Read the room version (version-specific auth rules, MSC3779).
        Filter::State(StateEventFilter::WithType("m.room.create".into())),
    ];
    read.extend(read_send());

    let mut send = vec![
        // Notify other users that a call has started (MSC4075).
        Filter::MessageLike(MessageLikeEventFilter::WithType(
            "org.matrix.msc4075.rtc.notification".into(),
        )),
        // Deprecated fallback notification type EC still sends alongside the above.
        Filter::MessageLike(MessageLikeEventFilter::WithType(
            "org.matrix.msc4075.call.notify".into(),
        )),
        // Call participation state event (the main MatrixRTC event). The several
        // state-key shapes cover legacy single-event memberships and the
        // MSC3779/MSC4143 per-device variants (with/without leading underscore
        // and `_m.call` application suffix).
        Filter::State(StateEventFilter::WithTypeAndStateKey(
            "org.matrix.msc3401.call.member".into(),
            own_user_id.to_owned(),
        )),
        Filter::State(StateEventFilter::WithTypeAndStateKey(
            "org.matrix.msc3401.call.member".into(),
            format!("{own_user_id}_{own_device_id}"),
        )),
        Filter::State(StateEventFilter::WithTypeAndStateKey(
            "org.matrix.msc3401.call.member".into(),
            format!("{own_user_id}_{own_device_id}_m.call"),
        )),
        Filter::State(StateEventFilter::WithTypeAndStateKey(
            "org.matrix.msc3401.call.member".into(),
            format!("_{own_user_id}_{own_device_id}"),
        )),
        Filter::State(StateEventFilter::WithTypeAndStateKey(
            "org.matrix.msc3401.call.member".into(),
            format!("_{own_user_id}_{own_device_id}_m.call"),
        )),
    ];
    send.extend(read_send());

    Capabilities {
        read,
        send,
        requires_client: true,
        update_delayed_event: true,
        send_delayed_event: true,
        download_file: true,
    }
}

/// Auto-approving capabilities provider for the (trusted) Element Call widget.
struct ElementCallCapabilitiesProvider {
    user_id: String,
    device_id: String,
}

impl CapabilitiesProvider for ElementCallCapabilitiesProvider {
    async fn acquire_capabilities(&self, _capabilities: Capabilities) -> Capabilities {
        element_call_required_permissions(&self.user_id, &self.device_id)
    }
}

/// A live widget session. Dropping the entry (or cancelling the token) tears the
/// session down: the dropped sender ends the forward loop and the token stops
/// the driver + receive loop.
struct ElementCallSession {
    /// Widget→driver messages pushed in from the webview, drained in order by
    /// the session's forward loop.
    to_driver_tx: mpsc::UnboundedSender<String>,
    cancel: CancellationToken,
}

fn sessions() -> &'static Mutex<HashMap<u64, ElementCallSession>> {
    static SESSIONS: OnceLock<Mutex<HashMap<u64, ElementCallSession>>> = OnceLock::new();
    SESSIONS.get_or_init(|| Mutex::new(HashMap::new()))
}

fn next_session_id() -> u64 {
    static NEXT: AtomicU64 = AtomicU64::new(1);
    NEXT.fetch_add(1, Ordering::Relaxed)
}

fn remove_session(session_id: u64) {
    sessions()
        .lock()
        .expect("poisoned element call session registry mutex")
        .remove(&session_id);
}

/// Start an Element Call widget session for `room_id`.
///
/// `base_url` is the secure origin the embedded bundle is served from
/// (`komai-ec://app/`, owned by the C++ scheme handler). Returns a session id
/// immediately; the generated webview URL is delivered asynchronously through
/// `matrix_notify_element_call_widget_url_ready` once the homeserver profile
/// lookup completes.
pub fn start_element_call_session(
    handle_id: u64,
    room_id: &str,
    base_url: &str,
    lang: &str,
    theme: &str,
) -> Result<u64, String> {
    // Validate the room synchronously so the caller learns about obvious
    // problems (unknown room / dead handle) before any async work is spawned.
    let room = room_for_handle(handle_id, room_id)?;

    let session_id = next_session_id();
    let (to_driver_tx, to_driver_rx) = mpsc::unbounded_channel::<String>();
    let cancel = CancellationToken::new();

    sessions()
        .lock()
        .expect("poisoned element call session registry mutex")
        .insert(session_id, ElementCallSession { to_driver_tx, cancel: cancel.clone() });

    let base_url = base_url.to_owned();
    let theme = if theme.trim().is_empty() { None } else { Some(theme.to_owned()) };
    // Parse the UI locale into a BCP-47 language tag. A malformed/empty tag
    // falls back to Element Call's default (en-US) inside ClientProperties.
    let lang = if lang.trim().is_empty() { None } else { LanguageTag::parse(lang.trim()).ok() };

    crate::matrix_backend::ffi::runtime().spawn(run_session(
        session_id,
        room,
        base_url,
        lang,
        theme,
        to_driver_rx,
        cancel,
    ));

    tracing::info!(handle_id, session_id, "Started Element Call widget session");
    Ok(session_id)
}

/// Forward a widget→driver message (raw Widget API JSON) from the webview.
pub fn send_element_call_message(session_id: u64, message: &str) -> Result<(), String> {
    let guard = sessions().lock().expect("poisoned element call session registry mutex");
    let session = guard
        .get(&session_id)
        .ok_or_else(|| format!("Element Call widget session {session_id} is not active"))?;
    session.to_driver_tx.send(message.to_owned()).map_err(|_| {
        format!("Element Call widget session {session_id} driver is no longer accepting messages")
    })
}

/// Tear down a widget session (user hung up / call surface closed).
pub fn stop_element_call_session(session_id: u64) {
    let session = sessions()
        .lock()
        .expect("poisoned element call session registry mutex")
        .remove(&session_id);
    if let Some(session) = session {
        tracing::info!(session_id, "Stopping Element Call widget session");
        session.cancel.cancel();
    }
}

/// Whether to treat `room` as a direct (1:1) chat when choosing the call
/// notification type. Mirrors the room-list classifier's `is_direct` rule
/// (`runtime_room_list::classify`): direct if the room has an `m.direct` target,
/// or — as a heuristic for unmarked 1:1 rooms — exactly two active members. We
/// use our own determination rather than the SDK's `Room::is_direct` so the
/// ring/notification choice matches what the rest of Komai considers a DM.
fn room_is_direct(room: &Room) -> bool {
    !room.direct_targets().is_empty() || room.active_members_count() == 2
}

async fn run_session(
    session_id: u64,
    room: Room,
    base_url: String,
    lang: Option<LanguageTag>,
    theme: Option<String>,
    mut to_driver_rx: mpsc::UnboundedReceiver<String>,
    cancel: CancellationToken,
) {
    let client = room.client();
    let own_user_id = room.own_user_id().to_string();
    let device_id =
        client.device_id().map(|id| id.to_string()).unwrap_or_else(|| "UNKNOWN".to_owned());

    // Element Call is an SPA whose router maps "/" to its home page and any
    // other path to the in-room call view. Point the widget at the "/room"
    // route (the same convention Element X / Element Web use, e.g.
    // `https://call.element.dev/room`) so it opens the call for `roomId` instead
    // of the "start a new call" home page. The scheme handler serves index.html
    // for this client-side route (SPA fallback). `base_url` is the secure origin
    // (`komai-ec://app/`); roomId/widgetId/... all travel in the URL fragment.
    let element_call_url =
        if base_url.ends_with('/') { format!("{base_url}room") } else { format!("{base_url}/room") };

    let props = VirtualElementCallWidgetProperties {
        element_call_url,
        widget_id: format!("komai-ec-{session_id}"),
        encryption: EncryptionSystem::PerParticipantKeys,
        ..Default::default()
    };
    // Leave the config at its defaults: no intent => Element Call shows its
    // lobby (the M4 goal). Header/PiP/audio-device tuning is deferred to the
    // real call UX milestone.
    let config = VirtualElementCallWidgetConfig::default();

    let settings = match WidgetSettings::new_virtual_element_call_widget(props, config) {
        Ok(settings) => settings,
        Err(e) => {
            finish_failed(session_id, format!("failed to build Element Call widget settings: {e}"));
            return;
        }
    };

    let client_props = ClientProperties::new(CLIENT_ID, lang, theme);
    let mut url = match settings.generate_webview_url(&room, client_props).await {
        Ok(url) => url,
        Err(e) => {
            finish_failed(session_id, format!("failed to generate Element Call widget URL: {e}"));
            return;
        }
    };

    // Element's convention (and ours): a 1:1 call RINGS the other party, a group
    // call sends a silent notification. We pick this ourselves rather than via an
    // EC `intent` preset, using our own DM determination (see `room_is_direct`).
    //   * `ring` makes the callee's client ring audibly; `notification` is silent.
    //     Either way Element Call publishes an `m.rtc.notification` (MSC4075) on
    //     call start, which also drives our "started a call" timeline tile.
    //   * For a DM ring we also set `waitForCallPickup` + `autoLeave` so our
    //     caller-side EC stops ringing out and leaves when the callee declines,
    //     nobody answers, or the other party leaves (Element's StartNewCallDM
    //     behaviour). Group notifications need neither.
    let (notification_type, dm_ring_params) = if room_is_direct(&room) {
        ("ring", "&waitForCallPickup=true&autoLeave=true")
    } else {
        ("notification", "")
    };

    // Tune Element Call's chrome for the embedded desktop surface. We pass no
    // `intent`, so EC falls into its "unknown intent" preset which turns the
    // branded header ON and `confineToRoom` OFF; we want the opposite:
    //   * `header=none` removes the branded EC header (we draw our own bar).
    //     `HeaderStyle` isn't re-exported from `matrix_sdk::widget` (so we can't
    //     set `config.header`) and EC 0.20.x no longer reads the deprecated
    //     `hideHeader` param, so the URL param is the only lever.
    //   * `confineToRoom=true` removes EC's "Back to recents" / return-to-home
    //     navigation (the call is embedded in a single room; leaving is driven by
    //     our own End call button).
    // `new_virtual_element_call_widget` emits none of these keys when the
    // corresponding config fields are `None`, so appending is safe.
    let fragment = format!(
        "{}&header=none&confineToRoom=true&sendNotificationType={notification_type}{dm_ring_params}",
        url.fragment().unwrap_or("?")
    );
    url.set_fragment(Some(&fragment));
    let url = url.to_string();

    let (driver, handle) = WidgetDriver::new(settings);
    let caps = ElementCallCapabilitiesProvider { user_id: own_user_id, device_id };

    // The widget API state machine. Single-shot and long-lived: it suspends for
    // the whole call and returns once the widget disconnects.
    let run_room = room.clone();
    let run_cancel = cancel.clone();
    let run_task = crate::matrix_backend::ffi::runtime().spawn(async move {
        tokio::select! {
            _ = run_cancel.cancelled() => {}
            result = driver.run(run_room, caps) => {
                if result.is_err() {
                    tracing::warn!(session_id, "Element Call widget driver stopped with an error");
                }
            }
        }
        // If the driver finished on its own, make sure everyone else winds down.
        run_cancel.cancel();
    });

    // Driver→widget: forward to the webview via C++.
    let recv_handle = handle.clone();
    let recv_cancel = cancel.clone();
    let recv_task = crate::matrix_backend::ffi::runtime().spawn(async move {
        loop {
            tokio::select! {
                _ = recv_cancel.cancelled() => break,
                message = recv_handle.recv() => match message {
                    Some(message) => {
                        let message = advertise_msc4515(session_id, message);
                        crate::ffi::matrix_notify_element_call_widget_message(session_id, &message);
                    }
                    // `None` => the driver is gone; stop the session.
                    None => break,
                }
            }
        }
        recv_cancel.cancel();
    });

    // The webview can load now.
    crate::ffi::matrix_notify_element_call_widget_url_ready(session_id, &url);
    tracing::info!(session_id, "Element Call widget URL ready");

    // Widget→driver: drain messages pushed in from the webview, in order.
    loop {
        tokio::select! {
            _ = cancel.cancelled() => break,
            message = to_driver_rx.recv() => match message {
                Some(message) => {
                    // MSC4515 is ours to answer: the driver has no handler and
                    // would reject it (see `answer_rtc_transports`). Resolving
                    // the transports hits the network, so it runs off to the
                    // side rather than stalling this forwarding loop.
                    if let Some(request) = parse_rtc_transports_request(&message) {
                        crate::matrix_backend::ffi::runtime().spawn(answer_rtc_transports(
                            session_id,
                            client.clone(),
                            request,
                        ));
                    } else if !handle.send(message).await {
                        break;
                    }
                }
                None => break,
            }
        }
    }

    cancel.cancel();
    let _ = recv_task.await;
    run_task.abort();

    finish_stopped(session_id);
}

/// Splices MSC4515 into the driver's `supported_api_versions` answer.
///
/// Element Call only sends `get_rtc_transports` once the host has advertised
/// `org.matrix.msc4515`, and matrix-sdk's driver answers that handshake from a
/// hardcoded list that predates the MSC. We amend its response instead of
/// answering the action ourselves so the rest of the list keeps tracking
/// whatever matrix-sdk supports. Any message we cannot parse or that is not the
/// versions response passes through untouched.
fn advertise_msc4515(session_id: u64, message: String) -> String {
    let Ok(mut value) = serde_json::from_str::<serde_json::Value>(&message) else {
        return message;
    };
    if value.get("action").and_then(|action| action.as_str()) != Some("supported_api_versions") {
        return message;
    }
    let Some(versions) = value
        .get_mut("response")
        .and_then(|response| response.get_mut("supported_versions"))
        .and_then(|versions| versions.as_array_mut())
    else {
        // Only reachable if matrix-sdk changes the shape of its answer. Element
        // Call would then silently never ask for transports, so say so loudly.
        tracing::warn!(
            session_id,
            "Could not advertise MSC4515: unexpected supported_api_versions response from the widget driver"
        );
        return message;
    };
    if versions.iter().any(|version| version.as_str() == Some(MSC4515_API_VERSION)) {
        return message;
    }
    versions.push(serde_json::Value::String(MSC4515_API_VERSION.to_owned()));
    tracing::info!(session_id, "Advertised MSC4515 RTC transport discovery to Element Call");
    serde_json::to_string(&value).unwrap_or(message)
}

/// Recognises Element Call's MSC4515 `get_rtc_transports` request.
///
/// Returns the request envelope, which the reply echoes back with a `response`
/// field appended (matrix-widget-api's reply shape). Messages that already carry
/// a `response` are replies, not requests, and are left alone.
fn parse_rtc_transports_request(message: &str) -> Option<serde_json::Value> {
    let value = serde_json::from_str::<serde_json::Value>(message).ok()?;
    let object = value.as_object()?;
    if object.get("api").and_then(|api| api.as_str()) != Some("fromWidget") {
        return None;
    }
    if object.contains_key("response") {
        return None;
    }
    if object.get("action").and_then(|action| action.as_str()) != Some(MSC4515_GET_RTC_TRANSPORTS) {
        return None;
    }
    Some(value)
}

/// Answers one `get_rtc_transports` request straight back into the webview.
///
/// The driver never sees the request, so nothing else is going to reply to it:
/// a failure has to come back as a widget error response, or Element Call waits
/// forever on the lobby.
async fn answer_rtc_transports(session_id: u64, client: Client, mut request: serde_json::Value) {
    tracing::info!(session_id, "Element Call asked for the MatrixRTC transports");

    let response = match resolve_rtc_transports(session_id, &client).await {
        Ok(transports) => {
            // The full payload, SFU URLs included: this is the one line that
            // says whether Element Call got something it can actually dial.
            let payload = serde_json::json!({ "rtc_transports": transports });
            if transports.is_empty() {
                tracing::warn!(
                    session_id,
                    "Answering Element Call with no MatrixRTC transports; it cannot connect a call"
                );
            } else {
                tracing::info!(
                    session_id,
                    transports = %payload["rtc_transports"],
                    "Answering Element Call MatrixRTC transport discovery"
                );
            }
            payload
        }
        Err(error) => {
            tracing::warn!(session_id, error, "Failed to resolve MatrixRTC transports");
            serde_json::json!({ "error": { "message": error } })
        }
    };

    let Some(object) = request.as_object_mut() else { return };
    object.insert("response".to_owned(), response);

    match serde_json::to_string(&request) {
        Ok(message) => {
            crate::ffi::matrix_notify_element_call_widget_message(session_id, &message)
        }
        Err(e) => {
            tracing::warn!(session_id, "Failed to serialize MatrixRTC transports reply: {e}")
        }
    }
}

/// Resolves the homeserver's MatrixRTC transports (the LiveKit SFU Element Call
/// connects its media to).
///
/// MSC4519's `rtc/transports` endpoint is the method Element Call 0.24+ expects,
/// but few homeservers implement it yet, so we fall back to the `.well-known`
/// `rtc_foci` list Element Call used to read for itself before it dropped that
/// discovery path.
async fn resolve_rtc_transports(
    session_id: u64,
    client: &Client,
) -> Result<Vec<RtcTransport>, String> {
    match client.send(rtc_transports::Request::new()).await {
        Ok(response) if !response.rtc_transports.is_empty() => {
            tracing::info!(
                session_id,
                count = response.rtc_transports.len(),
                "Resolved MatrixRTC transports from the homeserver's MSC4519 rtc/transports endpoint"
            );
            return Ok(response.rtc_transports);
        }
        Ok(_) => {
            tracing::info!(
                session_id,
                "MSC4519 rtc/transports endpoint returned no transports; falling back to .well-known"
            );
        }
        Err(e) => {
            tracing::info!(
                session_id,
                "MSC4519 rtc/transports endpoint unavailable ({e}); falling back to .well-known"
            );
        }
    }

    let well_known = client.fetch_client_well_known().await.ok_or_else(|| {
        "no MatrixRTC transports from the rtc/transports endpoint, and the homeserver \
         publishes no client .well-known to fall back to"
            .to_owned()
    })?;
    tracing::info!(
        session_id,
        count = well_known.rtc_foci.len(),
        "Resolved MatrixRTC transports from the .well-known rtc_foci list"
    );
    Ok(well_known.rtc_foci)
}

fn finish_failed(session_id: u64, reason: String) {
    tracing::warn!(session_id, reason, "Element Call widget session failed");
    remove_session(session_id);
    crate::ffi::matrix_notify_element_call_widget_stopped(session_id, &reason);
}

fn finish_stopped(session_id: u64) {
    remove_session(session_id);
    crate::ffi::matrix_notify_element_call_widget_stopped(session_id, "");
    tracing::info!(session_id, "Element Call widget session stopped");
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The exact envelope matrix-sdk's widget driver emits for the handshake
    /// (mirrors its own `test_get_supported_api_versions`).
    fn driver_versions_response() -> String {
        serde_json::json!({
            "api": "fromWidget",
            "widgetId": "komai-ec-1",
            "requestId": "S2ixNhjaC0kd0jJn",
            "action": "supported_api_versions",
            "data": {},
            "response": {
                "supported_versions": ["0.0.1", "0.0.2", "org.matrix.msc4039"],
            },
        })
        .to_string()
    }

    fn supported_versions(message: &str) -> Vec<String> {
        serde_json::from_str::<serde_json::Value>(message).unwrap()["response"]
            ["supported_versions"]
            .as_array()
            .unwrap()
            .iter()
            .map(|version| version.as_str().unwrap().to_owned())
            .collect()
    }

    #[test]
    fn versions_response_gains_msc4515_without_losing_the_driver_list() {
        let amended = advertise_msc4515(1, driver_versions_response());
        assert_eq!(
            supported_versions(&amended),
            ["0.0.1", "0.0.2", "org.matrix.msc4039", MSC4515_API_VERSION]
        );
    }

    #[test]
    fn versions_response_is_not_amended_twice() {
        let amended = advertise_msc4515(1, advertise_msc4515(1, driver_versions_response()));
        assert_eq!(
            supported_versions(&amended).iter().filter(|v| *v == MSC4515_API_VERSION).count(),
            1
        );
    }

    #[test]
    fn unrelated_messages_pass_through_untouched() {
        for message in [
            "not json at all".to_owned(),
            serde_json::json!({"api": "toWidget", "action": "im.vector.hangup"}).to_string(),
            // A malformed versions response must not be "repaired" into one.
            serde_json::json!({"action": "supported_api_versions"}).to_string(),
        ] {
            assert_eq!(advertise_msc4515(1, message.clone()), message);
        }
    }

    #[test]
    fn livekit_transports_round_trip_into_the_shape_element_call_reads() {
        // What a homeserver publishes, in `rtc/transports` and in the
        // `.well-known` `rtc_foci` list alike.
        let advertised = serde_json::json!([
            {"type": "livekit", "livekit_service_url": "https://livekit.example.com"},
            {"type": "future.transport", "some_field": 1},
        ]);
        let transports: Vec<RtcTransport> = serde_json::from_value(advertised.clone()).unwrap();
        assert_eq!(transports[0].transport_type(), "livekit");
        // Element Call picks the first entry whose `type` is `livekit` and dials
        // its `livekit_service_url`, so the reply has to carry both through
        // untouched — including transport types this ruma build has no variant for.
        assert_eq!(
            serde_json::json!({ "rtc_transports": transports }),
            serde_json::json!({ "rtc_transports": advertised })
        );
    }

    #[test]
    fn rtc_transports_request_is_recognised() {
        let request = serde_json::json!({
            "api": "fromWidget",
            "widgetId": "komai-ec-1",
            "requestId": "abc",
            "action": MSC4515_GET_RTC_TRANSPORTS,
            "data": {},
        })
        .to_string();
        assert_eq!(
            parse_rtc_transports_request(&request),
            Some(serde_json::from_str(&request).unwrap())
        );
    }

    #[test]
    fn other_widget_traffic_is_left_to_the_driver() {
        let cases = [
            // The widget's own reply to a host request, not a request.
            serde_json::json!({
                "api": "fromWidget",
                "action": MSC4515_GET_RTC_TRANSPORTS,
                "response": {"rtc_transports": []},
            }),
            // Host→widget direction.
            serde_json::json!({"api": "toWidget", "action": MSC4515_GET_RTC_TRANSPORTS}),
            // A different action.
            serde_json::json!({"api": "fromWidget", "action": "supported_api_versions"}),
        ];
        for case in cases {
            assert_eq!(parse_rtc_transports_request(&case.to_string()), None);
        }
    }
}
