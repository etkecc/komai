// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// ## Per-room sliding-sync subscriptions
//
// When the user opens a room (main window tab, detached window, …) we want
// the server to send us the room's extended required_state — in particular
// `m.room.pinned_events` — so we can read pinned-message data straight from
// the local state store and receive live updates when another client pins
// or unpins something. Matrix-sdk-ui's `RoomListService::subscribe_to_rooms`
// does that, but it has three quirks to work around:
//
// 1. **Clear-and-replace semantics.** Each call replaces the full set of
//    subscriptions. To support multiple open rooms (e.g. main window plus a
//    detached window) we keep a refcounted set of "rooms some UI is showing"
//    and submit the full union on every change.
//
// 2. **Replay cost.** Every `subscribe_to_rooms` call marks all subscribed
//    rooms' members as missing (the SDK re-fetches them). Re-submitting the
//    same set would trigger unnecessary member refetches, so the reconciler
//    tracks the last-submitted set and only calls the SDK when it changes.
//
// 3. **Debounce.** Rapid UI actions (tab switching, close-reopen) can toggle
//    the set in bursts. The reconciler waits a short window after each
//    change notification to coalesce them into one `subscribe_to_rooms`
//    call.
//
// The reconciler also owns a per-room `pinned_event_ids_stream` forwarder:
// for each room that enters the subscription set, it emits the current
// pinned list to the UI immediately and keeps emitting when the state-store
// value changes, until the room leaves the set.

use std::collections::{HashMap, HashSet};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use matrix_sdk::{Client, ruma::OwnedRoomId, stream::StreamExt};
use matrix_sdk_ui::room_list_service::RoomListService;
use tokio::sync::Notify;

/// How long to wait after a subscription-set change before calling
/// `RoomListService::subscribe_to_rooms`, to coalesce bursts of
/// subscribe/unsubscribe FFI calls.
const SUBSCRIPTION_DEBOUNCE: Duration = Duration::from_millis(150);

/// How long to wait before retrying to start a per-room forwarder when the
/// room isn't known to the client yet (typical first-subscribe race: the
/// sliding-sync response with the room info hasn't arrived yet).
const FORWARDER_ROOM_WAIT: Duration = Duration::from_millis(500);

/// Reference-counted set of rooms that are currently in use by some UI.
///
/// Keyed by `OwnedRoomId`, value is the number of UI surfaces (main-window
/// active tab, detached room windows, …) currently showing that room.
/// Zero-count rooms are removed.
pub struct SubscribedRooms {
    state: Mutex<SubscribedRoomsState>,
    notify: Arc<Notify>,
}

struct SubscribedRoomsState {
    refs: HashMap<OwnedRoomId, usize>,
}

impl SubscribedRooms {
    pub fn new() -> Arc<Self> {
        Arc::new(Self {
            state: Mutex::new(SubscribedRoomsState { refs: HashMap::new() }),
            notify: Arc::new(Notify::new()),
        })
    }

    /// Increment the refcount for `room_id`. Wakes the reconciler if the
    /// effective set changed (i.e. the room went from absent to present).
    pub fn subscribe(&self, room_id: OwnedRoomId) {
        let became_active = {
            let mut guard = self.state.lock().expect("poisoned subscribed-rooms mutex");
            let entry = guard.refs.entry(room_id).or_insert(0);
            let was_zero = *entry == 0;
            *entry += 1;
            was_zero
        };
        if became_active {
            self.notify.notify_one();
        }
    }

    /// Decrement the refcount for `room_id`. Removes the entry when the
    /// count drops to zero, and wakes the reconciler in that case.
    pub fn unsubscribe(&self, room_id: &OwnedRoomId) {
        let became_inactive = {
            let mut guard = self.state.lock().expect("poisoned subscribed-rooms mutex");
            match guard.refs.get_mut(room_id) {
                Some(count) if *count > 1 => {
                    *count -= 1;
                    false
                }
                Some(_) => {
                    guard.refs.remove(room_id);
                    true
                }
                None => false,
            }
        };
        if became_inactive {
            self.notify.notify_one();
        }
    }

    fn snapshot(&self) -> HashSet<OwnedRoomId> {
        self.state
            .lock()
            .expect("poisoned subscribed-rooms mutex")
            .refs
            .keys()
            .cloned()
            .collect()
    }

    fn notify(&self) -> Arc<Notify> {
        Arc::clone(&self.notify)
    }
}

