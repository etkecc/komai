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
use matrix_sdk::widget::{
    Capabilities, CapabilitiesProvider, ClientProperties, EncryptionSystem, Filter,
    MessageLikeEventFilter, StateEventFilter, ToDeviceEventFilter, VirtualElementCallWidgetConfig,
    VirtualElementCallWidgetProperties, WidgetDriver, WidgetSettings,
};
use tokio_util::sync::CancellationToken;

/// Client identifier handed to Element Call so it can adapt to the host.
const CLIENT_ID: &str = "cc.etke.komai";

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
    // `new_virtual_element_call_widget` emits neither key when the corresponding
    // config fields are `None`, so appending is safe.
    let fragment =
        format!("{}&header=none&confineToRoom=true", url.fragment().unwrap_or("?"));
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
                    if !handle.send(message).await {
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