/// Run the reconciler loop. Intended to be spawned on the sync loop's
/// tokio runtime at room-list start-up; exits when `stop_requested` is set.
///
/// The reconciler:
/// - waits on `subscribed.notify()` for set changes,
/// - debounces by `SUBSCRIPTION_DEBOUNCE`,
/// - on change, calls `room_list_service.subscribe_to_rooms(&union)` exactly
///   once with the new set,
/// - maintains per-room `pinned_event_ids_stream` forwarders for UI live
///   updates.
pub async fn run_reconciler(
    handle_id: u64,
    client: Client,
    room_list_service: Arc<RoomListService>,
    subscribed: Arc<SubscribedRooms>,
    stop_requested: Arc<std::sync::atomic::AtomicBool>,
) {
    let notify = subscribed.notify();
    let mut last_submitted: HashSet<OwnedRoomId> = HashSet::new();
    let mut pinned_forwarders: HashMap<OwnedRoomId, PinnedForwarder> = HashMap::new();

    tracing::info!(handle_id, "Started sliding-sync room-subscription reconciler");

    while !stop_requested.load(std::sync::atomic::Ordering::Relaxed) {
        // Block until a subscription change is signalled, or bail out on stop.
        tokio::select! {
            _ = notify.notified() => {}
            _ = tokio::time::sleep(Duration::from_millis(250)) => {
                // Periodic wake so stop_requested is observed without
                // requiring a final notify. No reconcile work if the set
                // hasn't changed.
                continue;
            }
        }

        // Debounce burst changes.
        tokio::time::sleep(SUBSCRIPTION_DEBOUNCE).await;

        if stop_requested.load(std::sync::atomic::Ordering::Relaxed) {
            break;
        }

        let desired = subscribed.snapshot();

        // Start forwarders for newly-added rooms.
        for room_id in desired.iter() {
            if pinned_forwarders.contains_key(room_id) {
                continue;
            }
            let forwarder = PinnedForwarder::start(handle_id, client.clone(), room_id.clone());
            pinned_forwarders.insert(room_id.clone(), forwarder);
        }

        // Stop forwarders for rooms that left the set.
        pinned_forwarders.retain(|room_id, forwarder| {
            if desired.contains(room_id) {
                true
            } else {
                forwarder.abort();
                false
            }
        });

        // Only call the SDK when the effective set has changed, to avoid
        // the unnecessary per-room `mark_members_missing()` the SDK does on
        // every subscribe call (see module docs).
        if desired != last_submitted {
            let refs: Vec<&matrix_sdk::ruma::RoomId> =
                desired.iter().map(|id| id.as_ref()).collect();
            room_list_service.subscribe_to_rooms(&refs).await;

            tracing::debug!(
                handle_id,
                room_count = desired.len(),
                "Applied sliding-sync room subscriptions"
            );

            last_submitted = desired;
        }
    }

    // Abort any remaining forwarders on shutdown.
    for (_, forwarder) in pinned_forwarders.drain() {
        forwarder.abort();
    }

    tracing::info!(handle_id, "Stopped sliding-sync room-subscription reconciler");
}

/// Per-room forwarder that emits `matrix_notify_room_pinned_events_changed`
/// to the C++ side whenever the state-store's pinned-events list changes.
///
/// Calling `abort()` (or dropping the `AbortHandle`-wrapper) stops the
/// forwarder.
struct PinnedForwarder {
    abort: tokio::task::AbortHandle,
}

impl PinnedForwarder {
    fn start(handle_id: u64, client: Client, room_id: OwnedRoomId) -> Self {
        let task = tokio::spawn(async move {
            // The room may not be known to the client the very first time
            // we subscribe — the sliding-sync response that populates it
            // hasn't arrived yet. Wait for it to appear rather than
            // dropping the forwarder on the floor.
            let room = loop {
                if let Some(r) = client.get_room(&room_id) {
                    break r;
                }
                tokio::time::sleep(FORWARDER_ROOM_WAIT).await;
            };

            // Seed the UI with whatever the state store currently has. On a
            // first subscription the value may be `None` because the
            // sliding-sync response carrying `m.room.pinned_events` as
            // required_state hasn't landed yet — in that case we emit an
            // empty list and wait for the stream below. The gap is one
            // sync round (sub-second on a healthy server, since
            // `subscribe_to_rooms` cancels the in-flight request).
            //
            // We deliberately do NOT fall back to `/state` here: it
            // produces an unavoidable `matrix_sdk::http_client: Error
            // while sending request ... status=404` log for every room
            // that has no pinned events — of which the vast majority of a
            // normal user's rooms qualify — drowning out real HTTP errors.
            let mut last_emitted = room.pinned_event_ids().unwrap_or_default();
            emit(handle_id, room.room_id(), &last_emitted);

            let mut stream = Box::pin(room.pinned_event_ids_stream());
            while let Some(next) = stream.next().await {
                if next == last_emitted {
                    // The stream fires on every RoomInfo change; skip the
                    // ones where the pinned list itself didn't move.
                    continue;
                }
                emit(handle_id, room.room_id(), &next);
                last_emitted = next;
            }
        });
        Self { abort: task.abort_handle() }
    }

    fn abort(&self) {
        self.abort.abort();
    }
}

fn emit(
    handle_id: u64,
    room_id: &matrix_sdk::ruma::RoomId,
    ids: &[matrix_sdk::ruma::OwnedEventId],
) {
    let event_ids: Vec<String> = ids.iter().map(ToString::to_string).collect();
    crate::ffi::matrix_notify_room_pinned_events_changed(
        handle_id,
        room_id.as_str(),
        event_ids,
    );
}

/// Entry-point called from FFI to mark a room as in-use by some UI surface.
pub fn subscribe_room(handle_id: u64, room_id: &str) -> Result<(), String> {
    let parsed = matrix_sdk::ruma::RoomId::parse(room_id)
        .map_err(|e| format!("invalid room id '{room_id}': {e}"))?;
    let subscribed = super::subscribed_rooms_for_handle(handle_id)?;
    subscribed.subscribe(parsed);
    Ok(())
}

/// Entry-point called from FFI to release a room reference.
pub fn unsubscribe_room(handle_id: u64, room_id: &str) -> Result<(), String> {
    let parsed = matrix_sdk::ruma::RoomId::parse(room_id)
        .map_err(|e| format!("invalid room id '{room_id}': {e}"))?;
    let subscribed = super::subscribed_rooms_for_handle(handle_id)?;
    subscribed.unsubscribe(&parsed);
    Ok(())
}
